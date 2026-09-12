#include "Effects/OverdriveProcessor.h"

#include <juce_core/juce_core.h>

namespace openguitarmultifx
{

class OverdriveProcessorTests : public juce::UnitTest
{
public:
    OverdriveProcessorTests() : juce::UnitTest ("OverdriveProcessor", "Effects") {}

    void runTest() override
    {
        beginTest ("output never exceeds the level ceiling, even with a huge input");
        {
            OverdriveProcessor od;
            od.prepare (48000.0, 512, 1);

            juce::AudioBuffer<float> buffer (1, 64);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 100.0f); // way beyond 1/drive

            od.process (buffer);

            // default level is 0dB -> ceiling is tanh(...) * 1.0. At this drive,
            // tanh saturates to exactly 1.0f in float32 precision -- allow that,
            // just reject anything that overshoots the ceiling.
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                expect (std::abs (buffer.getSample (0, i)) <= 1.0f);
        }

        beginTest ("a silent input stays silent (odd-symmetric waveshaper, no DC)");
        {
            OverdriveProcessor od;
            od.prepare (48000.0, 512, 1);

            juce::AudioBuffer<float> buffer (1, 16);
            buffer.clear();

            od.process (buffer);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                expectWithinAbsoluteError (buffer.getSample (0, i), 0.0f, 1.0e-6f);
        }
    }
};

static OverdriveProcessorTests overdriveProcessorTests;

} // namespace openguitarmultifx
