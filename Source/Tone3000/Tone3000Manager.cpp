#include "Tone3000Manager.h"

#include "LoopbackServer.h"
#include "Pkce.h"

#include <juce_events/juce_events.h> // MessageManager::callAsync

namespace pedaleira
{

namespace
{
    const juce::String apiBase = "https://www.tone3000.com/api/v1";
    constexpr int loopbackPort = 17872; // arbitrary high port for the OAuth redirect
    constexpr int httpTimeoutMs = 15000;
}

Tone3000Manager::Tone3000Manager()
{
    clientId = juce::SystemStats::getEnvironmentVariable ("TONE3000_CLIENT_ID", {}).trim();
    loadPersistedAuth();
}

Tone3000Manager::~Tone3000Manager()
{
    aliveFlag->store (false);
    loginServer.reset();
}

void Tone3000Manager::setClientId (const juce::String& newClientId)
{
    clientId = newClientId.trim();
    savePersistedAuth();
}

bool Tone3000Manager::isLoggedIn() const
{
    return accessToken.isNotEmpty();
}

void Tone3000Manager::logOut()
{
    accessToken.clear();
    refreshToken.clear();
    savePersistedAuth();
}

juce::File Tone3000Manager::getAuthFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("PedaleiraNAM")
               .getChildFile ("tone3000_auth.json");
}

void Tone3000Manager::loadPersistedAuth()
{
    const auto file = getAuthFile();

    if (! file.existsAsFile())
        return;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (auto* obj = parsed.getDynamicObject())
    {
        if (clientId.isEmpty())
            clientId = obj->getProperty ("client_id").toString();

        accessToken = obj->getProperty ("access_token").toString();
        refreshToken = obj->getProperty ("refresh_token").toString();
    }
}

void Tone3000Manager::savePersistedAuth() const
{
    // Tokens land in a plain file under the user's app-data dir. Good enough
    // for a dev build on a single-user machine; a shipping product should
    // move these into the OS keychain / secret service instead.
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("client_id", clientId);
    obj->setProperty ("access_token", accessToken);
    obj->setProperty ("refresh_token", refreshToken);

    const auto file = getAuthFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (juce::var (obj)));
}

Tone3000Manager::HttpResult Tone3000Manager::httpGet (const juce::String& url, bool withAuth) const
{
    HttpResult result;

    // InputStreamOptions has a const member, so it can't be reassigned --
    // every option has to be chained in one expression.
    const juce::String headers = (withAuth && accessToken.isNotEmpty())
                                      ? "Authorization: Bearer " + accessToken
                                      : juce::String();

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs (httpTimeoutMs)
                             .withStatusCode (&result.statusCode)
                             .withExtraHeaders (headers);

    if (auto stream = juce::URL (url).createInputStream (options))
    {
        result.body = stream->readEntireStreamAsString();
        result.ok = result.statusCode >= 200 && result.statusCode < 300;
    }

    return result;
}

Tone3000Manager::HttpResult Tone3000Manager::httpPostForm (const juce::String& url,
                                                            const juce::String& formBody) const
{
    HttpResult result;

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                             .withConnectionTimeoutMs (httpTimeoutMs)
                             .withStatusCode (&result.statusCode)
                             .withHttpRequestCmd ("POST")
                             .withExtraHeaders ("Content-Type: application/x-www-form-urlencoded");

    if (auto stream = juce::URL (url).withPOSTData (formBody).createInputStream (options))
    {
        result.body = stream->readEntireStreamAsString();
        result.ok = result.statusCode >= 200 && result.statusCode < 300;
    }

    return result;
}

bool Tone3000Manager::httpDownloadToFile (const juce::String& url,
                                           const juce::File& destination,
                                           juce::String& error) const
{
    int statusCode = 0;

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs (httpTimeoutMs)
                             .withStatusCode (&statusCode)
                             .withExtraHeaders (accessToken.isNotEmpty()
                                                     ? "Authorization: Bearer " + accessToken
                                                     : juce::String());

    auto stream = juce::URL (url).createInputStream (options);

    if (stream == nullptr)
    {
        error = "Could not open a connection to " + url;
        return false;
    }

    if (statusCode < 200 || statusCode >= 300)
    {
        error = "Download failed (HTTP " + juce::String (statusCode) + ").";
        return false;
    }

    destination.getParentDirectory().createDirectory();
    destination.deleteFile();

    juce::FileOutputStream out (destination);

    if (out.failedToOpen())
    {
        error = "Could not write " + destination.getFullPathName();
        return false;
    }

    out.writeFromInputStream (*stream, -1);
    out.flush();
    return true;
}

