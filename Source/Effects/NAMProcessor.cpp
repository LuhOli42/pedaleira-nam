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

std::unique_ptr<juce::XmlElement> NAMProcessor::getState() const
{
    auto xml = EffectProcessor::getState(); // base: input/output gain

    if (! lastLoadedPath.empty())
        xml->setAttribute ("modelPath", juce::String (lastLoadedPath.string()));

    return xml;
}

void NAMProcessor::setState (const juce::XmlElement& state)
{
    EffectProcessor::setState (state); // base: input/output gain

    const auto path = state.getStringAttribute ("modelPath");
    if (path.isEmpty())
        return;

    try
    {
        loadModel (std::filesystem::path (path.toStdString()));
    }
    catch (const std::exception&)
    {
        // The file may have moved or been deleted since the preset was
        // saved -- leave this block unloaded rather than fail the whole
        // preset load over one missing model.
    }
}

namespace
{
    // The "NEURA AMP"/"NEURA PEDAL" glyph on the actual reference sheet
    // (docs/icons/reference-sheet.png, attached by the user 2026-09-11):
    // a chip/IC symbol -- outer square, a small circuit "face" (two dot
    // eyes + a curved smile) instead of a plain inner die, and one short
    // perpendicular pin tick on each of the four sides. The die-square
    // version was an earlier guess before the face detail was confirmed
    // against the real sheet -- see AGENT-icon-notes.md's history of this
    // glyph being guessed at more than once before this.
    void drawChipGlyph (juce::Graphics& g, juce::Rectangle<float> box)
    {
        g.drawRoundedRectangle (box, 2.0f, 1.8f);

        const auto c = box.getCentre();
        const float s = box.getWidth() * 0.5f;
        const float eyeR = s * 0.12f;

        g.fillEllipse (c.x - s * 0.35f - eyeR, c.y - s * 0.2f - eyeR, eyeR * 2.0f, eyeR * 2.0f);
        g.fillEllipse (c.x + s * 0.35f - eyeR, c.y - s * 0.2f - eyeR, eyeR * 2.0f, eyeR * 2.0f);

        juce::Path smile;
        smile.startNewSubPath (c.x - s * 0.35f, c.y + s * 0.3f);
        smile.quadraticTo (c.x, c.y + s * 0.55f, c.x + s * 0.35f, c.y + s * 0.3f);
        g.strokePath (smile, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float pinLength = box.getWidth() * 0.14f;

        g.drawLine (c.x, box.getY() - pinLength, c.x, box.getY(), 1.6f);            // top
        g.drawLine (c.x, box.getBottom(), c.x, box.getBottom() + pinLength, 1.6f);  // bottom
        g.drawLine (box.getX() - pinLength, c.y, box.getX(), c.y, 1.6f);            // left
        g.drawLine (box.getRight(), c.y, box.getRight() + pinLength, c.y, 1.6f);    // right
    }
}

void NAMProcessor::drawIcon (juce::Graphics& g, juce::Rectangle<float> b) const
{
    // See docs/icons/AGENT-icon-notes.md and drawChipGlyph()'s comment
    // above for where this shape comes from -- corrected 2026-09-11 against
    // the actual reference sheet (previously guessed at twice, wrongly,
    // without ever having seen the real image).
    g.setColour (juce::Colours::white);

    if (isPedalRole())
    {
        // Same chip glyph as Neural Amp -- the sheet draws NEURA PEDAL as a
        // chip too, not a distinct stompbox shape (an earlier guess before
        // the real sheet was seen assumed otherwise). Inset a bit more
        // since pins need room on all four sides within the tile.
        drawChipGlyph (g, b.reduced (b.getWidth() * 0.16f, b.getHeight() * 0.16f));
        return;
    }

    if (isAmpCabRole())
    {
        // Chip on top (compact), cab box with a 2x2 speaker-grille dot
        // pattern underneath -- matches the sheet's "NEURA AMP + CAB" tile.
        auto chipBox = b.withHeight (b.getHeight() * 0.42f).withY (b.getY())
                        .reduced (b.getWidth() * 0.2f, 0.0f);
        drawChipGlyph (g, chipBox);

        auto cabBox = b.withY (chipBox.getBottom() + b.getHeight() * 0.14f)
                       .withHeight (b.getBottom() - (chipBox.getBottom() + b.getHeight() * 0.14f));
        g.drawRoundedRectangle (cabBox, 2.0f, 1.8f);
        for (int gx = -1; gx <= 1; gx += 2)
            for (int gy = -1; gy <= 1; gy += 2)
            {
                const float x = cabBox.getCentreX() + (float) gx * cabBox.getWidth() * 0.22f;
                const float y = cabBox.getCentreY() + (float) gy * cabBox.getHeight() * 0.24f;
                g.fillEllipse (x - 1.8f, y - 1.8f, 3.6f, 3.6f);
            }
        return;
    }

    // Plain Neural Amp: just the chip, filling most of the tile.
    drawChipGlyph (g, b.reduced (b.getWidth() * 0.16f, b.getHeight() * 0.16f));
}

} // namespace pedaleira
