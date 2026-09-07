#include "Engine/DeferredReclaimer.h"

#include <juce_core/juce_core.h>

namespace pedaleira
{

namespace
{
    struct Counted
    {
        explicit Counted (std::atomic<int>& counter) : liveCount (counter) { ++liveCount; }
        ~Counted() { --liveCount; }
        std::atomic<int>& liveCount;
    };
}

class DeferredReclaimerTests : public juce::UnitTest
{
public:
    DeferredReclaimerTests() : juce::UnitTest ("DeferredReclaimer", "Engine") {}

    void runTest() override
    {
        beginTest ("publish swaps the active pointer immediately");
        {
            std::atomic<int> live { 0 };
            DeferredReclaimer<Counted> slot;

            slot.publish (std::make_unique<Counted> (live));
            expectEquals (live.load(), 1);
            auto* first = slot.currentRaw();
            expect (first != nullptr);

            slot.publish (std::make_unique<Counted> (live));
            expectEquals (live.load(), 2); // the old one hasn't been swept yet
            expect (slot.currentRaw() != first);
        }

        beginTest ("sweep frees old objects after the safety margin");
        {
            std::atomic<int> live { 0 };
            DeferredReclaimer<Counted> slot;

            slot.publish (std::make_unique<Counted> (live));
            slot.publish (std::make_unique<Counted> (live));
            expectEquals (live.load(), 2);

            slot.sweep (0); // zero margin -- test only; production uses >=500ms
            expectEquals (live.load(), 1);
        }

        beginTest ("the destructor frees everything, active and retired");
        {
            std::atomic<int> live { 0 };
            {
                DeferredReclaimer<Counted> slot;
                slot.publish (std::make_unique<Counted> (live));
                slot.publish (std::make_unique<Counted> (live));
                expectEquals (live.load(), 2);
            }
            expectEquals (live.load(), 0);
        }
    }
};

static DeferredReclaimerTests deferredReclaimerTests;

} // namespace pedaleira
