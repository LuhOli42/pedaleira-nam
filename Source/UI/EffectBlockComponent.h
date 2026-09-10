#pragma once

#include "../Effects/EffectProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace pedaleira
{

/**
    One tile in the horizontal signal chain (Quad Cortex's "Grid" is the
    reference point: a row of device blocks you tap to select and edit).
    Purely a view -- it never owns the processor, never touches the audio
    thread, and reads/writes only through the generic EffectProcessor
    contract (bypass, name, status) so it works for any registered effect
    type without knowing about it.
*/
class EffectBlockComponent : public juce::Component
{
public:
    explicit EffectBlockComponent (EffectProcessor& processorToShow);

    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& event) override;

    void setSelected (bool shouldBeSelected);

    std::function<void()> onClicked;

    EffectProcessor& processor;

private:
    bool selected = false;
};

} // namespace pedaleira
