#include "Effects/ShimmerReverbProcessor.h"

#include <juce_core/juce_core.h>

namespace openguitarmultifx
{

class ShimmerReverbProcessorTests : public juce::UnitTest
{
public:
    ShimmerReverbProcessorTests() : juce::UnitTest ("ShimmerReverbProcessor", "Effects") {}

    static float tailEnergyAfterImpulse (ShimmerReverbProcessor& shimmer, int blocksToMeasure)
    {
        juce::AudioBuffer<float> dryBlock (1, 512);
        dryBlock.clear();
        dryBlock.setSample (0, 0, 1.0f);
        shimmer.process (dryBlock);

        float energy = 0.0f;
        juce::AudioBuffer<float> tailBlock (1, 512);
        for (int block = 0; block < blocksToMeasure; ++block)
        {
            tailBlock.clear();
            shimmer.process (tailBlock);
            for (int i = 0; i < tailBlock.getNumSamples(); ++i)
                energy += std::abs (tailBlock.getSample (0, i));
        }
        return energy;
    }

    /** Sums energy only over blocks [lateStartBlock, totalBlocks) -- shimmer
        redirects early energy through the pitch shifter (a net loss right
        after the transient, since it replaces rather than adds to the
        normal feedback -- see CombFilter::process's doc comment), so a
        cumulative-from-the-start measure actually favours no-shimmer. The
        difference shimmer actually makes is a livelier LATE tail. */
    static float lateTailEnergyAfterImpulse (ShimmerReverbProcessor& shimmer, int totalBlocks, int lateStartBlock)
    {
        juce::AudioBuffer<float> dryBlock (1, 512);
        dryBlock.clear();
        dryBlock.setSample (0, 0, 1.0f);
        shimmer.process (dryBlock);

        float energy = 0.0f;
        juce::AudioBuffer<float> tailBlock (1, 512);
        for (int block = 0; block < totalBlocks; ++block)
        {
            tailBlock.clear();
            shimmer.process (tailBlock);
            if (block >= lateStartBlock)
                for (int i = 0; i < tailBlock.getNumSamples(); ++i)
                    energy += std::abs (tailBlock.getSample (0, i));
        }
        return energy;
    }

    void runTest() override
    {
        beginTest ("an impulse leaves a ringing tail, not silence, after the dry sample");
        {
            ShimmerReverbProcessor shimmer;
            shimmer.prepare (48000.0, 512, 1);

            const float energy = tailEnergyAfterImpulse (shimmer, 4);
            expect (energy > 0.001f);
        }

        beginTest ("more shimmer keeps the tail alive longer (octave-up energy re-injected into the loop)");
        {
            // With shimmer off, the tank's own decay applies once; with
            // shimmer on, every cycle also re-injects a fraction of a
            // pitched-up copy of itself, so a long-out tail should still
            // carry meaningfully more energy than with it off, at the
            // same comb decay setting.
            ShimmerReverbProcessor noShimmer;
            noShimmer.prepare (48000.0, 512, 1);
            auto noShimmerState = noShimmer.getState();
            noShimmerState->setAttribute ("shimmer_decay", 0.5);
            noShimmerState->setAttribute ("shimmer_amount", 0.0);
            noShimmer.setState (*noShimmerState);

            ShimmerReverbProcessor withShimmer;
            withShimmer.prepare (48000.0, 512, 1);
            auto withShimmerState = withShimmer.getState();
            withShimmerState->setAttribute ("shimmer_decay", 0.5);
            withShimmerState->setAttribute ("shimmer_amount", 0.9);
            withShimmer.setState (*withShimmerState);

            // Compare only the late window (blocks 40-59), well past the
            // point the un-shimmered tail has mostly died out.
            const float noShimmerLateEnergy = lateTailEnergyAfterImpulse (noShimmer, 60, 40);
            const float withShimmerLateEnergy = lateTailEnergyAfterImpulse (withShimmer, 60, 40);

            expect (withShimmerLateEnergy > noShimmerLateEnergy);
        }

        beginTest ("repeated blocks of a bounded signal never produce NaN/Inf or runaway output");
        {
            ShimmerReverbProcessor shimmer;
            shimmer.prepare (48000.0, 512, 2);
            auto state = shimmer.getState();
            state->setAttribute ("shimmer_amount", 1.0); // worst case for feedback-loop stability
            shimmer.setState (*state);

            juce::Random random (343536);
            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < 200; ++block)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

                shimmer.process (buffer);

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

static ShimmerReverbProcessorTests shimmerReverbProcessorTests;

} // namespace openguitarmultifx
