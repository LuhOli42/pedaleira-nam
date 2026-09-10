#include "OAuthLoginDialog.h"

#include <juce_events/juce_events.h>

namespace pedaleira
{

OAuthLoginDialog::OAuthLoginDialog (const juce::String& authorizeUrl, const juce::String& redirectUriPrefix)
    : DocumentWindow ("TONE3000 Login", juce::Colours::black, DocumentWindow::closeButton)
    , redirectPrefix (redirectUriPrefix)
{
    setUsingNativeTitleBar (false);

    browser.onPageAboutToLoad = [this] (const juce::String& url) -> bool
    {
        if (! url.startsWith (redirectPrefix))
            return true;

        juce::StringPairArray params;
        const auto query = url.fromFirstOccurrenceOf ("?", false, false);

        for (const auto& pair : juce::StringArray::fromTokens (query, "&", ""))
        {
            if (pair.isEmpty())
                continue;

            params.set (juce::URL::removeEscapeChars (pair.upToFirstOccurrenceOf ("=", false, false)),
                        juce::URL::removeEscapeChars (pair.fromFirstOccurrenceOf ("=", false, false)));
        }

        // Deferred: we're inside the webview's own navigation-decision
        // callback here, not a normal message-loop turn. Let that unwind
        // first before the caller potentially deletes this window.
        if (onRedirectReached)
        {
            auto callback = onRedirectReached;
            juce::MessageManager::callAsync ([callback, params] { callback (params); });
        }

        return false; // cancel navigation -- we never actually load the redirect URI
    };

    setContentNonOwned (&browser, true);
    setResizable (true, false);
    centreWithSize (480, 640);
    setVisible (true);

    browser.goToURL (authorizeUrl);
}

void OAuthLoginDialog::closeButtonPressed()
{
    if (onCancelled)
        onCancelled();
}

} // namespace pedaleira
