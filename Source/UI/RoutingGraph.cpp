#include "RoutingGraph.h"

#include <algorithm>
#include <map>
#include <set>

namespace pedaleira
{

NodeId RoutingGraph::addNode (NodeKind kind, juce::String typeKey, juce::String label,
                              int lane, int column, int numInputs, int numOutputs)
{
    GraphNode node;
    node.id = nextNodeId++;
    node.kind = kind;
    node.typeKey = std::move (typeKey);
    node.label = std::move (label);
    node.lane = lane;
    node.column = column;
    node.numInputs = juce::jmax (0, numInputs);
    node.numOutputs = juce::jmax (0, numOutputs);

    nodes.push_back (node);
    return node.id;
}

void RoutingGraph::removeNode (NodeId id)
{
    connections.erase (std::remove_if (connections.begin(), connections.end(),
                                        [id] (const Connection& c)
                                        { return c.source.node == id || c.target.node == id; }),
                        connections.end());

    nodes.erase (std::remove_if (nodes.begin(), nodes.end(),
                                  [id] (const GraphNode& n) { return n.id == id; }),
                  nodes.end());
}

const GraphNode* RoutingGraph::findNode (NodeId id) const
{
    const auto it = std::find_if (nodes.begin(), nodes.end(),
                                   [id] (const GraphNode& n) { return n.id == id; });
    return it != nodes.end() ? &*it : nullptr;
}

GraphNode* RoutingGraph::findNodeMutable (NodeId id)
{
    const auto it = std::find_if (nodes.begin(), nodes.end(),
                                   [id] (const GraphNode& n) { return n.id == id; });
    return it != nodes.end() ? &*it : nullptr;
}

const Connection* RoutingGraph::findConnection (ConnectionId id) const
{
    const auto it = std::find_if (connections.begin(), connections.end(),
                                   [id] (const Connection& c) { return c.id == id; });
    return it != connections.end() ? &*it : nullptr;
}

bool RoutingGraph::canReach (NodeId from, NodeId to) const
{
    if (from == to)
        return true;

    std::set<NodeId> seen { from };
    std::vector<NodeId> stack { from };

    while (! stack.empty())
    {
        const NodeId current = stack.back();
        stack.pop_back();

        for (const auto& c : connections)
        {
            if (c.source.node != current)
                continue;

            if (c.target.node == to)
                return true;

            if (seen.insert (c.target.node).second)
                stack.push_back (c.target.node);
        }
    }

    return false;
}

bool RoutingGraph::canConnect (PortRef source, PortRef target) const
{
    const auto* sourceNode = findNode (source.node);
    const auto* targetNode = findNode (target.node);

    if (sourceNode == nullptr || targetNode == nullptr)
        return false;

    if (source.port < 0 || source.port >= sourceNode->numOutputs)
        return false;
    if (target.port < 0 || target.port >= targetNode->numInputs)
        return false;

    if (source.node == target.node)
        return false;

    for (const auto& c : connections)
    {
        // One cable per input: a second source into the same input would be
        // an implicit, invisible sum. Summing is a MERGE node you can see.
        if (c.target == target)
            return false;
        if (c.source == source && c.target == target)
            return false;
    }

    // Following the new cable forwards must not lead back to where it starts.
    return ! canReach (target.node, source.node);
}

ConnectionId RoutingGraph::connect (PortRef source, PortRef target)
{
    if (! canConnect (source, target))
        return invalidConnection;

    Connection c;
    c.id = nextConnectionId++;
    c.source = source;
    c.target = target;
    connections.push_back (c);
    return c.id;
}

void RoutingGraph::disconnect (ConnectionId id)
{
    connections.erase (std::remove_if (connections.begin(), connections.end(),
                                        [id] (const Connection& c) { return c.id == id; }),
                        connections.end());
}

void RoutingGraph::clear()
{
    nodes.clear();
    connections.clear();
    nextNodeId = 1;
    nextConnectionId = 1;
}

std::vector<NodeId> RoutingGraph::processingOrder() const
{
    // Kahn's algorithm: repeatedly take a node whose every feeder has
    // already been taken. Whatever's left over at the end sat in a cycle.
    std::map<NodeId, int> remainingInputs;
    for (const auto& n : nodes)
        remainingInputs[n.id] = 0;

    for (const auto& c : connections)
        if (remainingInputs.count (c.target.node) != 0)
            ++remainingInputs[c.target.node];

    std::vector<NodeId> ready;
    for (const auto& [id, count] : remainingInputs)
        if (count == 0)
            ready.push_back (id);

    std::vector<NodeId> ordered;
    ordered.reserve (nodes.size());

    while (! ready.empty())
    {
        const NodeId current = ready.front();
        ready.erase (ready.begin());
        ordered.push_back (current);

        for (const auto& c : connections)
        {
            if (c.source.node != current)
                continue;

            auto it = remainingInputs.find (c.target.node);
            if (it != remainingInputs.end() && --it->second == 0)
                ready.push_back (c.target.node);
        }
    }

    return ordered;
}

std::vector<NodeId> RoutingGraph::activeNodes() const
{
    std::set<NodeId> downstreamOfInput;
    std::set<NodeId> upstreamOfOutput;

    // Forwards from every input.
    std::vector<NodeId> stack;
    for (const auto& n : nodes)
        if (n.kind == NodeKind::audioInput)
        {
            downstreamOfInput.insert (n.id);
            stack.push_back (n.id);
        }

    while (! stack.empty())
    {
        const NodeId current = stack.back();
        stack.pop_back();
        for (const auto& c : connections)
            if (c.source.node == current && downstreamOfInput.insert (c.target.node).second)
                stack.push_back (c.target.node);
    }

    // Backwards from every output.
    for (const auto& n : nodes)
        if (n.kind == NodeKind::audioOutput)
        {
            upstreamOfOutput.insert (n.id);
            stack.push_back (n.id);
        }

    while (! stack.empty())
    {
        const NodeId current = stack.back();
        stack.pop_back();
        for (const auto& c : connections)
            if (c.target.node == current && upstreamOfOutput.insert (c.source.node).second)
                stack.push_back (c.source.node);
    }

    std::vector<NodeId> result;
    for (const auto& n : nodes)
        if (downstreamOfInput.count (n.id) != 0 && upstreamOfOutput.count (n.id) != 0)
            result.push_back (n.id);

    return result;
}

std::vector<ConnectionId> RoutingGraph::activeConnections() const
{
    const auto active = activeNodes();
    const std::set<NodeId> activeSet (active.begin(), active.end());

    std::vector<ConnectionId> result;
    for (const auto& c : connections)
        if (activeSet.count (c.source.node) != 0 && activeSet.count (c.target.node) != 0)
            result.push_back (c.id);

    return result;
}

std::unique_ptr<juce::XmlElement> RoutingGraph::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("RoutingGraph");

