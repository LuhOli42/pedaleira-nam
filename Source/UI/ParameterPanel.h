#pragma once

#include "../Effects/EffectProcessor.h"
#include "../Tone3000/Tone3000Manager.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
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

    Also offers TONE3000 search for whichever block is selected, via a
    popup (Tone3000SearchDialog) pre-filtered to that block's gear category
    -- a Neural Amp block only ever searches amps, a Cab block only ever
    searches cabs (see GearRouting.h::gearFilterForProcessorName).
    Tone3000Manager is injected and optional (setTone3000Manager()); with
    none set, the search button just never appears -- the integration
    stays optional end to end, per ARCHITECTURE.md.
*/
class ParameterPanel : public juce::Component
{
public:
    ParameterPanel();

    /** Base models directory (see MainComponent::getModelsDirectory) -- every
        block only ever browses/saves into its OWN category subfolder under
        here (amps/pedals/cabs/reverbs -- see GearRouting.h), so a Neural
        Pedal block never lists an amp capture and vice versa. */
    void setModelsDirectory (juce::File directory) { modelsDir = std::move (directory); }

    void setTone3000Manager (Tone3000Manager& managerToUse) { tone3000 = &managerToUse; }

    void setProcessor (EffectProcessor* processorToEdit);
    void refresh(); // call periodically -- picks up status text changes (e.g. after a model load)

    void resized() override;
    void paint (juce::Graphics& g) override;

    std::function<void (EffectProcessor*)> onRemoveRequested;

    /** Wired by MainComponent to OverlayHost::pushOverlay/popOverlay -- see
        AGENT.md's UI/UX Design Philosophy. Both the installed-models list
        and the TONE3000 search go through these instead of opening a
        separate OS window. */
    std::function<void (std::unique_ptr<juce::Component>)> onPushOverlay;
    std::function<void()> onPopOverlay;

private:
    void rebuildForCurrentProcessor();
    void browseInstalledModels();
    void openTone3000Search();

    EffectProcessor* current = nullptr;
    juce::File modelsDir;
    Tone3000Manager* tone3000 = nullptr;

    juce::Label titleLabel, statusLabel;
    juce::ToggleButton bypassToggle { "Bypassed" };
    juce::TextButton browseInstalledButton { "Browse installed..." };
    juce::TextButton searchTone3000Button { "Search TONE3000..." };
    juce::TextButton removeButton { "Remove" };

    struct SliderRow
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        juce::AudioParameterFloat* param = nullptr;
    };
    std::vector<SliderRow> sliders;
};

} // namespace pedaleira
