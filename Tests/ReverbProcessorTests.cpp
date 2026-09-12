#include "Effects/ReverbProcessor.h"

#include <juce_core/juce_core.h>

namespace openguitarmultifx
{

class ReverbProcessorTests : public juce::UnitTest
{
public:
    ReverbProcessorTests() : juce::UnitTest ("ReverbProcessor", "Effects") {}

    void runTest() override
    {
        beginTest ("an impulse leaves a decaying tail, not silence, after the dry sample");
        {
            ReverbProcessor reverb;
            reverb.prepare (48000.0, 512, 1);

            juce::AudioBuffer<float> buffer (1, 512);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);
            reverb.process (buffer);

            // A few blocks later, the algorithmic tail should still be audible --
            // proves the reverb is actually doing something, not passing silence.
            juce::AudioBuffer<float> tailBlock (1, 512);
            float tailEnergy = 0.0f;
            for (int block = 0; block < 4; ++block)
            {
                tailBlock.clear();
                reverb.process (tailBlock);
                for (int i = 0; i < tailBlock.getNumSamples(); ++i)
                    tailEnergy += std::abs (tailBlock.getSample (0, i));
            }

            expect (tailEnergy > 0.001f);
        }

        beginTest ("repeated blocks of a bounded signal never produce NaN/Inf or runaway output");
        {
            ReverbProcessor reverb;
            reverb.prepare (48000.0, 512, 2);

            juce::Random random (5678);
            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < 200; ++block)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

                reverb.process (buffer);

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

static ReverbProcessorTests reverbProcessorTests;

} // namespace openguitarmultifx
