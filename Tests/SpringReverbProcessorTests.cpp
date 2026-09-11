#include "Effects/SpringReverbProcessor.h"

#include <juce_core/juce_core.h>

namespace pedaleira
{

class SpringReverbProcessorTests : public juce::UnitTest
{
public:
    SpringReverbProcessorTests() : juce::UnitTest ("SpringReverbProcessor", "Effects") {}

    static float tailEnergyAfterImpulse (SpringReverbProcessor& spring, int blocksToMeasure)
    {
        juce::AudioBuffer<float> dryBlock (1, 512);
        dryBlock.clear();
        dryBlock.setSample (0, 0, 1.0f);
        spring.process (dryBlock);

        float energy = 0.0f;
        juce::AudioBuffer<float> tailBlock (1, 512);
        for (int block = 0; block < blocksToMeasure; ++block)
        {
            tailBlock.clear();
            spring.process (tailBlock);
            for (int i = 0; i < tailBlock.getNumSamples(); ++i)
                energy += std::abs (tailBlock.getSample (0, i));
        }
        return energy;
    }

    void runTest() override
    {
        beginTest ("an impulse leaves a ringing tail, not silence, after the dry sample");
        {
            SpringReverbProcessor spring;
            spring.prepare (48000.0, 512, 1);

            const float energy = tailEnergyAfterImpulse (spring, 4);
            expect (energy > 0.001f);
        }

        beginTest ("higher decay produces a longer/louder tail than lower decay");
        {
            // Same fixed test seed for both instances (deterministic construction --
            // no RNG involved), only the decay parameter differs via setState().
            SpringReverbProcessor lowDecaySpring;
            lowDecaySpring.prepare (48000.0, 512, 1);
            auto lowState = lowDecaySpring.getState();
            lowState->setAttribute ("spring_decay", 0.2);
            lowDecaySpring.setState (*lowState);

            SpringReverbProcessor highDecaySpring;
            highDecaySpring.prepare (48000.0, 512, 1);
            auto highState = highDecaySpring.getState();
            highState->setAttribute ("spring_decay", 0.9);
            highDecaySpring.setState (*highState);

            const float lowEnergy = tailEnergyAfterImpulse (lowDecaySpring, 8);
            const float highEnergy = tailEnergyAfterImpulse (highDecaySpring, 8);

            expect (highEnergy > lowEnergy);
        }

        beginTest ("repeated blocks of a bounded signal never produce NaN/Inf or runaway output");
        {
            SpringReverbProcessor spring;
            spring.prepare (48000.0, 512, 2);

            juce::Random random (121314);
            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < 200; ++block)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

                spring.process (buffer);

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

static SpringReverbProcessorTests springReverbProcessorTests;

} // namespace pedaleira
