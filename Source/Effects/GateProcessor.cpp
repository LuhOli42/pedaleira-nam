#include "GateProcessor.h"
#include "IconKit.h"

#include <IconData.h>

namespace openguitarmultifx
{

GateProcessor::GateProcessor()
{
    auto threshold = std::make_unique<juce::AudioParameterFloat> (
        "gate_threshold", "Threshold",
        juce::NormalisableRange<float> (-80.0f, 0.0f), -50.0f);
    auto attack = std::make_unique<juce::AudioParameterFloat> (
        "gate_attack", "Attack",
        juce::NormalisableRange<float> (0.1f, 50.0f, 0.0f, 0.4f), 1.0f);
    auto release = std::make_unique<juce::AudioParameterFloat> (
        "gate_release", "Release",
        juce::NormalisableRange<float> (10.0f, 1000.0f, 0.0f, 0.4f), 100.0f);

    thresholdDb = threshold.get();
    attackMs = attack.get();
    releaseMs = release.get();

    parameters = std::make_unique<juce::AudioProcessorParameterGroup> (
        "gate", "Noise Gate", "|",
        std::move (threshold), std::move (attack), std::move (release));
}

void GateProcessor::prepare (double sampleRate, int, int)
{
    levelDetector.prepare (sampleRate);
    levelDetector.setAttackTime (2.0f);
    levelDetector.setReleaseTime (2.0f);

    gainSmoother.prepare (sampleRate);
    reset();
}

void GateProcessor::reset()
{
    levelDetector.reset();
    gainSmoother.reset(); // gate starts closed -- silent until the first transient opens it
}

void GateProcessor::process (juce::AudioBuffer<float>& buffer)
{
    gainSmoother.setAttackTime (attackMs->get());
    gainSmoother.setReleaseTime (releaseMs->get());

    const float thresholdLinear = juce::Decibels::decibelsToGain (thresholdDb->get());
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

        const float detectedLevel = levelDetector.processSample (peak);
        const float targetGain = (detectedLevel >= thresholdLinear) ? 1.0f : 0.0f;
        const float gateGain = gainSmoother.processSample (targetGain);

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * gateGain);
    }
}

void GateProcessor::drawIcon (juce::Graphics& g, juce::Rectangle<float> b) const
{
    // See docs/icons/AGENT-icon-notes.md / Assets/Icons/gate.svg -- the
    // unified icon set's "Noise Gate" glyph (Dinamica category).
    static const std::unique_ptr<juce::Drawable> svg = icon::loadSvg (IconData::gate_svg, IconData::gate_svgSize);
    icon::drawSvg (g, b, svg.get());
}

} // namespace openguitarmultifx
