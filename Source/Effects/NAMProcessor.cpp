#include "NAMProcessor.h"

#include <NAM/get_dsp.h>

namespace pedaleira
{

NAMProcessor::NAMProcessor (juce::String chainRoleName)
    : name (std::move (chainRoleName))
{
    auto input = std::make_unique<juce::AudioParameterFloat> (
        "nam_input", "Input", juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f);
    auto output = std::make_unique<juce::AudioParameterFloat> (
        "nam_output", "Output", juce::NormalisableRange<float> (-24.0f, 24.0f), 0.0f);

    inputGainDb = input.get();
    outputGainDb = output.get();

    parameters = std::make_unique<juce::AudioProcessorParameterGroup> (
        "nam", name, "|", std::move (input), std::move (output));

    startTimer (100); // sweeps modelSlot -- see DeferredReclaimer
}

NAMProcessor::~NAMProcessor() = default;

void NAMProcessor::loadModel (const std::filesystem::path& namFilePath)
{
    auto model = nam::get_dsp (namFilePath); // throws nam::NamFileValidationError on a bad file

    if (sampleRate > 0.0)
        model->Reset (sampleRate, preparedBlockSize);

    lastLoadedPath = namFilePath;
    modelSlot.publish (std::move (model));
}

juce::String NAMProcessor::getLoadedModelName() const
{
    return lastLoadedPath.empty() ? juce::String() : juce::String (lastLoadedPath.filename().string());
}

void NAMProcessor::clearModel()
{
    modelSlot.publish (nullptr);
    lastLoadedPath.clear();
}

void NAMProcessor::prepare (double newSampleRate, int maxBlockSize, int)
{
    sampleRate = newSampleRate;
    preparedBlockSize = maxBlockSize;

    inputScratch.assign ((size_t) maxBlockSize, 0.0f);
    outputScratch.assign ((size_t) maxBlockSize, 0.0f);

    // A sample-rate/block-size change invalidates an already-Reset() model.
    // Reloading builds a fresh instance and swaps it in atomically, rather
    // than mutating the live one (which the audio thread might be using).
    if (! lastLoadedPath.empty())
        loadModel (lastLoadedPath);
}

void NAMProcessor::reset()
{
    // nam::DSP's internal state (WaveNet/LSTM hidden state) isn't safe to
    // reach into from here without risking a race with the audio thread --
    // see prepare()'s reload-on-change comment for how state actually resets.
}

void NAMProcessor::process (juce::AudioBuffer<float>& buffer)
{
    auto* model = modelSlot.currentRaw();
    const int numSamples = buffer.getNumSamples();

    if (model == nullptr)
        return; // no model loaded -- pass through unchanged

    const float inGain = juce::Decibels::decibelsToGain (inputGainDb->get());
    const float outGain = juce::Decibels::decibelsToGain (outputGainDb->get());

    for (int i = 0; i < numSamples; ++i)
        inputScratch[(size_t) i] = buffer.getSample (0, i) * inGain;

    float* inPtrs[1] = { inputScratch.data() };
    float* outPtrs[1] = { outputScratch.data() };
    model->process (inPtrs, outPtrs, numSamples);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (ch, i, outputScratch[(size_t) i] * outGain);
}

void NAMProcessor::drawIcon (juce::Graphics& g, juce::Rectangle<float> b) const
{
    g.setColour (juce::Colours::white);

    if (isDriveRole())
    {
        // A lightning bolt -- the same visual language as "neural"/energy,
        // and it reads as clearly different from Overdrive's clipped wave.
        juce::Path bolt;
        bolt.startNewSubPath (b.getCentreX() + b.getWidth() * 0.12f, b.getY());
        bolt.lineTo (b.getX() + b.getWidth() * 0.28f, b.getCentreY() - 1.0f);
        bolt.lineTo (b.getCentreX(), b.getCentreY() - 1.0f);
        bolt.lineTo (b.getCentreX() - b.getWidth() * 0.12f, b.getBottom());
        bolt.lineTo (b.getRight() - b.getWidth() * 0.28f, b.getCentreY() + 1.0f);
        bolt.lineTo (b.getCentreX(), b.getCentreY() + 1.0f);
        bolt.closeSubPath();
        g.fillPath (bolt);
    }
    else
    {
        // A speaker cone -- two concentric rings and a centre cap.
        g.drawEllipse (b.reduced (b.getWidth() * 0.12f), 2.0f);
        g.drawEllipse (b.reduced (b.getWidth() * 0.32f), 2.0f);
        g.fillEllipse (b.getCentreX() - 3.5f, b.getCentreY() - 3.5f, 7.0f, 7.0f);
    }
}

} // namespace pedaleira
