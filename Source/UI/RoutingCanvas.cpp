#include "RoutingCanvas.h"

#include "PedaleiraLookAndFeel.h"

#include <algorithm>
#include <set>

namespace pedaleira
{

namespace
{
    constexpr float portRadius = 5.0f;
    constexpr float portOffset = 7.0f; // how far a port sticks out past its block's edge
    constexpr int columnGap = 14;
    constexpr int laneInset = 14;
    constexpr float badgeRadius = 8.0f;

    juce::Colour laneColour (int lane)
    {
        // Four distinguishable rails, but all in the same family -- they're
        // organisational tracks, not four unrelated chains.
        static const juce::Colour colours[] = {
            juce::Colour (0xff4fb3d9), juce::Colour (0xff9d7fe0),
            juce::Colour (0xff62c08a), juce::Colour (0xffd8a33f)
        };
        return colours[juce::jlimit (0, 3, lane)];
    }
}

RoutingCanvas::RoutingCanvas (RoutingGraph& graphToShow)
    : graph (graphToShow)
{
    setWantsKeyboardFocus (true);
}

void RoutingCanvas::resized()
{
    laneHeight = juce::jmax (60, getHeight() / numLanes);

    const int middleWidth = juce::jmax (120, getWidth() - 2 * gutterWidth - 32);
    numColumns = juce::jmax (1, middleWidth / 150);
    columnWidth = middleWidth / numColumns;
}

juce::Rectangle<int> RoutingCanvas::boundsForCell (int lane, int column) const
{
    const int x = gutterWidth + 16 + column * columnWidth;
    const int y = lane * laneHeight + laneInset;
    return { x, y, juce::jmax (40, columnWidth - columnGap), juce::jmax (40, laneHeight - 2 * laneInset) };
}

juce::Rectangle<int> RoutingCanvas::boundsForNode (const GraphNode& node) const
{
    if (node.kind == NodeKind::audioInput)
        return { 4, laneInset, gutterWidth - 8, getHeight() - 2 * laneInset };

    if (node.kind == NodeKind::audioOutput)
        return { getWidth() - gutterWidth + 4, laneInset, gutterWidth - 8, getHeight() - 2 * laneInset };

    return boundsForCell (node.lane, node.column);
}

juce::Point<float> RoutingCanvas::portPosition (NodeId nodeId, int port, bool isInput) const
{
    const auto* node = graph.findNode (nodeId);
    if (node == nullptr)
        return {};

    const auto r = boundsForNode (*node).toFloat();
    const int count = juce::jmax (1, isInput ? node->numInputs : node->numOutputs);

    // Ports spread down the block's edge; a single port sits dead centre.
    const float usable = r.getHeight() - 2.0f * portRadius - 8.0f;
    const float step = count > 1 ? usable / (float) (count - 1) : 0.0f;
    const float y = count > 1 ? r.getY() + portRadius + 4.0f + step * (float) port
                               : r.getCentreY();

    return { isInput ? r.getX() - portOffset : r.getRight() + portOffset, y };
}

juce::Path RoutingCanvas::cablePath (juce::Point<float> from, juce::Point<float> to) const
{
    // Horizontal-tangent cubic: leaves an output going right, arrives at an
    // input coming from the left, however the two are positioned. Reads as
    // a patch cable rather than a routed wire.
    const float dx = juce::jmax (40.0f, std::abs (to.x - from.x) * 0.5f);

    juce::Path p;
    p.startNewSubPath (from);
    p.cubicTo (from.x + dx, from.y, to.x - dx, to.y, to.x, to.y);
    return p;
}

void RoutingCanvas::drawNodeBlock (juce::Graphics& g, const GraphNode& node, bool isActive) const
{
    const auto r = boundsForNode (node).toFloat();
    const auto accent = node.kind == NodeKind::audioInput || node.kind == NodeKind::audioOutput
                            ? PedaleiraLookAndFeel::getAppAccentColour()
                            : laneColour (node.lane);

    g.setColour (juce::Colour (0xff141414).withAlpha (isActive ? 1.0f : 0.6f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (accent.withAlpha (isActive ? 1.0f : 0.35f));
    g.drawRoundedRectangle (r, 8.0f, 2.0f);

    g.setColour (juce::Colours::white.withAlpha (isActive ? 0.95f : 0.4f));
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawText (node.label, r.reduced (6.0f), juce::Justification::centred, true);
}

void RoutingCanvas::drawPorts (juce::Graphics& g, const GraphNode& node, bool isActive) const
{
    const auto accent = laneColour (node.lane);

    for (int i = 0; i < node.numInputs; ++i)
    {
        const auto p = portPosition (node.id, i, true);
        const bool hot = hoveredPort.valid && hoveredPort.isInput && hoveredPort.ref == PortRef { node.id, i };
        g.setColour ((hot ? juce::Colours::white : accent).withAlpha (isActive || hot ? 1.0f : 0.45f));
        g.fillEllipse (p.x - portRadius, p.y - portRadius, portRadius * 2.0f, portRadius * 2.0f);
    }

    for (int i = 0; i < node.numOutputs; ++i)
    {
        const auto p = portPosition (node.id, i, false);
        const bool hot = hoveredPort.valid && ! hoveredPort.isInput && hoveredPort.ref == PortRef { node.id, i };
        g.setColour ((hot ? juce::Colours::white : accent).withAlpha (isActive || hot ? 1.0f : 0.45f));
        g.fillEllipse (p.x - portRadius, p.y - portRadius, portRadius * 2.0f, portRadius * 2.0f);
    }
}

void RoutingCanvas::paint (juce::Graphics& g)
{
    // 1. The four rails. Drawn first so everything else sits on top.
    for (int lane = 0; lane < numLanes; ++lane)
    {
        const float y = (float) (lane * laneHeight + laneHeight / 2);
        g.setColour (laneColour (lane).withAlpha (0.18f));
        g.drawLine ((float) gutterWidth, y, (float) (getWidth() - gutterWidth), y, 2.0f);

        g.setColour (laneColour (lane).withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText ("LINE " + juce::String (lane + 1),
                     juce::Rectangle<int> (gutterWidth + 4, lane * laneHeight + 2, 60, 14),
                     juce::Justification::centredLeft, false);
    }

    // 2. The hovered empty cell's "+", same affordance the grid UI had.
    if (hoveredLane >= 0 && ! draggingCable)
    {
        bool occupied = false;
        for (const auto& n : graph.getNodes())
            if (n.kind == NodeKind::effect && n.lane == hoveredLane && n.column == hoveredColumn)
                occupied = true;

        if (! occupied)
        {
            const auto cell = boundsForCell (hoveredLane, hoveredColumn).toFloat().reduced (4.0f);
            const auto accent = laneColour (hoveredLane);
            g.setColour (accent.withAlpha (0.10f));
            g.fillRoundedRectangle (cell, 8.0f);
            g.setColour (accent.withAlpha (0.6f));
            g.drawRoundedRectangle (cell, 8.0f, 1.5f);

            g.setColour (juce::Colours::white.withAlpha (0.8f));
            const float s = juce::jmin (cell.getWidth(), cell.getHeight()) * 0.22f;
            const auto c = cell.getCentre();
            g.drawLine (c.x - s, c.y, c.x + s, c.y, 2.5f);
            g.drawLine (c.x, c.y - s, c.x, c.y + s, 2.5f);
        }
    }

    // 3. Cables, behind the blocks (a parent paints before its children, and
    //    the effect blocks are children) -- so a cable visually runs
    //    underneath the blocks it connects, like a patch lead.
    const auto activeConns = graph.activeConnections();
    const std::set<ConnectionId> activeSet (activeConns.begin(), activeConns.end());

    for (const auto& c : graph.getConnections())
    {
        const auto from = portPosition (c.source.node, c.source.port, false);
        const auto to = portPosition (c.target.node, c.target.port, true);
        const auto path = cablePath (from, to);

        const bool isActive = activeSet.count (c.id) != 0;
        const bool isSelected = c.id == selectedConnection;
        const bool isHovered = c.id == hoveredConnection;

        const auto* sourceNode = graph.findNode (c.source.node);
        auto colour = sourceNode != nullptr ? laneColour (sourceNode->lane)
                                             : PedaleiraLookAndFeel::getAppAccentColour();
        if (! isActive)
            colour = colour.withAlpha (0.25f); // not on a path from an input to an output

        g.setColour (isSelected ? juce::Colours::white : colour.withAlpha (isHovered ? 1.0f : colour.getFloatAlpha()));
        g.strokePath (path, juce::PathStrokeType (isSelected || isHovered ? 3.5f : 2.5f,
                                                   juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Direction: a small arrowhead at the cable's midpoint, so which way
        // the audio flows is readable without tracing the whole cable.
        const auto mid = path.getPointAlongPath (path.getLength() * 0.5f);
        const auto tangent = path.getPointAlongPath (juce::jmin (path.getLength(), path.getLength() * 0.5f + 6.0f));
        const auto dir = (tangent - mid);
        const float angle = std::atan2 (dir.y, dir.x);

        juce::Path arrow;
        arrow.addTriangle (mid.x + 6.0f * std::cos (angle), mid.y + 6.0f * std::sin (angle),
                            mid.x + 6.0f * std::cos (angle + 2.5f), mid.y + 6.0f * std::sin (angle + 2.5f),
                            mid.x + 6.0f * std::cos (angle - 2.5f), mid.y + 6.0f * std::sin (angle - 2.5f));
        g.fillPath (arrow);

        // The selected cable gets a delete badge you can click.
        if (isSelected)
        {
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.fillEllipse (mid.x - badgeRadius, mid.y - badgeRadius, badgeRadius * 2.0f, badgeRadius * 2.0f);
            g.setColour (juce::Colours::white);
            g.drawEllipse (mid.x - badgeRadius, mid.y - badgeRadius, badgeRadius * 2.0f, badgeRadius * 2.0f, 1.5f);
            const float x = badgeRadius * 0.45f;
            g.drawLine (mid.x - x, mid.y - x, mid.x + x, mid.y + x, 1.8f);
            g.drawLine (mid.x - x, mid.y + x, mid.x + x, mid.y - x, 1.8f);
        }
    }

    // 4. The cable currently being pulled from a port.
    if (draggingCable && dragSource.isValid())
    {
        const auto from = portPosition (dragSource.node, dragSource.port, false);
        const auto to = dragCurrentPos.toFloat();
        const bool wouldConnect = hoveredPort.valid && hoveredPort.isInput
                                   && graph.canConnect (dragSource, hoveredPort.ref);

        g.setColour (wouldConnect ? juce::Colours::white : juce::Colours::white.withAlpha (0.45f));
        g.strokePath (cablePath (from, to), juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                                   juce::PathStrokeType::rounded));
    }

    // 5. IN/OUT nodes are drawn here (effect nodes are real child
    //    components and paint themselves after this), then every node's
    //    ports on top of the cables that land on them.
    const auto activeNodeIds = graph.activeNodes();
    const std::set<NodeId> activeNodeSet (activeNodeIds.begin(), activeNodeIds.end());

    for (const auto& n : graph.getNodes())
    {
        const bool isActive = activeNodeSet.count (n.id) != 0;
        if (n.kind != NodeKind::effect)
            drawNodeBlock (g, n, isActive);
        drawPorts (g, n, isActive);
    }
}

RoutingCanvas::PortHit RoutingCanvas::portAt (juce::Point<int> pos) const
{
    const auto p = pos.toFloat();
    constexpr float grabRadius = portRadius + 5.0f;

    for (const auto& n : graph.getNodes())
    {
        for (int i = 0; i < n.numInputs; ++i)
            if (portPosition (n.id, i, true).getDistanceFrom (p) <= grabRadius)
                return { { n.id, i }, true, true };

        for (int i = 0; i < n.numOutputs; ++i)
            if (portPosition (n.id, i, false).getDistanceFrom (p) <= grabRadius)
                return { { n.id, i }, false, true };
    }

    return {};
}

ConnectionId RoutingCanvas::connectionAt (juce::Point<int> pos) const
{
    const auto p = pos.toFloat();
    constexpr float grabDistance = 7.0f;

    for (const auto& c : graph.getConnections())
    {
        const auto path = cablePath (portPosition (c.source.node, c.source.port, false),
                                      portPosition (c.target.node, c.target.port, true));

        // Sample along the curve: JUCE has no "distance to path" helper, and
        // Path::contains() only answers for filled shapes, not strokes.
        const float length = path.getLength();
        for (float d = 0.0f; d <= length; d += 4.0f)
        {
            if (path.getPointAlongPath (d).getDistanceFrom (p) <= grabDistance)
                return c.id;
        }
    }

    return invalidConnection;
}

void RoutingCanvas::cellForPosition (juce::Point<int> pos, int& laneOut, int& columnOut) const
{
    laneOut = juce::jlimit (0, numLanes - 1, pos.y / juce::jmax (1, laneHeight));
    columnOut = juce::jlimit (0, numColumns - 1, (pos.x - gutterWidth - 16) / juce::jmax (1, columnWidth));
}

void RoutingCanvas::setSelectedConnection (ConnectionId id)
{
    if (selectedConnection != id)
    {
        selectedConnection = id;
        repaint();
    }
}

void RoutingCanvas::mouseMove (const juce::MouseEvent& event)
{
    const auto hit = portAt (event.getPosition());
    const auto conn = hit.valid ? invalidConnection : connectionAt (event.getPosition());

    int lane = -1, column = -1;
    if (! hit.valid && conn == invalidConnection
        && event.x > gutterWidth && event.x < getWidth() - gutterWidth)
        cellForPosition (event.getPosition(), lane, column);

    if (hit.ref != hoveredPort.ref || hit.valid != hoveredPort.valid
        || conn != hoveredConnection || lane != hoveredLane || column != hoveredColumn)
    {
        hoveredPort = hit;
        hoveredConnection = conn;
        hoveredLane = lane;
        hoveredColumn = column;
        repaint();
    }
}

void RoutingCanvas::mouseExit (const juce::MouseEvent&)
{
    hoveredPort = {};
    hoveredConnection = invalidConnection;
    hoveredLane = hoveredColumn = -1;
    repaint();
}

void RoutingCanvas::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    // The selected cable's × badge takes priority over everything else.
    if (selectedConnection != invalidConnection)
    {
        if (const auto* c = graph.findConnection (selectedConnection))
        {
            const auto path = cablePath (portPosition (c->source.node, c->source.port, false),
                                          portPosition (c->target.node, c->target.port, true));
            const auto mid = path.getPointAlongPath (path.getLength() * 0.5f);

            if (mid.getDistanceFrom (event.position) <= badgeRadius + 2.0f)
            {
                const auto id = selectedConnection;
                selectedConnection = invalidConnection;
                if (onConnectionRemoved)
                    onConnectionRemoved (id);
                repaint();
                return;
            }
        }
    }

    const auto hit = portAt (event.getPosition());
    if (hit.valid && ! hit.isInput)
    {
        // Pull a new cable out of an output port.
        dragSource = hit.ref;
        draggingCable = true;
        dragCurrentPos = event.getPosition();
        setSelectedConnection (invalidConnection);
        repaint();
        return;
    }

    if (hit.valid && hit.isInput)
    {
        // Grabbing a fed input picks that cable back up, so re-patching is
        // one gesture instead of delete-then-reconnect.
        for (const auto& c : graph.getConnections())
            if (c.target == hit.ref)
            {
                dragSource = c.source;
                draggingCable = true;
                dragCurrentPos = event.getPosition();
                if (onConnectionRemoved)
                    onConnectionRemoved (c.id);
                repaint();
                return;
            }
        return;
    }

    const auto conn = connectionAt (event.getPosition());
    setSelectedConnection (conn);
}

void RoutingCanvas::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingCable)
        return;

    dragCurrentPos = event.getPosition();
    hoveredPort = portAt (event.getPosition());
    repaint();
}

void RoutingCanvas::mouseUp (const juce::MouseEvent& event)
{
    if (draggingCable)
    {
        const auto hit = portAt (event.getPosition());
        if (hit.valid && hit.isInput && graph.canConnect (dragSource, hit.ref) && onConnectionRequested)
            onConnectionRequested (dragSource, hit.ref);

        draggingCable = false;
        dragSource = {};
        repaint();
        return;
    }

    // A plain click on empty middle ground opens the add-effect menu there.
    if (! event.mouseWasDraggedSinceMouseDown()
        && connectionAt (event.getPosition()) == invalidConnection
        && ! portAt (event.getPosition()).valid
        && event.x > gutterWidth && event.x < getWidth() - gutterWidth)
    {
        int lane = 0, column = 0;
        cellForPosition (event.getPosition(), lane, column);

        for (const auto& n : graph.getNodes())
            if (n.kind == NodeKind::effect && n.lane == lane && n.column == column)
                return; // occupied -- the block itself handles its own clicks

        if (onEmptyCellClicked)
            onEmptyCellClicked (lane, column);
    }
}

bool RoutingCanvas::keyPressed (const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        && selectedConnection != invalidConnection)
    {
        const auto id = selectedConnection;
        selectedConnection = invalidConnection;
        if (onConnectionRemoved)
            onConnectionRemoved (id);
        repaint();
        return true;
    }

    return false;
}

} // namespace pedaleira
