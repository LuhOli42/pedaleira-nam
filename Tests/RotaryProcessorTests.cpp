#include "Effects/RotaryProcessor.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <vector>

namespace openguitarmultifx
{

class RotaryProcessorTests : public juce::UnitTest
{
public:
    RotaryProcessorTests() : juce::UnitTest ("RotaryProcessor", "Effects") {}

    // Splits the buffer into sub-windows and returns each window's peak
    // absolute value -- a tone's own oscillation makes raw min/max
    // meaningless, but the AM envelope shows up clearly as the peak
    // rising and falling from one sub-window to the next.
    static std::vector<float> subWindowPeaks (const juce::AudioBuffer<float>& buffer, int numWindows)
    {
        std::vector<float> peaks ((size_t) numWindows, 0.0f);
        const int windowSize = buffer.getNumSamples() / numWindows;
        for (int w = 0; w < numWindows; ++w)
        {
            float peak = 0.0f;
            for (int i = w * windowSize; i < (w + 1) * windowSize; ++i)
                peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));
            peaks[(size_t) w] = peak;
        }
        return peaks;
    }

    static juce::AudioBuffer<float> renderTone (RotaryProcessor& rotary, double sampleRate, double toneHz, int numSamples)
    {
        juce::AudioBuffer<float> buffer (1, numSamples);
        double phase = 0.0;
        const double phaseInc = juce::MathConstants<double>::twoPi * toneHz / sampleRate;
        for (int i = 0; i < numSamples; ++i)
        {
            buffer.setSample (0, i, (float) std::sin (phase));
            phase += phaseInc;
        }
        rotary.process (buffer);
        return buffer;
    }

    void runTest() override
    {
        beginTest ("zero depth reconstructs the input exactly (the crossover bands always sum back to it)");
        {
            RotaryProcessor rotary;
            rotary.prepare (48000.0, 512, 1);
            auto state = rotary.getState();
            state->setAttribute ("rotary_depth", 0.0);
            rotary.setState (*state);

            const auto buffer = renderTone (rotary, 48000.0, 300.0, 4800);

            // Re-render the same tone dry to compare against, since the
            // crossover filter itself still shapes phase slightly.
            juce::AudioBuffer<float> dry (1, 4800);
            double phase = 0.0;
            const double phaseInc = juce::MathConstants<double>::twoPi * 300.0 / 48000.0;
            for (int i = 0; i < 4800; ++i)
            {
                dry.setSample (0, i, (float) std::sin (phase));
                phase += phaseInc;
            }

            float maxDiff = 0.0f;
            for (int i = 0; i < 4800; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (buffer.getSample (0, i) - dry.getSample (0, i)));

            expect (maxDiff < 1.0e-4f);
        }

        beginTest ("at full depth, a high tone (mostly the horn band) swings from near-silent to near-full over one horn period");
        {
            RotaryProcessor rotary;
            rotary.prepare (48000.0, 512, 1);
            auto state = rotary.getState();
            state->setAttribute ("rotary_depth", 1.0);
            state->setAttribute ("rotary_rate", 2.0); // horn period = 1/(2*1.6) = 0.3125s
            state->setAttribute ("rotary_mix", 1.0);
            rotary.setState (*state);

            const int hornPeriodSamples = (int) (48000.0 / (2.0 * 1.6));
            const auto buffer = renderTone (rotary, 48000.0, 4000.0, hornPeriodSamples);
            const auto peaks = subWindowPeaks (buffer, 20);

            const float maxPeak = *std::max_element (peaks.begin(), peaks.end());
            const float minPeak = *std::min_element (peaks.begin(), peaks.end());

            expect (maxPeak > 0.7f);
            expect (minPeak < 0.15f);
        }

        beginTest ("at full depth, a low tone (mostly the woofer band) swings from near-silent to near-full over one woofer period");
        {
            RotaryProcessor rotary;
            rotary.prepare (48000.0, 512, 1);
            auto state = rotary.getState();
            state->setAttribute ("rotary_depth", 1.0);
            state->setAttribute ("rotary_rate", 2.0); // woofer period = 1/(2*0.8) = 0.625s
            state->setAttribute ("rotary_mix", 1.0);
            rotary.setState (*state);

            const int woofPeriodSamples = (int) (48000.0 / (2.0 * 0.8));
            const auto buffer = renderTone (rotary, 48000.0, 100.0, woofPeriodSamples);
            const auto peaks = subWindowPeaks (buffer, 20);

            const float maxPeak = *std::max_element (peaks.begin(), peaks.end());
            const float minPeak = *std::min_element (peaks.begin(), peaks.end());

            expect (maxPeak > 0.7f);
            expect (minPeak < 0.15f);
        }

        beginTest ("repeated blocks of a bounded signal never produce NaN/Inf or runaway output");
        {
            RotaryProcessor rotary;
            rotary.prepare (48000.0, 512, 2);

            juce::Random random (636465);
            juce::AudioBuffer<float> buffer (2, 512);

            for (int block = 0; block < 200; ++block)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                        buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

                rotary.process (buffer);

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

static RotaryProcessorTests rotaryProcessorTests;

} // namespace openguitarmultifx
