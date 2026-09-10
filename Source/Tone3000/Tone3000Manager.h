#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace pedaleira
{

class LoopbackServer;

/**
    TONE3000 login (OAuth 2.0 + PKCE), tone search, and model download.

    Strictly a control-thread component: the audio thread never touches it,
    and per ARCHITECTURE.md the integration stays OPTIONAL -- the product
    works fully without it, loading .nam files from disk by hand.

    You need your own publishable client_id from tone3000.com -> Settings ->
    API Keys. There is no shared or demo client_id (verified against the
    official API docs and their example client repo), so this can't be
    hardcoded. Set it with setClientId(), or export TONE3000_CLIENT_ID.

    No client secret is used, and none is needed: PKCE exists precisely so
    that a public/native client like this never holds one. TONE3000's
    t3k_cs_... secret key is documented as server-only -- if you ever find
    yourself wanting to embed it here, that's a bug.

    Redirect is a loopback URI (http://127.0.0.1:<port>/callback); their
    docs state localhost origins are auto-allowed without registering a
    redirect URI up front.
*/
class Tone3000Manager
{
public:
    Tone3000Manager();
    ~Tone3000Manager();

    void setClientId (const juce::String& newClientId);
    bool hasClientId() const { return clientId.isNotEmpty(); }

    bool isLoggedIn() const;
    void logOut();

    /** Opens the system browser to authorize, then exchanges the code for tokens.
        onComplete fires on the message thread. */
    void beginLogin (std::function<void (bool success, juce::String error)> onComplete);

    struct Tone
    {
        int id = 0;
        juce::String title;
        juce::String author;
        juce::String license;
    };

    /** Search public tones by name, e.g. "JCM800" or "Tube Screamer".
        onComplete fires on the message thread. */
    void searchTones (const juce::String& query,
                       std::function<void (bool success, std::vector<Tone> results, juce::String error)> onComplete);

    /** Downloads a model file given the `model_url` returned by the API.
        onComplete fires on the message thread. */
    void downloadModel (const juce::String& modelUrl,
                         const juce::File& destination,
                         std::function<void (bool success, juce::String error)> onComplete);

    /** Lists a tone's models and downloads the first one to `destination`.
        onComplete fires on the message thread. */
    void downloadFirstModelForTone (int toneId,
                                     const juce::File& destination,
                                     std::function<void (bool success, juce::String error)> onComplete);

private:
    struct HttpResult
    {
        bool ok = false;
        int statusCode = 0;
        juce::String body;
    };

    HttpResult httpGet (const juce::String& url, bool withAuth) const;
    HttpResult httpPostForm (const juce::String& url, const juce::String& formBody) const;

    /** Streams an authenticated GET straight to disk -- model files are opaque bytes,
        not text, so they must never round-trip through a juce::String. */
    bool httpDownloadToFile (const juce::String& url, const juce::File& destination, juce::String& error) const;

    juce::File getAuthFile() const;
    void loadPersistedAuth();
    void savePersistedAuth() const;

    void exchangeCodeForTokens (const juce::String& code,
                                 const juce::String& verifier,
                                 const juce::String& redirectUri,
                                 std::function<void (bool, juce::String)> onComplete);

    /** Runs work on a detached background thread, then delivers the result on the message thread.
        The alive flag means a callback that outlives this object simply does nothing. */
    template <typename Work>
    void runInBackground (Work&& work) const
    {
        std::thread ([work = std::forward<Work> (work), alive = aliveFlag]() mutable
        {
            work (alive);
        }).detach();
    }

    juce::String clientId;
    juce::String accessToken, refreshToken;
    std::unique_ptr<LoopbackServer> loginServer;

    std::shared_ptr<std::atomic<bool>> aliveFlag = std::make_shared<std::atomic<bool>> (true);
};

} // namespace pedaleira
