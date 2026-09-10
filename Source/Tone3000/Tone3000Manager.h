#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace pedaleira
{

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

    Login itself is NOT done here: this class only builds the authorize URL
    and validates/exchanges the code once the UI hands it back (see
    beginLogin()/completeLogin()). There's no system-browser launch and no
    local HTTP listener -- the UI shows the authorize page in an embedded
    WebBrowserComponent (Source/UI/OAuthLoginDialog) and intercepts
    navigation to the redirect URI itself. That's not a PC-only shortcut:
    it's the only login flow that also works on the final target (a
    touchscreen device with no browser installed at all), and it happens to
    be TONE3000's own documented recommendation for native apps.
*/
class Tone3000Manager
{
public:
    Tone3000Manager();
    ~Tone3000Manager();

    void setClientId (const juce::String& newClientId);
    bool hasClientId() const { return clientId.isNotEmpty(); }
    juce::String getClientId() const { return clientId; }

    bool isLoggedIn() const;
    void logOut();

    /** Builds the authorize URL and remembers the PKCE verifier/state needed to
        complete login. Empty string if no client_id is set. The caller (UI) is
        responsible for showing this URL and detecting navigation to
        getRedirectUri() (see OAuthLoginDialog). */
    juce::String beginLogin();

    juce::String getRedirectUri() const { return redirectUri; }

    /** Call once the UI has intercepted a navigation to getRedirectUri() and
        pulled `code`/`state` out of its query string. Validates state against
        what beginLogin() generated, then exchanges the code for tokens.
        onComplete fires on the message thread. */
    void completeLogin (const juce::String& code, const juce::String& state,
                         std::function<void (bool success, juce::String error)> onComplete);

    /** gear/format values are exactly the strings TONE3000's API uses (verified
        against their docs, not guessed): gear one of "amp", "amp-cab", "pedal",
        "outboard", "cab", "space", "experimental"; format one of "nam", "ir",
        "aida-x", "aa-snapshot", "proteus". Only "nam" and "ir" are formats this
        app's engine can actually load (NeuralAmpModelerCore / juce::dsp::Convolution
        respectively) -- aida-x/aa-snapshot/proteus are other tools' formats. */
    struct Tone
    {
        int id = 0;
        juce::String title;
        juce::String author;
        juce::String license;
        juce::String gear;
        juce::String format;
    };

    /** Search public tones by name, e.g. "JCM800" or "Tube Screamer".
        gearFilter is one of the Gear strings above, or empty for no filter.
        onComplete fires on the message thread. */
    void searchTones (const juce::String& query, const juce::String& gearFilter,
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

    // Set by beginLogin(), consumed and cleared by completeLogin().
    juce::String pendingVerifier, pendingState;
    const juce::String redirectUri = "http://127.0.0.1:17872/callback"; // never actually connected to -- see class comment

    std::shared_ptr<std::atomic<bool>> aliveFlag = std::make_shared<std::atomic<bool>> (true);
};

} // namespace pedaleira
