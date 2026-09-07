#include "Engine/ParameterManager.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace pedaleira
{

class ParameterManagerTests : public juce::UnitTest
{
public:
    ParameterManagerTests() : juce::UnitTest ("ParameterManager", "Engine") {}

    void runTest() override
    {
        beginTest ("push/drain preserves FIFO order");
        {
            ParameterManager pm (8);
            pm.push (1, 0.1f);
            pm.push (2, 0.2f);
            pm.push (3, 0.3f);

            std::vector<ParameterManager::Change> received;
            pm.drain ([&] (const ParameterManager::Change& c) { received.push_back (c); });

            expectEquals ((int) received.size(), 3);
            expectEquals ((int) received[0].parameterId, 1);
            expectEquals ((int) received[2].parameterId, 3);
        }

        beginTest ("a full queue drops instead of blocking");
        {
            ParameterManager pm (2);
            expect (pm.push (1, 0.0f));
            expect (pm.push (2, 0.0f));
            expect (! pm.push (3, 0.0f)); // full -- returns false, never stalls
        }

        beginTest ("drain empties the queue (a second drain finds nothing)");
        {
            ParameterManager pm (8);
            pm.push (1, 0.0f);

            int count = 0;
            pm.drain ([&] (const ParameterManager::Change&) { ++count; });
            pm.drain ([&] (const ParameterManager::Change&) { ++count; });

            expectEquals (count, 1);
        }
    }
};

static ParameterManagerTests parameterManagerTests;

} // namespace pedaleira
