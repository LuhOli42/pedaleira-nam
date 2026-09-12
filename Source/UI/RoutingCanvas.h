#pragma once

#include "RoutingGraph.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace pedaleira
{

/**
    The patchbay view of a RoutingGraph: four lane rails, a block per node,
    a port per input/output, and a cable per connection.

    The lanes are rails to organise blocks on, NOT four separate chains --
    a block's lane/column decides only where it's drawn. What feeds what is
    always the cables, so dragging a block to another lane never changes
    the audio, and two blocks sitting next to each other on the same lane
    are unconnected until a cable says otherwise.

    Everything this component draws is derived from the graph on every
    paint -- there is no second copy of the topology living in the view, so
    cables follow their blocks for free when a block moves. Effect blocks
    themselves are EffectBlockComponent children owned by MainComponent
    (kept, so icons/selection/bypass rendering stay identical); this
    component positions them via boundsForCell() and draws everything
    around and behind them.
*/
class RoutingCanvas : public juce::Component
{
public:
    explicit RoutingCanvas (RoutingGraph& graphToShow);

    static constexpr int numLanes = 4;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& key) override;

    /** Where an effect block on this lane/column goes. MainComponent uses
        the same call to place its EffectBlockComponent children, so the
        ports this component draws always line up with the real blocks. */
    juce::Rectangle<int> boundsForCell (int lane, int column) const;

    /** How many columns fit between the IN and OUT gutters right now. */
    int getNumColumns() const noexcept { return numColumns; }

    /** Screen position of one port, for cable drawing and hit-testing. */
    juce::Point<float> portPosition (NodeId node, int port, bool isInput) const;

    /** A drag from an output port landed on an input port (already
        validated by RoutingGraph::canConnect before this fires). */
    std::function<void (PortRef source, PortRef target)> onConnectionRequested;

    /** The selected cable was deleted (via the × badge or the Delete key). */
    std::function<void (ConnectionId)> onConnectionRemoved;

    /** An empty lane/column cell was clicked -- MainComponent opens the
        add-effect menu and places the new node right there. */
    std::function<void (int lane, int column)> onEmptyCellClicked;

    /** A block was dragged to a different cell. Only lane/column change --
        never the topology (the cables stay exactly as they were). */
    std::function<void (NodeId, int lane, int column)> onNodeMoved;

    /** Which cell a point falls in, clamped into range. */
    void cellForPosition (juce::Point<int> pos, int& laneOut, int& columnOut) const;

    void setSelectedConnection (ConnectionId id);
    ConnectionId getSelectedConnection() const noexcept { return selectedConnection; }

private:
    struct PortHit
    {
        PortRef ref;
        bool isInput = false;
        bool valid = false;
    };

    PortHit portAt (juce::Point<int> pos) const;
    ConnectionId connectionAt (juce::Point<int> pos) const;
    juce::Rectangle<int> boundsForNode (const GraphNode& node) const;
    juce::Path cablePath (juce::Point<float> from, juce::Point<float> to) const;
    void drawNodeBlock (juce::Graphics& g, const GraphNode& node, bool isActive) const;
    void drawPorts (juce::Graphics& g, const GraphNode& node, bool isActive) const;

    RoutingGraph& graph;

    int laneHeight = 100;
    int columnWidth = 120;
    int numColumns = 6;
    int gutterWidth = 92;

    PortRef dragSource;              // the output port a cable is being pulled from
    bool draggingCable = false;
    juce::Point<int> dragCurrentPos;
    PortHit hoveredPort;
    ConnectionId selectedConnection = invalidConnection;
    ConnectionId hoveredConnection = invalidConnection;
    int hoveredLane = -1, hoveredColumn = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingCanvas)
};

} // namespace pedaleira
