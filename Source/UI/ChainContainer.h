#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace pedaleira
{

/** Draws the connector lines between chain blocks, underneath them (a plain
    Component's paint() runs before its children's, so this naturally sits
    behind the EffectBlockComponents it's the parent of). Purely visual --
    MainComponent still owns layout and pushes block bounds in here whenever
    the chain changes. */
class ChainContainer : public juce::Component
{
public:
    void setBlockBounds (std::vector<juce::Rectangle<float>> bounds);
    void paint (juce::Graphics& g) override;

private:
    std::vector<juce::Rectangle<float>> blockBounds;
};

/** The "+" tile at the end of the chain -- styled like a block (dashed
    outline) rather than a generic button, so it reads as "another slot",
    matching the reference UI's add-device tiles. */
class AddBlockButton : public juce::Button
{
public:
    AddBlockButton();
    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
};

} // namespace pedaleira
