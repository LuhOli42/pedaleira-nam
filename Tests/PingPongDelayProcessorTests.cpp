#include "Effects/PingPongDelayProcessor.h"

#include <juce_core/juce_core.h>

namespace pedaleira
{

class PingPongDelayProcessorTests : public juce::UnitTest
{
public:
    PingPongDelayProcessorTests() : juce::UnitTest ("PingPongDelayProcessor", "Effects") {}

    void runTest() override
    {
        beginTest ("an impulse in the left channel repeats in the RIGHT channel, not the left");
        {
            PingPongDelayProcessor delay;
            delay.prepare (48000.0, 512, 2);

            // pingpong_time defaults to 280ms.
            const int delaySamples = (int) (0.28 * 48000.0);
            juce::AudioBuffer<float> buffer (2, delaySamples + 200);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f); // left channel only

            delay.process (buffer);

            float peakLeft = 0.0f, peakRight = 0.0f;
            int peakRightIndex = 0;
            for (int i = 100; i < buffer.getNumSamples(); ++i)
            {
                peakLeft = juce::jmax (peakLeft, std::abs (buffer.getSample (0, i)));
                const float r = std::abs (buffer.getSample (1, i));
                if (r > peakRight)
                {
                    peakRight = r;
                    peakRightIndex = i;
                }
            }

            expectWithinAbsoluteError (peakRightIndex, delaySamples, 4);
            expect (peakRight > 0.2f);
            expect (peakRight > peakLeft * 3.0f); // the bounce should dominate the same-side repeat
        }

        beginTest ("with fewer than 2 channels, the signal passes through unchanged");
        {
            PingPongDelayProcessor delay;
            delay.prepare (48000.0, 512, 1);

            juce::AudioBuffer<float> buffer (1, 256);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 0.5f);

            delay.process (buffer);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                expectWithinAbsoluteError (buffer.getSample (0, i), 0.5f, 0.0001f);
        }

        beginTest ("repeated blocks of a bounded signal never produce NaN/Inf or runaway output");
        {
            PingPongDelayProcessor delay;
            delay.prepare (48000.0, 512, 2);

            juce::Random random (161718);
            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < 200; ++block)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

                delay.process (buffer);

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                    {
                        const float sample = buffer.getSample (ch, i);
                        expect (std::isfinite (sample));
                        expect (std::abs (sample) < 10.0f);
                    }
            }
        }
    }
};

static PingPongDelayProcessorTests pingPongDelayProcessorTests;

} // namespace pedaleira
