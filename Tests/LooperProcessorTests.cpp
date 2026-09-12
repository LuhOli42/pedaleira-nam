#include "Effects/LooperProcessor.h"

#include <juce_core/juce_core.h>

namespace openguitarmultifx
{

class LooperProcessorTests : public juce::UnitTest
{
public:
    LooperProcessorTests() : juce::UnitTest ("LooperProcessor", "Effects") {}

    static void press (LooperProcessor& looper, juce::AudioBuffer<float>& buffer)
    {
        // A "press" is a rising edge that lasts at least one whole
        // process() call, matching the once-per-call edge detection --
        // set the parameter high, process one (silent) block, then let
        // the next real block find it already high (no second edge).
        auto state = looper.getState();
        state->setAttribute ("looper_trigger", 1.0);
        looper.setState (*state);
        buffer.clear();
        looper.process (buffer);
    }

    static void release (LooperProcessor& looper, juce::AudioBuffer<float>& buffer)
    {
        auto state = looper.getState();
        state->setAttribute ("looper_trigger", 0.0);
        looper.setState (*state);
        buffer.clear();
        looper.process (buffer);
    }

    void runTest() override
    {
        beginTest ("idle: dry signal passes through completely unaffected");
        {
            LooperProcessor looper;
            looper.prepare (48000.0, 512, 1);

            juce::Random random (10203);
            juce::AudioBuffer<float> buffer (1, 512);
            juce::AudioBuffer<float> original (1, 512);
            for (int i = 0; i < 512; ++i)
            {
                const float v = random.nextFloat() * 2.0f - 1.0f;
                buffer.setSample (0, i, v);
                original.setSample (0, i, v);
            }

            looper.process (buffer);

            for (int i = 0; i < 512; ++i)
                expectWithinAbsoluteError (buffer.getSample (0, i), original.getSample (0, i), 1.0e-6f);

            expect (looper.getStatusText() == "Idle");
        }

        beginTest ("record then stop: the captured loop plays back on repeat, added under a now-silent dry signal");
        {
            LooperProcessor looper;
            looper.prepare (48000.0, 512, 1);

            juce::AudioBuffer<float> scratch (1, 512);
            press (looper, scratch); // idle -> recording
            expect (looper.getStatusText() == "Recording");
            release (looper, scratch);

            // Record several blocks of a constant tone -- comfortably past
            // the 0.05s (2400-sample) near-zero-length-loop guard, which
            // the press/release helpers above already ate into (each
            // consumes a full silent block while transitioning state).
            juce::AudioBuffer<float> recordBlock (1, 512);
            for (int i = 0; i < 512; ++i)
                recordBlock.setSample (0, i, 0.5f);
            for (int block = 0; block < 6; ++block)
                looper.process (recordBlock);

            press (looper, scratch); // recording -> playing (fixes the loop length)
            expect (looper.getStatusText() == "Playing");
            release (looper, scratch);

            // Now feed silence and expect the recorded loop to play back.
            juce::AudioBuffer<float> playback (1, 512);
            playback.clear();
            looper.process (playback);

            float energy = 0.0f;
            for (int i = 0; i < 512; ++i)
                energy += std::abs (playback.getSample (0, i));

            expect (energy > 1.0f);
        }

        beginTest ("clear resets to idle from any state, and the loop buffer is silenced");
        {
            LooperProcessor looper;
            looper.prepare (48000.0, 512, 1);

            juce::AudioBuffer<float> scratch (1, 512);
            press (looper, scratch);
            release (looper, scratch);

            juce::AudioBuffer<float> recordBlock (1, 512);
            for (int i = 0; i < 512; ++i)
                recordBlock.setSample (0, i, 0.5f);
            for (int block = 0; block < 6; ++block)
                looper.process (recordBlock);

            press (looper, scratch);
            release (looper, scratch);
            expect (looper.getStatusText() == "Playing");

            auto state = looper.getState();
            state->setAttribute ("looper_clear", 1.0);
            looper.setState (*state);
            juce::AudioBuffer<float> clearBlock (1, 512);
            clearBlock.clear();
            looper.process (clearBlock);

            expect (looper.getStatusText() == "Idle");

            state->setAttribute ("looper_clear", 0.0);
            looper.setState (*state);

            juce::AudioBuffer<float> afterClear (1, 512);
            afterClear.clear();
            looper.process (afterClear);

            float energy = 0.0f;
            for (int i = 0; i < 512; ++i)
                energy += std::abs (afterClear.getSample (0, i));

            expectWithinAbsoluteError (energy, 0.0f, 1.0e-6f);
        }

        beginTest ("repeated blocks of a bounded signal never produce NaN/Inf or runaway output through a full record/overdub cycle");
        {
            LooperProcessor looper;
            looper.prepare (48000.0, 512, 2);

            juce::Random random (40506);
            juce::AudioBuffer<float> buffer (2, 512);
            juce::AudioBuffer<float> scratch (2, 512);

            press (looper, scratch); // -> recording
            release (looper, scratch);

            for (int block = 0; block < 6; ++block)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                        buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);
                looper.process (buffer);
            }

            press (looper, scratch); // -> playing
            release (looper, scratch);
            press (looper, scratch); // -> overdubbing
            release (looper, scratch);

            for (int block = 0; block < 200; ++block)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                        buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

                looper.process (buffer);

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                    {
                        const float sample = buffer.getSample (ch, i);
                        expect (std::isfinite (sample));
                        expect (std::abs (sample) < 10.0f);
                    }
            }
        }
    }
};

static LooperProcessorTests looperProcessorTests;

} // namespace openguitarmultifx
