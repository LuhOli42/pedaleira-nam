#pragma once

#include "../Effects/EffectProcessor.h"
#include "../Tone3000/Tone3000Manager.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace pedaleira
{

/**
    Generic parameter editor: whatever processor is selected, this walks
    its getParameters() group and builds one knob per
    juce::AudioParameterFloat, live. No per-effect UI code anywhere --
    every Phase 1 pedal's state is fully described by float parameters
    (see EffectProcessor's default getState()/setState()), so a generic
    editor covers all of them for free, including ones added later.

    Also owns the TONE3000 search for whichever block is selected -- not a
    separate generic dialog, but contextual: a Neural Amp block only ever
    searches amps, a Cab block only ever searches cabs (see
    GearRouting.h::gearFilterForProcessorName). Tone3000Manager is injected
    and optional (setTone3000Manager()); with none set, the search section
    just never appears -- the integration stays optional end to end, per
    ARCHITECTURE.md.
*/
class ParameterPanel : public juce::Component,
                        private juce::ListBoxModel
{
public:
    ParameterPanel();

    /** Base models directory (see MainComponent::getModelsDirectory) -- lets the
        "Load file..." picker, and downloaded TONE3000 files, land in the right
        category subfolder. */
    void setModelsDirectory (juce::File directory) { modelsDir = std::move (directory); }

    void setTone3000Manager (Tone3000Manager& managerToUse) { tone3000 = &managerToUse; }

    void setProcessor (EffectProcessor* processorToEdit);
    void refresh(); // call periodically -- picks up status text changes (e.g. after a model load)

    void resized() override;
    void paint (juce::Graphics& g) override;

    std::function<void (EffectProcessor*)> onRemoveRequested;

private:
    void rebuildForCurrentProcessor();
    void chooseAndLoadModelFile();
    void doTone3000Search();
    void doTone3000DownloadSelected();

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;

    EffectProcessor* current = nullptr;
    juce::File modelsDir;
    Tone3000Manager* tone3000 = nullptr;

    juce::Label titleLabel, statusLabel;
    juce::ToggleButton bypassToggle { "Bypassed" };
    juce::TextButton loadModelButton { "Load file from disk..." };
    juce::TextButton removeButton { "Remove" };

    juce::TextEditor tone3000SearchField;
    juce::TextButton tone3000SearchButton { "Search TONE3000" };
    juce::ListBox tone3000ResultsList { "tone3000results", this };
    juce::TextButton tone3000DownloadButton { "Download && load" };
    std::vector<Tone3000Manager::Tone> tone3000Results;

    struct SliderRow
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        juce::AudioParameterFloat* param = nullptr;
    };
    std::vector<SliderRow> sliders;

    std::unique_ptr<juce::FileChooser> fileChooser; // kept alive for the async picker
};

} // namespace pedaleira
