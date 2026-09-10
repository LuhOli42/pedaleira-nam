#pragma once

#include "../Tone3000/Tone3000Manager.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace pedaleira
{

/**
    "Configure the connection" surface for TONE3000: paste your publishable
    client_id, log in through the browser, search tones, download a model.

    A downloaded model isn't loaded into the audio graph from here -- this
    panel just reports the file it wrote via onModelDownloaded, and
    MainComponent decides what to do with it (load into the selected NAM
    block). Keeps the network layer and the audio graph from knowing about
    each other at all.
*/
class Tone3000Panel : public juce::Component,
                       private juce::ListBoxModel
{
public:
    Tone3000Panel (Tone3000Manager& managerToUse, juce::File modelsDirectory);

    void resized() override;
    void paint (juce::Graphics& g) override;

    std::function<void (juce::File)> onModelDownloaded;

private:
    void refreshLoginState();
    void doLogin();
    void doSearch();
    void doDownloadSelected();

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;

    Tone3000Manager& manager;
    juce::File modelsDir;

    juce::Label clientIdLabel { {}, "client_id" };
    juce::TextEditor clientIdField;
    juce::TextButton saveClientIdButton { "Save key" };

    juce::TextButton loginButton { "Log in" };
    juce::TextButton logoutButton { "Log out" };
    juce::Label statusLabel;

    juce::TextEditor searchField;
    juce::TextButton searchButton { "Search" };
    juce::ListBox resultsList { "tones", this };
    juce::TextButton downloadButton { "Download selected" };

    std::vector<Tone3000Manager::Tone> results;
};

} // namespace pedaleira
