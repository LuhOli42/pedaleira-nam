#include "Tone3000Panel.h"

#include "../Tone3000/GearRouting.h"

namespace pedaleira
{

namespace
{
    // (display label, TONE3000 API enum value) -- exact strings per GearRouting.h.
    const std::vector<std::pair<juce::String, juce::String>> gearFilterOptions = {
        { "All gear", {} },
        { "Amp", "amp" },
        { "Amp + Cab", "amp-cab" },
        { "Pedal", "pedal" },
        { "Outboard", "outboard" },
        { "Cabinet", "cab" },
        { "Space (reverb)", "space" },
        { "Experimental", "experimental" },
    };
}

Tone3000Panel::Tone3000Panel (Tone3000Manager& managerToUse, juce::File modelsDirectory)
    : manager (managerToUse), modelsDir (std::move (modelsDirectory))
{
    addAndMakeVisible (clientIdLabel);

    addAndMakeVisible (clientIdField);
    clientIdField.setTextToShowWhenEmpty ("t3k_pub_...", juce::Colours::grey);
    clientIdField.setText (manager.getClientId(), juce::dontSendNotification); // show what's already saved, don't make it look lost

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

    addAndMakeVisible (searchField);
    searchField.setTextToShowWhenEmpty ("JCM800, Plexi, Tube Screamer...", juce::Colours::grey);
    searchField.onReturnKey = [this] { doSearch(); };

    addAndMakeVisible (gearFilterCombo);
    for (int i = 0; i < (int) gearFilterOptions.size(); ++i)
        gearFilterCombo.addItem (gearFilterOptions[(size_t) i].first, i + 1);
    gearFilterCombo.setSelectedId (1, juce::dontSendNotification); // "All gear"

    addAndMakeVisible (searchButton);
    searchButton.onClick = [this] { doSearch(); };

    addAndMakeVisible (resultsList);
    resultsList.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1a1a1a));

    addAndMakeVisible (downloadButton);
    downloadButton.onClick = [this] { doDownloadSelected(); };

    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { if (onRequestClose) onRequestClose(); };

    refreshLoginState();
    setSize (620, 460);
}

void Tone3000Panel::refreshLoginState()
{
    const bool loggedIn = manager.isLoggedIn();

    loginButton.setVisible (! loggedIn);
    logoutButton.setVisible (loggedIn);
    searchButton.setEnabled (loggedIn);
    downloadButton.setEnabled (loggedIn);

    if (! manager.hasClientId())
        statusLabel.setText ("Paste your publishable key from tone3000.com -> Settings -> API Keys",
                              juce::dontSendNotification);
    else
        statusLabel.setText (loggedIn ? "Logged in." : "Key saved. Log in to search and download.",
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

void Tone3000Panel::doSearch()
{
    const auto query = searchField.getText().trim();

    if (query.isEmpty())
        return;

    const auto gearIndex = gearFilterCombo.getSelectedId() - 1;
    const auto gearFilter = (gearIndex >= 0 && gearIndex < (int) gearFilterOptions.size())
                                 ? gearFilterOptions[(size_t) gearIndex].second
                                 : juce::String();

    statusLabel.setText ("Searching for \"" + query + "\"...", juce::dontSendNotification);

    manager.searchTones (query, gearFilter, [this] (bool success, std::vector<Tone3000Manager::Tone> found, juce::String error)
    {
        if (! success)
        {
            statusLabel.setText (error, juce::dontSendNotification);
            return;
        }

        results = std::move (found);
        resultsList.updateContent();
        resultsList.deselectAllRows();
        statusLabel.setText (juce::String ((int) results.size()) + " result(s).", juce::dontSendNotification);
    });
}

void Tone3000Panel::doDownloadSelected()
{
    const int row = resultsList.getSelectedRow();

    if (row < 0 || row >= (int) results.size())
    {
        statusLabel.setText ("Select a tone first.", juce::dontSendNotification);
        return;
    }

    const auto& tone = results[(size_t) row];
    const auto route = tone3000routing::routeFor (tone.gear, tone.format);

    if (! route.supported)
    {
        statusLabel.setText ("\"" + tone.title + "\" is format \"" + tone.format
                                  + "\", which this engine can't load (only nam/ir are supported).",
                              juce::dontSendNotification);
        return;
    }

    // Saved separately by category (amps/pedals/cabs/reverbs/...) -- not
    // just a flat pile of files.
    const auto safeName = juce::File::createLegalFileName (tone.title.isEmpty() ? juce::String (tone.id) : tone.title);
    const auto destination = modelsDir.getChildFile (route.subfolder).getChildFile (safeName + route.fileExtension);

    statusLabel.setText ("Downloading \"" + tone.title + "\"...", juce::dontSendNotification);

    manager.downloadFirstModelForTone (tone.id, destination,
        [this, destination, gear = tone.gear, format = tone.format] (bool success, juce::String error)
    {
        if (! success)
        {
            statusLabel.setText (error, juce::dontSendNotification);
            return;
        }

        statusLabel.setText ("Saved to " + destination.getFullPathName(), juce::dontSendNotification);

        if (onModelDownloaded)
            onModelDownloaded (destination, gear, format);
    });
}

int Tone3000Panel::getNumRows()
{
    return (int) results.size();
}

void Tone3000Panel::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int) results.size())
        return;

    const auto& tone = results[(size_t) rowNumber];

    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff2d5c56));

    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    g.drawText (tone.title, 8, 0, width - 16, height / 2, juce::Justification::centredLeft);

    g.setColour (juce::Colours::lightgrey);
    g.setFont (11.0f);
    juce::String subtitle = tone.author;
    if (tone.gear.isNotEmpty())     subtitle += "  ·  " + tone.gear;
    if (tone.format.isNotEmpty())   subtitle += "  ·  " + tone.format;
    if (tone.license.isNotEmpty())  subtitle += "  ·  " + tone.license;
    g.drawText (subtitle, 8, height / 2, width - 16, height / 2, juce::Justification::centredLeft);
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

    area.removeFromTop (6);
    statusLabel.setBounds (area.removeFromTop (22));
    area.removeFromTop (10);

    auto searchRow = area.removeFromTop (28);
    searchButton.setBounds (searchRow.removeFromRight (90));
    searchRow.removeFromRight (6);
    gearFilterCombo.setBounds (searchRow.removeFromRight (140));
    searchRow.removeFromRight (6);
    searchField.setBounds (searchRow);

    area.removeFromTop (8);
    auto bottomRow = area.removeFromBottom (30);
    closeButton.setBounds (bottomRow.removeFromRight (90));
    bottomRow.removeFromRight (8);
    downloadButton.setBounds (bottomRow.removeFromRight (180));
    area.removeFromBottom (8);
    resultsList.setBounds (area);
}

void Tone3000Panel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141414));
}

} // namespace pedaleira
