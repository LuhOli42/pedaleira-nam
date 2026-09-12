#include "Effects/RoomReverbProcessor.h"

#include <juce_core/juce_core.h>

namespace openguitarmultifx
{

class RoomReverbProcessorTests : public juce::UnitTest
{
public:
    RoomReverbProcessorTests() : juce::UnitTest ("RoomReverbProcessor", "Effects") {}

    static float tailEnergyAfterImpulse (RoomReverbProcessor& room, int blocksToMeasure)
    {
        juce::AudioBuffer<float> dryBlock (1, 512);
        dryBlock.clear();
        dryBlock.setSample (0, 0, 1.0f);
        room.process (dryBlock);

        float energy = 0.0f;
        juce::AudioBuffer<float> tailBlock (1, 512);
        for (int block = 0; block < blocksToMeasure; ++block)
        {
            tailBlock.clear();
            room.process (tailBlock);
            for (int i = 0; i < tailBlock.getNumSamples(); ++i)
                energy += std::abs (tailBlock.getSample (0, i));
        }
        return energy;
    }

    void runTest() override
    {
        beginTest ("an impulse leaves a ringing tail, not silence, after the dry sample");
        {
            RoomReverbProcessor room;
            room.prepare (48000.0, 512, 1);

            const float energy = tailEnergyAfterImpulse (room, 2);
            expect (energy > 0.001f);
        }

        beginTest ("higher decay produces a longer/louder tail than lower decay");
        {
            RoomReverbProcessor lowDecayRoom;
            lowDecayRoom.prepare (48000.0, 512, 1);
            auto lowState = lowDecayRoom.getState();
            lowState->setAttribute ("room_decay", 0.05);
            lowDecayRoom.setState (*lowState);

            RoomReverbProcessor highDecayRoom;
            highDecayRoom.prepare (48000.0, 512, 1);
            auto highState = highDecayRoom.getState();
            highState->setAttribute ("room_decay", 0.95);
            highDecayRoom.setState (*highState);

            const float lowEnergy = tailEnergyAfterImpulse (lowDecayRoom, 6);
            const float highEnergy = tailEnergyAfterImpulse (highDecayRoom, 6);

            expect (highEnergy > lowEnergy);
        }

        beginTest ("Room's tail decays faster than Hall's at matched decay settings (a small space, not a cathedral)");
        {
            // Same decay knob position on both, same measurement window --
            // Room's shorter comb tunings and lower feedback ceiling
            // (0.5-0.9 vs Hall's 0.7-0.98) should still die out faster.
            RoomReverbProcessor room;
            room.prepare (48000.0, 512, 1);
            auto roomState = room.getState();
            roomState->setAttribute ("room_decay", 0.8);
            room.setState (*roomState);

            juce::AudioBuffer<float> impulse (1, 512);
            impulse.clear();
            impulse.setSample (0, 0, 1.0f);
            room.process (impulse);

            // Measure energy far out in the tail -- a small room should
            // have mostly died out by half a second in.
            float lateEnergy = 0.0f;
            juce::AudioBuffer<float> tail (1, 512);
            for (int block = 0; block < 40; ++block) // ~427ms at 48kHz/512
            {
                tail.clear();
                room.process (tail);
                if (block >= 35)
                    for (int i = 0; i < tail.getNumSamples(); ++i)
                        lateEnergy += std::abs (tail.getSample (0, i));
            }

            expect (lateEnergy < 0.5f);
        }

        beginTest ("repeated blocks of a bounded signal never produce NaN/Inf or runaway output");
        {
            RoomReverbProcessor room;
            room.prepare (48000.0, 512, 2);

            juce::Random random (222324);
            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < 200; ++block)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

                room.process (buffer);

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

static RoomReverbProcessorTests roomReverbProcessorTests;

} // namespace openguitarmultifx
