#include "IRLoaderProcessor.h"

#include <cmath>

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
    // See docs/icons/AGENT-icon-notes.md for the unified icon set this
    // follows.
    g.setColour (juce::Colours::white);

    if (isReverbRole())
    {
        // Concentric rings -- the set's "Ambient" reverb glyph, the closest
        // generic match while this role covers every IR-based space in one
        // block (Hall/Plate/Room/... aren't separate blocks yet).
        for (int i = 0; i < 3; ++i)
        {
            const float inset = juce::jmin (b.getWidth(), b.getHeight()) * (0.08f + 0.16f * (float) i);
            g.drawEllipse (b.reduced (inset), 1.8f);
        }
        g.fillEllipse (b.getCentreX() - 2.5f, b.getCentreY() - 2.5f, 5.0f, 5.0f);
    }
    else
    {
        // An isometric cube -- the set's "Cab" glyph.
        const auto c = b.getCentre();
        const float s = juce::jmin (b.getWidth(), b.getHeight()) * 0.42f;

        juce::Point<float> pts[6];
        for (int i = 0; i < 6; ++i)
        {
            const float angle = juce::MathConstants<float>::pi * (-0.5f + (float) i / 3.0f);
            pts[i] = { c.x + s * std::cos (angle), c.y + s * std::sin (angle) };
        }

        juce::Path cube;
        cube.startNewSubPath (pts[0]);
        for (int i = 1; i < 6; ++i)
            cube.lineTo (pts[i]);
        cube.closeSubPath();

        cube.startNewSubPath (c); cube.lineTo (pts[0]);
        cube.startNewSubPath (c); cube.lineTo (pts[2]);
        cube.startNewSubPath (c); cube.lineTo (pts[4]);

        g.strokePath (cube, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

} // namespace pedaleira
