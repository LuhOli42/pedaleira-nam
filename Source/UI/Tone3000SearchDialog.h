#pragma once

#include "../Tone3000/Tone3000Manager.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace pedaleira
{

/**
    A focused popup for searching and downloading ONE category of TONE3000
    gear -- gearFilter is fixed at construction (see
    GearRouting.h::gearFilterForProcessorName), not a dropdown the user
    picks, because this dialog is always opened from a specific block
    ("Search TONE3000..." on a Neural Amp block only ever searches amps).
*/
class Tone3000SearchDialog : public juce::Component,
                              private juce::ListBoxModel
{
public:
    Tone3000SearchDialog (Tone3000Manager& managerToUse, juce::String gearFilterToUse, juce::File destinationFolderToUse);

    void resized() override;
    void paint (juce::Graphics& g) override;

    /** Fires once a result has been downloaded and is ready to load. */
    std::function<void (juce::File)> onFileReady;
    std::function<void()> onRequestClose;

private:
    void doSearch();
    void doDownloadSelected();

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;

    Tone3000Manager& manager;
    juce::String gearFilter;
    juce::File destinationFolder;

    juce::Label titleLabel;
    juce::TextEditor searchField;
    juce::ComboBox architectureBox;
    juce::TextButton searchButton { "Search" };
    juce::ListBox resultsList { "tone3000results", this };
    juce::TextButton downloadButton { "Download && load" };
    juce::TextButton closeButton { "Close" };
    juce::Label statusLabel;

    std::vector<Tone3000Manager::Tone> results;
};

} // namespace pedaleira
