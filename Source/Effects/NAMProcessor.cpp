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
    // A chip -- see docs/icons/AGENT-icon-notes.md: this is the unified
    // icon set's "Neura Amp"/"Neura Pedal" glyph. Deliberately the SAME
    // glyph for both roles (that's what the reference sheet itself does --
    // the category colour and block label are what tell them apart, not
    // the icon), a small IC outline with a centre dot and pin stubs on all
    // four sides standing in for "this is the neural model block".
    g.setColour (juce::Colours::white);

    auto chip = b.reduced (b.getWidth() * 0.2f, b.getHeight() * 0.2f);
    g.drawRoundedRectangle (chip, 2.0f, 2.0f);
    g.fillEllipse (chip.getCentreX() - 3.0f, chip.getCentreY() - 3.0f, 6.0f, 6.0f);

    for (int i = -1; i <= 1; i += 2)
    {
        const float x = chip.getCentreX() + (float) i * chip.getWidth() * 0.28f;
        g.drawLine (x, b.getY(), x, chip.getY(), 1.6f);
        g.drawLine (x, chip.getBottom(), x, b.getBottom(), 1.6f);

        const float y = chip.getCentreY() + (float) i * chip.getHeight() * 0.28f;
        g.drawLine (b.getX(), y, chip.getX(), y, 1.6f);
        g.drawLine (chip.getRight(), y, b.getRight(), y, 1.6f);
    }
}

} // namespace pedaleira
