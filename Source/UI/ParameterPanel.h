#pragma once

#include "../Effects/EffectProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace pedaleira
{

/**
    Generic parameter editor: whatever processor is selected, this walks
    its getParameters() group and builds one slider per
    juce::AudioParameterFloat, live. No per-effect UI code anywhere --
    every Phase 1 pedal's state is fully described by float parameters
    (see EffectProcessor's default getState()/setState()), so a generic
    editor covers all of them for free, including ones added later.
*/
class ParameterPanel : public juce::Component
{
public:
    ParameterPanel();

    void setProcessor (EffectProcessor* processorToEdit);
    void refresh(); // call periodically -- picks up status text changes (e.g. after a model load)

    void resized() override;
    void paint (juce::Graphics& g) override;

    std::function<void (EffectProcessor*)> onRemoveRequested;

private:
    void rebuildForCurrentProcessor();
    void chooseAndLoadModelFile();

    EffectProcessor* current = nullptr;

    juce::Label titleLabel, statusLabel;
    juce::ToggleButton bypassToggle { "Bypassed" };
    juce::TextButton loadModelButton { "Load .nam file..." };
    juce::TextButton removeButton { "Remove" };

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
