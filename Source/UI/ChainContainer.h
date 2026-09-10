#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
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

    /** The chain wraps into rows instead of scrolling sideways -- this is
        what lets both paint() (one signal line per row) and mouseUp()
        (translating a click position into a flat insertion index) agree
        with MainComponent::layoutChain()'s own row/col math. */
    void setRowMetrics (int columnsIn, int blockWidthIn, int blockHeightIn, int gapIn);

    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& event) override;

    /** Fires with the insertion index a click resolved to -- i.e. a click
        that landed directly on the container (NOT on a block or the add
        tile, both of which are child components that intercept their own
        clicks first), translated from its 2D position through the current
        row/column grid. That's exactly "clicked on the line itself",
        including a gap between two existing blocks, not just the empty
        tail past the last one. */
    std::function<void (int insertIndex)> onSlotClicked;

private:
    std::vector<juce::Rectangle<float>> blockBounds;
    int columns = 1;
    int gridBlockWidth = 1, gridBlockHeight = 1, gridGap = 0;
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