    for (const auto& n : nodes)
    {
        auto* nodeXml = xml->createNewChildElement ("Node");
        nodeXml->setAttribute ("id", n.id);
        nodeXml->setAttribute ("kind", (int) n.kind);
        nodeXml->setAttribute ("typeKey", n.typeKey);
        nodeXml->setAttribute ("label", n.label);
        nodeXml->setAttribute ("lane", n.lane);
        nodeXml->setAttribute ("column", n.column);
        nodeXml->setAttribute ("numInputs", n.numInputs);
        nodeXml->setAttribute ("numOutputs", n.numOutputs);
    }

    for (const auto& c : connections)
    {
        auto* connXml = xml->createNewChildElement ("Connection");
        connXml->setAttribute ("id", c.id);
        connXml->setAttribute ("sourceNode", c.source.node);
        connXml->setAttribute ("sourcePort", c.source.port);
        connXml->setAttribute ("targetNode", c.target.node);
        connXml->setAttribute ("targetPort", c.target.port);
    }

    return xml;
}

void RoutingGraph::fromXml (const juce::XmlElement& xml)
{
    clear();

    for (auto* nodeXml : xml.getChildWithTagNameIterator ("Node"))
    {
        GraphNode n;
        n.id = nodeXml->getIntAttribute ("id");
        n.kind = (NodeKind) nodeXml->getIntAttribute ("kind", (int) NodeKind::effect);
        n.typeKey = nodeXml->getStringAttribute ("typeKey");
        n.label = nodeXml->getStringAttribute ("label");
        n.lane = nodeXml->getIntAttribute ("lane");
        n.column = nodeXml->getIntAttribute ("column");
        n.numInputs = nodeXml->getIntAttribute ("numInputs", 1);
        n.numOutputs = nodeXml->getIntAttribute ("numOutputs", 1);

        nodes.push_back (n);
        nextNodeId = juce::jmax (nextNodeId, n.id + 1);
    }

    for (auto* connXml : xml.getChildWithTagNameIterator ("Connection"))
    {
        Connection c;
        c.id = connXml->getIntAttribute ("id");
        c.source = { connXml->getIntAttribute ("sourceNode"), connXml->getIntAttribute ("sourcePort") };
        c.target = { connXml->getIntAttribute ("targetNode"), connXml->getIntAttribute ("targetPort") };

        connections.push_back (c);
        nextConnectionId = juce::jmax (nextConnectionId, c.id + 1);
    }
}

} // namespace pedaleira
