#include "ReverbProcessor.h"
#include "IconKit.h"

#include <IconData.h>

namespace pedaleira
{

ReverbProcessor::ReverbProcessor()
{
    auto sizeParam = std::make_unique<juce::AudioParameterFloat> (
        "ambient_size", "Size", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f);
    auto dampingParam = std::make_unique<juce::AudioParameterFloat> (
        "ambient_damping", "Damping", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f);
    auto mixParam = std::make_unique<juce::AudioParameterFloat> (
        "ambient_mix", "Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f);
    auto widthParam = std::make_unique<juce::AudioParameterFloat> (
        "ambient_width", "Width", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f);

    size = sizeParam.get();
    damping = dampingParam.get();
    mix = mixParam.get();
    width = widthParam.get();

    parameters = std::make_unique<juce::AudioProcessorParameterGroup> (
        "ambient", "Ambient", "|",
        std::move (sizeParam), std::move (dampingParam), std::move (mixParam), std::move (widthParam));
}

void ReverbProcessor::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, (juce::uint32) juce::jmax (1, numChannels) };
    reverb.prepare (spec);
    updateReverbParameters();
    reset();
}

void ReverbProcessor::reset()
{
    reverb.reset();
}

void ReverbProcessor::updateReverbParameters()
{
    juce::dsp::Reverb::Parameters params;
    params.roomSize = size->get();
    params.damping = damping->get();
    params.wetLevel = mix->get();
    params.dryLevel = 1.0f - mix->get();
    params.width = width->get();
    reverb.setParameters (params);
}

void ReverbProcessor::process (juce::AudioBuffer<float>& buffer)
{
    updateReverbParameters(); // cheap struct assignment, no allocation -- safe every block

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    reverb.process (context);
}

void ReverbProcessor::drawIcon (juce::Graphics& g, juce::Rectangle<float> b) const
{
    // See docs/icons/AGENT-icon-notes.md / Assets/Icons/ambient.svg --
    // the unified icon set's "Ambient" glyph (Reverb category).
    static const std::unique_ptr<juce::Drawable> svg = icon::loadSvg (IconData::ambient_svg, IconData::ambient_svgSize);
    icon::drawSvg (g, b, svg.get());
}

} // namespace pedaleira
