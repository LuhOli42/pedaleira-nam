#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

namespace pedaleira
{

/**
    An OAuth authorize page rendered INSIDE the app (WebKitGTK on Linux, via
    juce::WebBrowserComponent), never a separate system browser process.

    This isn't a PC-only convenience: TONE3000's own docs recommend exactly
    this pattern for native apps ("open the authorization URL in an in-app
    browser"), and it's the only one that works on the final target
    (Radxa Cubie A7S touchscreen, no browser installed at all). PC and
    embedded share this same code path -- nothing to swap later.

    No loopback HTTP server either: navigation to the redirect URI is
    intercepted and cancelled before it actually happens
    (pageAboutToLoad), and the authorization code is read straight out of
    that URL's query string. Simpler and it needs no open port.
*/
class OAuthLoginDialog : public juce::DocumentWindow
{
public:
    OAuthLoginDialog (const juce::String& authorizeUrl, const juce::String& redirectUriPrefix);

    void closeButtonPressed() override;

    /** Fires once (on the message thread) when the browser is about to navigate
        to the redirect URI, with that URL's query parameters. */
    std::function<void (const juce::StringPairArray&)> onRedirectReached;

    /** Fires if the user closes the window without completing login. */
    std::function<void()> onCancelled;

private:
    class Browser : public juce::WebBrowserComponent
    {
    public:
        std::function<bool (const juce::String&)> onPageAboutToLoad;
        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return onPageAboutToLoad ? onPageAboutToLoad (newURL) : true;
        }
    };

    Browser browser;
    juce::String redirectPrefix;
};

} // namespace pedaleira
