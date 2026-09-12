#include "MultiTapDelayProcessor.h"
#include "IconKit.h"

#include <IconData.h>

#include <cmath>

namespace openguitarmultifx
{

MultiTapDelayProcessor::MultiTapDelayProcessor()
{
    auto time = std::make_unique<juce::AudioParameterFloat> (
        "multitap_time", "Time",
        juce::NormalisableRange<float> (1.0f, maxDelayMs, 0.0f, 0.4f), 400.0f);
    auto fb = std::make_unique<juce::AudioParameterFloat> (
        "multitap_feedback", "Feedback",
        juce::NormalisableRange<float> (0.0f, 0.9f), 0.25f);
    auto mixParam = std::make_unique<juce::AudioParameterFloat> (
        "multitap_mix", "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.35f);

    timeMs = time.get();
    feedback = fb.get();
    mix = mixParam.get();

    parameters = std::make_unique<juce::AudioProcessorParameterGroup> (
        "multitap", "Multi Tap", "|",
        std::move (time), std::move (fb), std::move (mixParam));
}

void MultiTapDelayProcessor::prepare (double sampleRate, int, int numChannels)
{
    currentSampleRate = sampleRate;

    const int bufferLength = (int) std::ceil (maxDelayMs * 0.001 * sampleRate) + 4;
    delayBuffer.setSize (juce::jmax (1, numChannels), bufferLength, false, true, true);

    smoothedDelaySamples.reset (sampleRate, 0.03);
    smoothedFeedback.reset (sampleRate, 0.03);
    smoothedMix.reset (sampleRate, 0.03);

    smoothedDelaySamples.setCurrentAndTargetValue ((float) (timeMs->get() * 0.001 * sampleRate));
    smoothedFeedback.setCurrentAndTargetValue (feedback->get());
    smoothedMix.setCurrentAndTargetValue (mix->get());

    reset();
}

void MultiTapDelayProcessor::reset()
{
    delayBuffer.clear();
    writePos = 0;
}

void MultiTapDelayProcessor::process (juce::AudioBuffer<float>& buffer)
{
    if (currentSampleRate <= 0.0 || delayBuffer.getNumSamples() == 0)
        return;

    smoothedDelaySamples.setTargetValue ((float) (timeMs->get() * 0.001 * currentSampleRate));
    smoothedFeedback.setTargetValue (feedback->get());
    smoothedMix.setTargetValue (mix->get());

    const int numChannels = juce::jmin (buffer.getNumChannels(), delayBuffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    const int bufferLength = delayBuffer.getNumSamples();

    // Normalised so the taps' combined level never exceeds unity even
    // though all 4 are audible together -- otherwise a hot input would
    // clip as soon as every tap lines up at once.
    constexpr float totalTapLevel = tapLevels[0] + tapLevels[1] + tapLevels[2] + tapLevels[3];

    for (int i = 0; i < numSamples; ++i)
    {
        const float baseDelaySamples = smoothedDelaySamples.getNextValue();
        const float fb = smoothedFeedback.getNextValue();
        const float wet = smoothedMix.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto* delayData = delayBuffer.getWritePointer (ch);
            const float input = data[i];

            float tapSum = 0.0f;
            float longestTapDelayed = 0.0f;

            for (int t = 0; t < numTaps; ++t)
            {
                const float delaySamples = baseDelaySamples * tapRatios[(size_t) t];

                float readPos = (float) writePos - delaySamples;
                while (readPos < 0.0f)
                    readPos += (float) bufferLength;

                const int readIndex0 = (int) readPos;
                const int readIndex1 = (readIndex0 + 1) % bufferLength;
                const float frac = readPos - (float) readIndex0;

                const float delayed = delayData[readIndex0] + frac * (delayData[readIndex1] - delayData[readIndex0]);
                tapSum += delayed * tapLevels[(size_t) t];

                if (t == numTaps - 1)
                    longestTapDelayed = delayed;
            }

            delayData[writePos] = input + fb * longestTapDelayed;
            data[i] = input * (1.0f - wet) + (tapSum / totalTapLevel) * wet;
        }

        writePos = (writePos + 1) % bufferLength;
    }
}

void MultiTapDelayProcessor::drawIcon (juce::Graphics& g, juce::Rectangle<float> b) const
{
    // See docs/icons/AGENT-icon-notes.md / Assets/Icons/multi_tap.svg --
    // one line with several branching taps of decreasing height.
    static const std::unique_ptr<juce::Drawable> svg =
        icon::loadSvg (IconData::multi_tap_svg, IconData::multi_tap_svgSize);
    icon::drawSvg (g, b, svg.get());
}

} // namespace openguitarmultifx
