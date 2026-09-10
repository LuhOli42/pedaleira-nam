#include "IRLoaderProcessor.h"

namespace pedaleira
{

IRLoaderProcessor::IRLoaderProcessor (juce::String chainRoleName)
    : name (std::move (chainRoleName))
{
    auto mix = std::make_unique<juce::AudioParameterFloat> (
        "ir_mix", "Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f);
    auto output = std::make_unique<juce::AudioParameterFloat> (
        "ir_output", "Output", juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f);

    mixParam = mix.get();
    outputGainDb = output.get();

    parameters = std::make_unique<juce::AudioProcessorParameterGroup> (
        "ir", name, "|", std::move (mix), std::move (output));

    startTimer (100); // sweeps irSlot -- see DeferredReclaimer
}

IRLoaderProcessor::~IRLoaderProcessor() = default;

void IRLoaderProcessor::loadImpulseResponse (const juce::File& irFile)
{
    auto conv = std::make_unique<juce::dsp::Convolution>();

    if (sampleRate > 0.0)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) preparedBlockSize, (juce::uint32) preparedNumChannels };
        conv->prepare (spec);
    }

    conv->loadImpulseResponse (irFile,
                                juce::dsp::Convolution::Stereo::yes,
                                juce::dsp::Convolution::Trim::yes,
                                0, // 0 -- use the whole file
                                juce::dsp::Convolution::Normalise::yes);

    lastLoadedFile = irFile;
    loadedName = irFile.getFileName();
    irSlot.publish (std::move (conv));
}

void IRLoaderProcessor::clearImpulseResponse()
{
    irSlot.publish (nullptr);
    lastLoadedFile = juce::File();
    loadedName.clear();
}

void IRLoaderProcessor::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    sampleRate = newSampleRate;
    preparedBlockSize = maxBlockSize;
    preparedNumChannels = juce::jmax (1, numChannels);

    dryScratch.setSize (preparedNumChannels, maxBlockSize, false, false, true);

    // Same reasoning as NAMProcessor::prepare(): a sample-rate/block-size
    // change invalidates an already-prepared Convolution, so reloading
    // builds a fresh instance and swaps it in atomically rather than
    // mutating the live one.
    if (lastLoadedFile.existsAsFile())
        loadImpulseResponse (lastLoadedFile);
}

void IRLoaderProcessor::reset()
{
    // Convolution's internal tail state isn't safe to reach into from here
    // without risking a race with the audio thread -- see prepare()'s
    // reload-on-change comment for how state actually resets.
}

void IRLoaderProcessor::process (juce::AudioBuffer<float>& buffer)
{
    auto* conv = irSlot.currentRaw();

    if (conv == nullptr)
        return; // no IR loaded -- pass through unchanged

    const int numSamples = buffer.getNumSamples();
    const float mix = mixParam->get();
    const bool needsDryBlend = mix < 0.999f;

    if (needsDryBlend)
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            dryScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    conv->process (context);

    if (needsDryBlend)
    {
        buffer.applyGain (mix);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom (ch, 0, dryScratch, ch, 0, numSamples, 1.0f - mix);
    }

    const float outGain = juce::Decibels::decibelsToGain (outputGainDb->get());
    if (outGain != 1.0f)
        buffer.applyGain (outGain);
}

juce::Colour IRLoaderProcessor::getAccentColour() const
{
    return isReverbRole() ? juce::Colour (0xff2d7a9e) : juce::Colour (0xff7a5c2d);
}

void IRLoaderProcessor::drawIcon (juce::Graphics& g, juce::Rectangle<float> b) const
{
    g.setColour (juce::Colours::white);

    if (isReverbRole())
    {
        // Concentric arcs spreading outward -- a space, reflecting sound.
        for (int i = 0; i < 3; ++i)
        {
            const float inset = b.getWidth() * (0.1f + 0.22f * (float) i);
            juce::Path arc;
            arc.addCentredArc (b.getCentreX(), b.getBottom(), b.getWidth() * 0.5f - inset,
                                b.getWidth() * 0.5f - inset, 0.0f,
                                juce::MathConstants<float>::pi, juce::MathConstants<float>::pi * 2.0f, true);
            g.strokePath (arc, juce::PathStrokeType (2.0f));
        }
    }
    else
    {
        // A speaker cabinet: a rounded box with a cone circle -- distinct
        // from NAMProcessor's amp-role speaker (which has no box around it).
        auto box = b.reduced (b.getWidth() * 0.08f, 0.0f);
        g.drawRoundedRectangle (box, 3.0f, 2.0f);
        g.drawEllipse (box.reduced (box.getWidth() * 0.28f), 2.0f);
    }
}

} // namespace pedaleira