void Tone3000Manager::beginLogin (std::function<void (bool, juce::String)> onComplete)
{
    if (clientId.isEmpty())
    {
        onComplete (false, "No TONE3000 client_id set. Create a publishable key at "
                            "tone3000.com -> Settings -> API Keys, then set it here "
                            "(or export TONE3000_CLIENT_ID).");
        return;
    }

    const auto verifier = pkce::randomUrlSafeString();
    const auto challenge = pkce::codeChallengeFromVerifier (verifier);
    const auto state = pkce::randomUrlSafeString (16);
    const auto redirectUri = "http://127.0.0.1:" + juce::String (loopbackPort) + "/callback";

    loginServer = std::make_unique<LoopbackServer>();

    const bool started = loginServer->start (loopbackPort,
        [this, verifier, state, redirectUri, onComplete] (const juce::StringPairArray& params)
        {
            // Still on the LoopbackServer's own thread here.
            const auto code = params["code"];
            const auto returnedState = params["state"];
            const auto oauthError = params["error"];

            if (oauthError.isNotEmpty())
            {
                juce::MessageManager::callAsync ([onComplete, oauthError] { onComplete (false, "Authorization denied: " + oauthError); });
                return;
            }

            if (returnedState != state)
            {
                // Mismatched state means the redirect didn't come from the
                // request we started -- treat it as hostile and drop it.
                juce::MessageManager::callAsync ([onComplete] { onComplete (false, "OAuth state mismatch -- login aborted."); });
                return;
            }

            if (code.isEmpty())
            {
                juce::MessageManager::callAsync ([onComplete] { onComplete (false, "No authorization code in the redirect."); });
                return;
            }

            exchangeCodeForTokens (code, verifier, redirectUri, onComplete);
        });

    if (! started)
    {
        loginServer.reset();
        onComplete (false, "Could not listen on 127.0.0.1:" + juce::String (loopbackPort)
                             + " for the OAuth redirect (port already in use?).");
        return;
    }

    juce::URL authorize (apiBase + "/oauth/authorize");
    authorize = authorize.withParameter ("client_id", clientId)
                          .withParameter ("redirect_uri", redirectUri)
                          .withParameter ("response_type", "code")
                          .withParameter ("code_challenge", challenge)
                          .withParameter ("code_challenge_method", "S256")
                          .withParameter ("state", state);

    if (! authorize.launchInDefaultBrowser())
    {
        loginServer.reset();
        onComplete (false, "Could not open the browser for authorization.");
    }
}

void Tone3000Manager::exchangeCodeForTokens (const juce::String& code,
                                              const juce::String& verifier,
                                              const juce::String& redirectUri,
                                              std::function<void (bool, juce::String)> onComplete)
{
    const auto form = "grant_type=authorization_code"
                       "&code=" + juce::URL::addEscapeChars (code, true)
                       + "&code_verifier=" + juce::URL::addEscapeChars (verifier, true)
                       + "&redirect_uri=" + juce::URL::addEscapeChars (redirectUri, true)
                       + "&client_id=" + juce::URL::addEscapeChars (clientId, true);

    runInBackground ([this, form, onComplete] (std::shared_ptr<std::atomic<bool>> alive)
    {
        const auto result = httpPostForm (apiBase + "/oauth/token", form);

        if (! alive->load())
            return;

        juce::String error;
        juce::String newAccess, newRefresh;

        if (! result.ok)
        {
            error = "Token exchange failed (HTTP " + juce::String (result.statusCode) + ").";
        }
        else
        {
            const auto parsed = juce::JSON::parse (result.body);

            if (auto* obj = parsed.getDynamicObject())
            {
                newAccess = obj->getProperty ("access_token").toString();
                newRefresh = obj->getProperty ("refresh_token").toString();
            }

            if (newAccess.isEmpty())
                error = "Token response had no access_token.";
        }

        juce::MessageManager::callAsync ([this, alive, onComplete, error, newAccess, newRefresh]
        {
            if (! alive->load())
                return;

            loginServer.reset();

            if (error.isNotEmpty())
            {
                onComplete (false, error);
                return;
            }

            accessToken = newAccess;
            refreshToken = newRefresh;
            savePersistedAuth();
            onComplete (true, {});
        });
    });
}

