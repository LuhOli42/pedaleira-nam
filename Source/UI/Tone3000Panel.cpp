#include "Tone3000Panel.h"

namespace pedaleira
{

Tone3000Panel::Tone3000Panel (Tone3000Manager& managerToUse, juce::File modelsDirectory)
    : manager (managerToUse), modelsDir (std::move (modelsDirectory))
{
    addAndMakeVisible (clientIdLabel);

    addAndMakeVisible (clientIdField);
    clientIdField.setTextToShowWhenEmpty ("t3k_pub_...", juce::Colours::grey);

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

    addAndMakeVisible (searchButton);
    searchButton.onClick = [this] { doSearch(); };

    addAndMakeVisible (resultsList);
    resultsList.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1a1a1a));

    addAndMakeVisible (downloadButton);
    downloadButton.onClick = [this] { doDownloadSelected(); };

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
    statusLabel.setText ("Opening browser for authorization...", juce::dontSendNotification);

    manager.beginLogin ([this] (bool success, juce::String error)
    {
        statusLabel.setText (success ? "Logged in." : error, juce::dontSendNotification);
        refreshLoginState();
    });
}

void Tone3000Panel::doSearch()
{
    const auto query = searchField.getText().trim();

    if (query.isEmpty())
        return;

    statusLabel.setText ("Searching for \"" + query + "\"...", juce::dontSendNotification);

    manager.searchTones (query, [this] (bool success, std::vector<Tone3000Manager::Tone> found, juce::String error)
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
    const auto safeName = juce::File::createLegalFileName (tone.title.isEmpty() ? juce::String (tone.id) : tone.title);
    const auto destination = modelsDir.getChildFile (safeName + ".nam");

    statusLabel.setText ("Downloading \"" + tone.title + "\"...", juce::dontSendNotification);

    manager.downloadFirstModelForTone (tone.id, destination, [this, destination] (bool success, juce::String error)
    {
        if (! success)
        {
            statusLabel.setText (error, juce::dontSendNotification);
            return;
        }

        statusLabel.setText ("Saved to " + destination.getFullPathName(), juce::dontSendNotification);

        if (onModelDownloaded)
            onModelDownloaded (destination);
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
    g.drawText (tone.author + (tone.license.isEmpty() ? juce::String() : "  ·  " + tone.license),
                 8, height / 2, width - 16, height / 2, juce::Justification::centredLeft);
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
    searchField.setBounds (searchRow.reduced (0, 0).withTrimmedRight (6));

    area.removeFromTop (8);
    downloadButton.setBounds (area.removeFromBottom (30).removeFromRight (180));
    area.removeFromBottom (8);
    resultsList.setBounds (area);
}

void Tone3000Panel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141414));
}

} // namespace pedaleira
