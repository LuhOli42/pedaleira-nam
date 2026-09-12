#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace pedaleira
{

using NodeId = int;
using ConnectionId = int;

inline constexpr NodeId invalidNode = -1;
inline constexpr ConnectionId invalidConnection = -1;

enum class NodeKind
{
    audioInput,  // the hardware input -- L/R output ports, no inputs
    audioOutput, // the hardware output -- L/R input ports, no outputs
    effect,      // an EffectRegistry type (typeKey names which)
    split,       // 1 in -> 2 out, for parallel paths
    merge        // 2 in -> 1 out, sums its inputs
};

/** One endpoint: a specific port on a specific node. Ports are indexed per
    direction (input port 0 and output port 0 are different endpoints), so a
    PortRef is only meaningful together with the direction it came from --
    Connection below fixes that by construction (source is always an output,
    target always an input). */
struct PortRef
{
    NodeId node = invalidNode;
    int port = 0;

    bool operator== (const PortRef& other) const noexcept { return node == other.node && port == other.port; }
    bool operator!= (const PortRef& other) const noexcept { return ! (*this == other); }
    bool isValid() const noexcept { return node != invalidNode; }
};

struct GraphNode
{
    NodeId id = invalidNode;
    NodeKind kind = NodeKind::effect;
    juce::String typeKey; // EffectRegistry key -- effect nodes only
    juce::String label;   // what the block shows
    int lane = 0;         // 0..3, VISUAL organisation only -- never affects topology
    int column = 0;       // horizontal slot within the lane, also visual only
    int numInputs = 1;
    int numOutputs = 1;
};

struct Connection
{
    ConnectionId id = invalidConnection;
    PortRef source; // always an OUTPUT port
    PortRef target; // always an INPUT port
};

/**
    The real topology of the signal path: nodes + ports + connections, with
    the connections (NOT a node's lane/column, and NOT the order nodes were
    added) deciding what feeds what. This is deliberately the opposite of
    the ordered-list-of-effects model it replaces -- there is no "position
    3 of 8" concept in here at all, because the user's requirement is
    "connect audio like plugging cables", not "choose each effect's index
    in a list".

    Lane/column exist ONLY so the patchbay UI has somewhere to draw each
    block; moving a block between lanes changes nothing about the audio.
    processingOrder() is what an audio engine would consume: a topological
    sort of the graph, so evaluation order falls out of the cables.

    Pure data + graph algorithms -- no JUCE GUI types, no audio-thread
    access. RoutingCanvas owns the view; the engine hookup (splits/merges
    as real parallel DSP) comes after this layer, per the staged plan.
*/
class RoutingGraph
{
public:
    NodeId addNode (NodeKind kind, juce::String typeKey, juce::String label,
                    int lane, int column, int numInputs, int numOutputs);
    void removeNode (NodeId id); // also drops every connection touching it

    /** False when the endpoints don't exist, the ports are out of range,
        the target input is already fed (an input takes one cable; use a
        merge node to sum), the pair is already connected, or the cable
        would close a loop. */
    bool canConnect (PortRef source, PortRef target) const;
    ConnectionId connect (PortRef source, PortRef target); // invalidConnection if !canConnect
    void disconnect (ConnectionId id);

    void clear();

    const std::vector<GraphNode>& getNodes() const noexcept { return nodes; }
    const std::vector<Connection>& getConnections() const noexcept { return connections; }
    const GraphNode* findNode (NodeId id) const;
    GraphNode* findNodeMutable (NodeId id);
    const Connection* findConnection (ConnectionId id) const;

    /** Every node in an order where each one comes after everything feeding
        it. Nodes in a cycle can't be ordered and are left out -- connect()
        refuses to create one in the first place, so that only happens with
        externally-loaded state. */
    std::vector<NodeId> processingOrder() const;

    /** The nodes/cables that actually carry audio right now: reachable from
        some audioInput AND reaching some audioOutput. Everything else is
        drawn dimmed, so "where is the audio going?" is answerable at a
        glance (the stated priority for this whole feature). */
    std::vector<NodeId> activeNodes() const;
    std::vector<ConnectionId> activeConnections() const;

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement& xml);

private:
    bool canReach (NodeId from, NodeId to) const; // follows connections forwards

    std::vector<GraphNode> nodes;
    std::vector<Connection> connections;
    NodeId nextNodeId = 1;
    ConnectionId nextConnectionId = 1;
};

} // namespace pedaleira