void Tone3000Manager::searchTones (const juce::String& query,
                                    std::function<void (bool, std::vector<Tone>, juce::String)> onComplete)
{
    if (! isLoggedIn())
    {
        onComplete (false, {}, "Not logged in to TONE3000.");
        return;
    }

    const auto url = apiBase + "/tones/search?query=" + juce::URL::addEscapeChars (query, true);

    runInBackground ([this, url, onComplete] (std::shared_ptr<std::atomic<bool>> alive)
    {
        const auto result = httpGet (url, true);

        if (! alive->load())
            return;

        std::vector<Tone> tones;
        juce::String error;

        if (! result.ok)
        {
            error = "Search failed (HTTP " + juce::String (result.statusCode) + ").";
        }
        else
        {
            const auto parsed = juce::JSON::parse (result.body);

            // Paginated envelope: { data: [...], page, page_size, total, total_pages }
            const auto data = parsed.getProperty ("data", juce::var());

            if (auto* array = data.getArray())
            {
                for (const auto& item : *array)
                {
                    Tone tone;
                    tone.id = (int) item.getProperty ("id", 0);
                    tone.title = item.getProperty ("title", {}).toString();
                    tone.license = item.getProperty ("license", {}).toString();

                    const auto user = item.getProperty ("user", juce::var());
                    tone.author = user.getProperty ("username", {}).toString();

                    tones.push_back (tone);
                }
            }
            else
            {
                error = "Unexpected search response shape.";
            }
        }

        juce::MessageManager::callAsync ([alive, onComplete, tones, error]
        {
            if (alive->load())
                onComplete (error.isEmpty(), tones, error);
        });
    });
}

void Tone3000Manager::downloadModel (const juce::String& modelUrl,
                                      const juce::File& destination,
                                      std::function<void (bool, juce::String)> onComplete)
{
    if (! isLoggedIn())
    {
        onComplete (false, "Not logged in to TONE3000.");
        return;
    }

    runInBackground ([this, modelUrl, destination, onComplete] (std::shared_ptr<std::atomic<bool>> alive)
    {
        // model_url is a pre-built download URL that still needs the bearer token.
        juce::String error;
        httpDownloadToFile (modelUrl, destination, error);

        if (! alive->load())
            return;

        juce::MessageManager::callAsync ([alive, onComplete, error]
        {
            if (alive->load())
                onComplete (error.isEmpty(), error);
        });
    });
}

void Tone3000Manager::downloadFirstModelForTone (int toneId,
                                                  const juce::File& destination,
                                                  std::function<void (bool, juce::String)> onComplete)
{
    if (! isLoggedIn())
    {
        onComplete (false, "Not logged in to TONE3000.");
        return;
    }

    // NOTE: the tone_id filter on /models is the one piece of this file not
    // verified against the live API (the docs confirm /models lists models
    // and that each carries model_url + tone_id, but not the exact filter
    // param). If listing comes back empty for a tone that clearly has
    // models, this query string is the first thing to check.
    const auto url = apiBase + "/models?tone_id=" + juce::String (toneId);

    runInBackground ([this, url, destination, onComplete] (std::shared_ptr<std::atomic<bool>> alive)
    {
        const auto listResult = httpGet (url, true);

        if (! alive->load())
            return;

        juce::String modelUrl, error;

        if (! listResult.ok)
        {
            error = "Model list failed (HTTP " + juce::String (listResult.statusCode) + ").";
        }
        else
        {
            const auto parsed = juce::JSON::parse (listResult.body);
            const auto data = parsed.getProperty ("data", juce::var());

            if (auto* array = data.getArray(); array != nullptr && ! array->isEmpty())
                modelUrl = array->getReference (0).getProperty ("model_url", {}).toString();

            if (modelUrl.isEmpty())
                error = "No downloadable model found for this tone.";
        }

        if (error.isNotEmpty())
        {
            juce::MessageManager::callAsync ([alive, onComplete, error]
            {
                if (alive->load())
                    onComplete (false, error);
            });
            return;
        }

        juce::MessageManager::callAsync ([this, alive, modelUrl, destination, onComplete]
        {
            if (alive->load())
                downloadModel (modelUrl, destination, onComplete);
        });
    });
}

} // namespace pedaleira
