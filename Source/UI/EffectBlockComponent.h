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
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

    void setSelected (bool shouldBeSelected);

    /** Which RoutingGraph node this block is the view of. The graph -- not
        this block's position, and not its index in any array -- is what
        decides what feeds what, so moving a block around changes only
        where it's drawn (its node's lane/column), never the signal path.
        Replaced the old gridSlot (a position in a strictly ordered grid)
        when routing became a real node/port/connection graph, per user
        request 2026-09-11 ("a ordem deve surgir naturalmente das
        conexões"). See MainComponent::addEffect()/handleBlockDragEnded()
        and RoutingGraph.h. */
    int nodeId = -1;

    /** A plain click (no meaningful drag) selects the block for editing. */
    std::function<void()> onClicked;

    /** Fires once, on mouse-up, only if this block was actually dragged --
        the block's current (mid-drag) x position is where it was dropped;
        the caller decides what index that maps to and reorders. */
    std::function<void (EffectBlockComponent&)> onDragEnded;

    EffectProcessor& processor;

private:
    bool selected = false;
    bool isDragging = false;
    juce::ComponentDragger dragger;
};

} // namespace pedaleira
