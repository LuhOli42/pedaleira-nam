#include "Effects/HoldProcessor.h"

#include <juce_core/juce_core.h>

namespace openguitarmultifx
{

class HoldProcessorTests : public juce::UnitTest
{
public:
    HoldProcessorTests() : juce::UnitTest ("HoldProcessor", "Effects") {}

    void runTest() override
    {
        beginTest ("not held: the signal passes through completely untouched");
        {
            HoldProcessor holdFx;
            holdFx.prepare (48000.0, 512, 1);

            juce::AudioBuffer<float> buffer (1, 1000);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, std::sin ((float) i * 0.1f));

            juce::AudioBuffer<float> original (buffer);
            holdFx.process (buffer);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                expectWithinAbsoluteError (buffer.getSample (0, i), original.getSample (0, i), 0.0001f);
        }

        beginTest ("engaging hold loops the just-captured window at the expected period");
        {
            HoldProcessor holdFx;

            auto state = holdFx.getState();
            state->setAttribute ("hold_capture", 50.0); // 50ms
            state->setAttribute ("hold_mix", 1.0);
            holdFx.setState (*state);

            holdFx.prepare (48000.0, 512, 1);

            const int captureSamples = (int) (0.05 * 48000.0); // 2400

            // Feed a 100ms ramp while NOT held, so the buffer has known,
            // distinguishable content. hold_capture (2400 samples) only
            // covers the second half of these 4800 samples once engaged.
            const int fillLength = captureSamples * 2;
            juce::AudioBuffer<float> fillBuffer (1, fillLength);
            for (int i = 0; i < fillLength; ++i)
                fillBuffer.setSample (0, i, (float) i / (float) fillLength);
            holdFx.process (fillBuffer);

            // Engage hold, then feed silence -- with mix=1.0, the output is
            // purely the frozen loop.
            state = holdFx.getState();
            state->setAttribute ("hold_engage", 1.0);
            holdFx.setState (*state);

            juce::AudioBuffer<float> heldBuffer (1, captureSamples * 2 + 50);
            heldBuffer.clear();
            holdFx.process (heldBuffer);

            // The frozen window is buffer indices [2400, 4799] of fillBuffer
            // (the last captureSamples written before engaging), so the
            // loop's first sample should equal fillBuffer[2400] = 0.5, and
            // the loop should repeat with period captureSamples.
            expectWithinAbsoluteError (heldBuffer.getSample (0, 0), 0.5f, 0.001f);
            for (int k = 0; k < captureSamples; k += 137) // sparse sample, not exhaustive
                expectWithinAbsoluteError (heldBuffer.getSample (0, k), heldBuffer.getSample (0, k + captureSamples), 0.0001f);
        }

        beginTest ("repeated blocks with hold toggling never produce NaN/Inf or runaway output");
        {
            HoldProcessor holdFx;
            holdFx.prepare (48000.0, 512, 2);

            juce::Random random (222324);
            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < 200; ++block)
            {
                if (block % 17 == 0)
                {
                    auto state = holdFx.getState();
                    state->setAttribute ("hold_engage", (block / 17) % 2 == 0 ? 1.0 : 0.0);
                    holdFx.setState (*state);
                }

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

                holdFx.process (buffer);

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

static HoldProcessorTests holdProcessorTests;

} // namespace openguitarmultifx
