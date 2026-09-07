#include "Effects/CompressorProcessor.h"

#include <juce_core/juce_core.h>

namespace pedaleira
{

class CompressorProcessorTests : public juce::UnitTest
{
public:
    CompressorProcessorTests() : juce::UnitTest ("CompressorProcessor", "Effects") {}

    void runTest() override
    {
        beginTest ("a signal below threshold is left unchanged once settled");
        {
            CompressorProcessor comp;
            comp.prepare (48000.0, 512, 1);

            // threshold defaults to -18dB; feed a -30dB signal.
            const float amplitude = juce::Decibels::decibelsToGain (-30.0f);
            juce::AudioBuffer<float> buffer (1, 4096);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, amplitude);

            comp.process (buffer);

            expectWithinAbsoluteError (buffer.getSample (0, buffer.getNumSamples() - 1), amplitude, amplitude * 0.05f);
        }

        beginTest ("a signal above threshold is reduced by roughly the static ratio");
        {
            CompressorProcessor comp;
            comp.prepare (48000.0, 512, 1);

            // threshold -18dB, ratio 4:1 by default. Feed 0dB (amplitude 1.0):
            // expected steady-state gain reduction = -(0 - -18) * (1 - 1/4) = -13.5dB.
            juce::AudioBuffer<float> buffer (1, 8192);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 1.0f);

            comp.process (buffer);

            const float outputDb = juce::Decibels::gainToDecibels (
                std::abs (buffer.getSample (0, buffer.getNumSamples() - 1)));

            expectWithinAbsoluteError (outputDb, -13.5f, 1.0f);
        }
    }
};

static CompressorProcessorTests compressorProcessorTests;

} // namespace pedaleira
