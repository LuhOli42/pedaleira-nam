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
    // See docs/icons/AGENT-icon-notes.md. Mirrors the plain (non-neural)
    // Amp/Amp+Cab glyph shapes from the reference sheet per user request --
    // NOT the same glyph for every role (an earlier version wrongly used
    // one "chip" icon for all of them): Amp is a head with control knobs,
    // Amp+Cab stacks that head over a cab, Pedal is its own stompbox shape.
    g.setColour (juce::Colours::white);

    if (isPedalRole())
    {
        // A stompbox, viewed from above: narrower at the top, a footswitch
        // dot near the bottom -- reads as "pedal", not "amp head".
        auto box = b.reduced (b.getWidth() * 0.16f, b.getHeight() * 0.08f);
        juce::Path pedal;
        pedal.startNewSubPath (box.getX() + box.getWidth() * 0.15f, box.getY());
        pedal.lineTo (box.getRight() - box.getWidth() * 0.15f, box.getY());
        pedal.lineTo (box.getRight(), box.getBottom());
        pedal.lineTo (box.getX(), box.getBottom());
        pedal.closeSubPath();
        g.strokePath (pedal, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawEllipse (box.getCentreX() - 4.0f, box.getBottom() - box.getHeight() * 0.32f, 8.0f, 8.0f, 1.8f);
        return;
    }

    // Amp head: a box with a row of control knobs along the top.
    auto ampBox = isAmpCabRole() ? b.withHeight (b.getHeight() * 0.5f).withY (b.getY())
                                  : b.reduced (0.0f, b.getHeight() * 0.14f);
    g.drawRoundedRectangle (ampBox, 2.0f, 1.8f);
    for (int i = -1; i <= 1; ++i)
    {
        const float x = ampBox.getCentreX() + (float) i * ampBox.getWidth() * 0.26f;
        g.drawEllipse (x - 2.2f, ampBox.getY() + ampBox.getHeight() * 0.32f, 4.4f, 4.4f, 1.4f);
    }

    if (isAmpCabRole())
    {
        // Cab underneath: a box with a 2x2 speaker-grille dot pattern.
        auto cabBox = b.withY (ampBox.getBottom() + b.getHeight() * 0.08f)
                       .withHeight (b.getBottom() - (ampBox.getBottom() + b.getHeight() * 0.08f));
        g.drawRoundedRectangle (cabBox, 2.0f, 1.8f);
        for (int gx = -1; gx <= 1; gx += 2)
            for (int gy = -1; gy <= 1; gy += 2)
            {
                const float x = cabBox.getCentreX() + (float) gx * cabBox.getWidth() * 0.22f;
                const float y = cabBox.getCentreY() + (float) gy * cabBox.getHeight() * 0.24f;
                g.fillEllipse (x - 1.8f, y - 1.8f, 3.6f, 3.6f);
            }
    }
}

} // namespace pedaleira
