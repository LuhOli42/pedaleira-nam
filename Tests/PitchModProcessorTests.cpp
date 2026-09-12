#include "Effects/PitchModProcessor.h"

#include <juce_core/juce_core.h>

namespace openguitarmultifx
{

class PitchModProcessorTests : public juce::UnitTest
{
public:
    PitchModProcessorTests() : juce::UnitTest ("PitchModProcessor", "Effects") {}

    void runTest() override
    {
        beginTest ("at zero depth (ratio always 1), the shifter degenerates to a fixed delay -- an impulse's echo lands at half a grain length");
        {
            PitchModProcessor pitchMod;
            pitchMod.prepare (48000.0, 512, 1);
            auto state = pitchMod.getState();
            state->setAttribute ("pitchmod_depth", 0.0);
            state->setAttribute ("pitchmod_mix", 1.0);
            pitchMod.setState (*state);

            // Grain is 50ms; at ratio=1 the two read pointers freeze at
            // their starting offsets (grain*0.5 and 0), and the one at
            // grain*0.5 sits at its window's peak (gain 1) forever while
            // the one at 0 sits at its window's zero -- so the whole
            // shifter reduces to a plain fixed delay of grain*0.5.
            const int expectedDelay = (int) (0.025 * 48000.0); // grain(50ms)/2
            juce::AudioBuffer<float> buffer (1, expectedDelay + 400);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);
            pitchMod.process (buffer);

            float peak = 0.0f;
            int peakIndex = 0;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float mag = std::abs (buffer.getSample (0, i));
                if (mag > peak) { peak = mag; peakIndex = i; }
            }

            expectWithinAbsoluteError (peakIndex, expectedDelay, 4);
            expect (peak > 0.5f);
        }

        beginTest ("repeated blocks of a bounded signal never produce NaN/Inf or runaway output, even at maximum depth");
        {
            PitchModProcessor pitchMod;
            pitchMod.prepare (48000.0, 512, 2);
            auto state = pitchMod.getState();
            state->setAttribute ("pitchmod_depth", 1.0);
            state->setAttribute ("pitchmod_rate", 8.0);
            pitchMod.setState (*state);

            juce::Random random (727374);
            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < 200; ++block)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

                pitchMod.process (buffer);

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

static PitchModProcessorTests pitchModProcessorTests;

} // namespace openguitarmultifx
