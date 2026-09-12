#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace openguitarmultifx
{

/** Draws the connector lines between chain blocks, underneath them (a plain
    Component's paint() runs before its children's, so this naturally sits
    behind the EffectBlockComponents it's the parent of). Purely visual --
    MainComponent still owns layout and pushes block bounds in here whenever
    the chain changes.

    The whole grid (every row/col up to maxVisibleRows x columns, not just
    cells with a real block) is hoverable: moving the mouse anywhere over it
    reveals a single "+" exactly under the cursor's cell instead of a
    permanent dashed-box tile sitting there all the time -- per user
    request 2026-09-10 ("tire todo esses + com caixinha quadrada... quando
    eu passar o mouse em cima da linha, vai aparecer esse +", "teria q ter
    um grid pra podermos adicionar esse + aonde quisermos"). Clicking an
    EMPTY cell fires onSlotClicked with exactly that cell -- MainComponent
    places the new block there directly (EffectBlockComponent::gridSlot),
    it does NOT get clamped/forced into sequential order any more (that was
    the bug fixed the same day: "ta adicionando ainda sequencialmente em
    vez daonde eu clico"). An occupied cell is unreachable here in the
    first place -- the real block Component sitting on top of it
    intercepts the hover/click before this container ever sees it. */
class ChainContainer : public juce::Component
{
public:
    void setBlockBounds (std::vector<juce::Rectangle<float>> bounds);

    /** The chain wraps into rows instead of scrolling sideways -- this is
        what lets both paint() (one signal line per row) and the hover/click
        handling below agree with MainComponent::layoutChain()'s own
        row/col math. Column and row spacing are independent: columns stay
        a fixed gap so 8 square tiles fit a row, rows get whatever's left
        of the available height distributed between them -- see
        MainComponent::resized(). */
    void setRowMetrics (int columnsIn, int blockWidthIn, int blockHeightIn, int colGapIn, int rowGapIn);

    /** How many rows the grid spans in total (hoverable/clickable even well
        past whatever's actually in use) -- see the class doc comment. */
    void setMaxVisibleRows (int rows) { maxVisibleRows = juce::jmax (1, rows); }

    /** The Y positions (this container's own coordinates) where a row-to-row
        connector crosses the grid horizontally. MainComponent computes these
        from the per-row routing and draws the two gutter stubs either side;
        this container draws the crossing itself because the gutters are
        OUTSIDE the scrolling viewport while this part has to be inside it.
        Empty when no row feeds another. */
    void setCrossings (std::vector<int> crossingYs);

    /** How many rows currently have at least one real block in them --
        MainComponent computes this from every block's gridSlot (which can
        now be sparse, so "how many rows are used" is no longer simply
        derivable from the block COUNT the way it used to be). Rows below
        this get the plain, unconnected placeholder line treatment in
        paint() instead of the "real" signal-path line + connecting seam. */
    void setUsedRows (int rows) { usedRows = juce::jlimit (1, maxVisibleRows, rows); }

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

    /** Fires with exactly the grid cell (row*columns+col) that was clicked
        -- see the class doc comment. */
    std::function<void (int gridSlot)> onSlotClicked;

private:
    std::vector<juce::Rectangle<float>> blockBounds;
    int columns = 1;
    int gridBlockWidth = 1, gridBlockHeight = 1, gridColGap = 0, gridRowGap = 0;
    int maxVisibleRows = 4;
    int usedRows = 1;
    std::vector<int> crossings;
    int hoveredIndex = -1; // -1 = not hovering the grid at all -- the RAW cell under the cursor

    /** The exact grid cell (row*columns+col) under `pos`, anywhere across
        the full maxVisibleRows x columns grid. */
    int rawIndexForPosition (juce::Point<int> pos) const;
    juce::Rectangle<float> cellBounds (int index) const;
};

} // namespace openguitarmultifx
