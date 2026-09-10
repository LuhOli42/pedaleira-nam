#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace pedaleira
{

/**
    A fixed block at either end of the chain row for picking which physical
    device channel feeds the chain (input side) or which output(s) the
    chain reaches (output side) -- e.g. Quad Cortex's "In 1" / "Out 1/2".
    Styled like an EffectBlockComponent so it reads as part of the same
    row, but it isn't one: this is AudioEngine's device I/O routing, not a
    SignalGraph node, so it never goes through EffectRegistry.
*/
class IOSelectorBlock : public juce::Component
{
public:
    explicit IOSelectorBlock (juce::String labelText);

    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent&) override;

    /** Repopulates the popup menu and shows which one is current. */
    void setOptions (juce::StringArray optionNames, int currentIndex);

    /** Fires with the chosen option's index into the list passed to setOptions(). */
    std::function<void (int optionIndex)> onSelectionChanged;

private:
    juce::String label;
    juce::String selectionText { "Default" };
    juce::StringArray options;
};

} // namespace pedaleira
