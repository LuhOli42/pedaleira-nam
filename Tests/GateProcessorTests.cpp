#include "Effects/GateProcessor.h"

#include <juce_core/juce_core.h>

namespace pedaleira
{

class GateProcessorTests : public juce::UnitTest
{
public:
    GateProcessorTests() : juce::UnitTest ("GateProcessor", "Effects") {}

    void runTest() override
    {
        beginTest ("a loud sustained signal ends up passing through near unity");
        {
            GateProcessor gate;
            gate.prepare (48000.0, 512, 1);

            // gate_threshold defaults to -50dB; feed a loud 0.5-amplitude
            // signal for long enough (attack default 1ms) that it settles open.
            juce::AudioBuffer<float> buffer (1, 4096);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 0.5f);

            gate.process (buffer);

            expectWithinAbsoluteError (buffer.getSample (0, buffer.getNumSamples() - 1), 0.5f, 0.02f);
        }

        beginTest ("a quiet signal well below threshold gets substantially attenuated");
        {
            GateProcessor gate;
            gate.prepare (48000.0, 512, 1);

            // -50dB threshold; feed a -70dB signal for long enough to close.
            const float quietAmplitude = juce::Decibels::decibelsToGain (-70.0f);
            juce::AudioBuffer<float> buffer (1, 8192);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, quietAmplitude);

            gate.process (buffer);

            const float outputLevel = std::abs (buffer.getSample (0, buffer.getNumSamples() - 1));
            expect (outputLevel < quietAmplitude * 0.5f);
        }
    }
};

static GateProcessorTests gateProcessorTests;

} // namespace pedaleira
