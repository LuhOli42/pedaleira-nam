#include "UI/RoutingGraph.h"

#include <juce_core/juce_core.h>

#include <algorithm>

namespace pedaleira
{

class RoutingGraphTests : public juce::UnitTest
{
public:
    RoutingGraphTests() : juce::UnitTest ("RoutingGraph", "UI") {}

    static bool contains (const std::vector<NodeId>& v, NodeId id)
    {
        return std::find (v.begin(), v.end(), id) != v.end();
    }

    static int indexOf (const std::vector<NodeId>& v, NodeId id)
    {
        const auto it = std::find (v.begin(), v.end(), id);
        return it == v.end() ? -1 : (int) std::distance (v.begin(), it);
    }

    void runTest() override
    {
        beginTest ("processing order comes from the cables, not from lane, column or insertion order");
        {
            RoutingGraph g;
            // Added last-to-first and placed on lanes in the opposite order
            // to how they're cabled -- if either of those leaked into the
            // ordering, this test would catch it.
            const auto last = g.addNode (NodeKind::effect, "C", "C", 0, 0, 1, 1);
            const auto first = g.addNode (NodeKind::effect, "A", "A", 3, 9, 1, 1);
            const auto middle = g.addNode (NodeKind::effect, "B", "B", 2, 5, 1, 1);

            g.connect ({ first, 0 }, { middle, 0 });
            g.connect ({ middle, 0 }, { last, 0 });

            const auto order = g.processingOrder();
            expect (indexOf (order, first) < indexOf (order, middle));
            expect (indexOf (order, middle) < indexOf (order, last));
        }

        beginTest ("an input takes only one cable, an output can feed many (that's a split)");
        {
            RoutingGraph g;
            const auto source = g.addNode (NodeKind::effect, "src", "src", 0, 0, 1, 1);
            const auto branchA = g.addNode (NodeKind::effect, "a", "a", 0, 1, 1, 1);
            const auto branchB = g.addNode (NodeKind::effect, "b", "b", 1, 1, 1, 1);

            expect (g.connect ({ source, 0 }, { branchA, 0 }) != invalidConnection);
            expect (g.connect ({ source, 0 }, { branchB, 0 }) != invalidConnection); // split: same output, second target

            // A second cable into branchA's single input is refused -- summing
            // is an explicit merge node, never an invisible implicit one.
            const auto other = g.addNode (NodeKind::effect, "other", "other", 2, 0, 1, 1);
            expect (g.connect ({ other, 0 }, { branchA, 0 }) == invalidConnection);
        }

        beginTest ("a cable that would close a loop is refused");
        {
            RoutingGraph g;
            const auto a = g.addNode (NodeKind::effect, "a", "a", 0, 0, 1, 1);
            const auto b = g.addNode (NodeKind::effect, "b", "b", 0, 1, 1, 1);
            const auto c = g.addNode (NodeKind::effect, "c", "c", 0, 2, 1, 1);

            g.connect ({ a, 0 }, { b, 0 });
            g.connect ({ b, 0 }, { c, 0 });

            expect (! g.canConnect ({ c, 0 }, { a, 0 })); // would make a -> b -> c -> a
            expect (g.connect ({ c, 0 }, { a, 0 }) == invalidConnection);
        }

        beginTest ("only what reaches an output from an input counts as active");
        {
            RoutingGraph g;
            const auto in = g.addNode (NodeKind::audioInput, {}, "IN", 0, 0, 0, 2);
            const auto out = g.addNode (NodeKind::audioOutput, {}, "OUT", 0, 0, 2, 0);
            const auto wired = g.addNode (NodeKind::effect, "wired", "wired", 0, 1, 1, 1);
            const auto orphan = g.addNode (NodeKind::effect, "orphan", "orphan", 1, 1, 1, 1);
            const auto deadEnd = g.addNode (NodeKind::effect, "dead", "dead", 2, 1, 1, 1);

            g.connect ({ in, 0 }, { wired, 0 });
            g.connect ({ wired, 0 }, { out, 0 });
            g.connect ({ in, 1 }, { deadEnd, 0 }); // fed, but goes nowhere

            const auto active = g.activeNodes();
            expect (contains (active, wired));
            expect (contains (active, in));
            expect (contains (active, out));
            expect (! contains (active, orphan));  // no cables at all
            expect (! contains (active, deadEnd)); // fed but never reaches the output
        }

        beginTest ("removing a node takes its cables with it");
        {
            RoutingGraph g;
            const auto a = g.addNode (NodeKind::effect, "a", "a", 0, 0, 1, 1);
            const auto b = g.addNode (NodeKind::effect, "b", "b", 0, 1, 1, 1);
            const auto c = g.addNode (NodeKind::effect, "c", "c", 0, 2, 1, 1);

            g.connect ({ a, 0 }, { b, 0 });
            g.connect ({ b, 0 }, { c, 0 });
            expect (g.getConnections().size() == 2);

            g.removeNode (b);
            expect (g.getConnections().empty());
            expect (g.findNode (b) == nullptr);
            expect (g.findNode (a) != nullptr);
        }

        beginTest ("a graph survives a save/load round trip, cables included");
        {
            RoutingGraph g;
            const auto in = g.addNode (NodeKind::audioInput, {}, "IN", 0, 0, 0, 2);
            const auto fx = g.addNode (NodeKind::effect, "Overdrive", "Overdrive", 2, 3, 1, 1);
            const auto out = g.addNode (NodeKind::audioOutput, {}, "OUT", 0, 0, 2, 0);
            g.connect ({ in, 0 }, { fx, 0 });
            g.connect ({ fx, 0 }, { out, 1 });

            const auto xml = g.toXml();

            RoutingGraph restored;
            restored.fromXml (*xml);

            expect (restored.getNodes().size() == 3);
            expect (restored.getConnections().size() == 2);

            const auto* restoredFx = restored.findNode (fx);
            expect (restoredFx != nullptr);
            expect (restoredFx->typeKey == "Overdrive");
            expect (restoredFx->lane == 2);
            expect (restoredFx->column == 3);

            // The restored graph still knows the same path is live.
            expect (contains (restored.activeNodes(), fx));

            // And a new node added afterwards doesn't collide with restored ids.
            const auto fresh = restored.addNode (NodeKind::effect, "x", "x", 0, 0, 1, 1);
            expect (restored.findNode (fresh) != nullptr);
            expect (fresh != in && fresh != fx && fresh != out);
        }
    }
};

static RoutingGraphTests routingGraphTests;

} // namespace pedaleira
