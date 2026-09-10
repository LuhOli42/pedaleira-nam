#include "Tone3000Panel.h"

namespace pedaleira
{

Tone3000Panel::Tone3000Panel (Tone3000Manager& managerToUse)
    : manager (managerToUse)
{
    addAndMakeVisible (clientIdLabel);

    addAndMakeVisible (clientIdField);
    clientIdField.setTextToShowWhenEmpty ("t3k_pub_...", juce::Colours::grey);
    clientIdField.setText (manager.getClientId(), juce::dontSendNotification);

    addAndMakeVisible (saveClientIdButton);
    saveClientIdButton.onClick = [this]
    {
        manager.setClientId (clientIdField.getText());
        refreshLoginState();
    };

    addAndMakeVisible (loginButton);
    loginButton.onClick = [this] { doLogin(); };

    addAndMakeVisible (logoutButton);
    logoutButton.onClick = [this] { manager.logOut(); refreshLoginState(); };

    addAndMakeVisible (statusLabel);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { if (onRequestClose) onRequestClose(); };

    refreshLoginState();
    setSize (420, 210);
}

void Tone3000Panel::refreshLoginState()
{
    const bool loggedIn = manager.isLoggedIn();

    loginButton.setVisible (! loggedIn);
    logoutButton.setVisible (loggedIn);

    if (! manager.hasClientId())
        statusLabel.setText ("Paste your publishable key from tone3000.com -> Settings -> API Keys",
                              juce::dontSendNotification);
    else
        statusLabel.setText (loggedIn ? "Logged in. Search for gear from inside any block that takes a model."
                                       : "Key saved. Log in to search and download.",
                              juce::dontSendNotification);
}

void Tone3000Panel::doLogin()
{
    const auto authorizeUrl = manager.beginLogin();

    if (authorizeUrl.isEmpty())
    {
        statusLabel.setText ("Set your client_id first.", juce::dontSendNotification);
        return;
    }

    // Embedded in-app browser, not the system one -- see OAuthLoginDialog
    // and Tone3000Manager's class comment for why: this is the only login
    // flow that also works on the final touchscreen target.
    loginDialog = std::make_unique<OAuthLoginDialog> (authorizeUrl, manager.getRedirectUri());

    loginDialog->onRedirectReached = [this] (const juce::StringPairArray& params)
    {
        const auto code = params["code"];
        const auto state = params["state"];
        const auto oauthError = params["error"];

        loginDialog.reset();

        if (oauthError.isNotEmpty())
        {
            statusLabel.setText ("Authorization denied: " + oauthError, juce::dontSendNotification);
            return;
        }

        statusLabel.setText ("Completing login...", juce::dontSendNotification);

        manager.completeLogin (code, state, [this] (bool success, juce::String error)
        {
            statusLabel.setText (success ? "Logged in." : error, juce::dontSendNotification);
            refreshLoginState();
        });
    };

    loginDialog->onCancelled = [this]
    {
        loginDialog.reset();
        statusLabel.setText ("Login cancelled.", juce::dontSendNotification);
    };
}

void Tone3000Panel::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto keyRow = area.removeFromTop (28);
    clientIdLabel.setBounds (keyRow.removeFromLeft (70));
    saveClientIdButton.setBounds (keyRow.removeFromRight (90));
    clientIdField.setBounds (keyRow.reduced (4, 0));

    area.removeFromTop (8);

    auto loginRow = area.removeFromTop (28);
    loginButton.setBounds (loginRow.removeFromLeft (110));
    logoutButton.setBounds (loginRow.getX() - 110, loginRow.getY(), 110, loginRow.getHeight());

    area.removeFromTop (10);
    statusLabel.setBounds (area.removeFromTop (44));

    closeButton.setBounds (area.removeFromBottom (30).removeFromRight (90));
}

void Tone3000Panel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141414));
}

} // namespace pedaleira
