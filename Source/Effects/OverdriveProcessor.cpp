#include "OverdriveProcessor.h"

#include <cmath>

namespace pedaleira
{

OverdriveProcessor::OverdriveProcessor()
{
    auto driveParam = std::make_unique<juce::AudioParameterFloat> (
        "od_drive", "Drive",
        juce::NormalisableRange<float> (1.0f, 40.0f, 0.0f, 0.5f), 6.0f);
    auto levelParam = std::make_unique<juce::AudioParameterFloat> (
        "od_level", "Level",
        juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f);

    drive = driveParam.get();
    levelDb = levelParam.get();

    parameters = std::make_unique<juce::AudioProcessorParameterGroup> (
        "overdrive", "Overdrive", "|",
        std::move (driveParam), std::move (levelParam));
}

void OverdriveProcessor::prepare (double, int, int)
{
}

void OverdriveProcessor::reset()
{
    // Stateless (no filters, no envelope) -- nothing to reset.
}

void OverdriveProcessor::process (juce::AudioBuffer<float>& buffer)
{
    const float driveAmount = drive->get();
    const float outputGain = juce::Decibels::decibelsToGain (levelDb->get());

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = std::tanh (data[i] * driveAmount) * outputGain;
    }
}

void OverdriveProcessor::drawIcon (juce::Graphics& g, juce::Rectangle<float> b) const
{
    // A soft-clipped waveform (~1.5 cycles, flattened peaks) -- see
    // docs/icons/AGENT-icon-notes.md, matched against the user's actual
    // reference sheet (docs/icons/reference-sheet.png): this is the unified
    // icon set's "Overdrive" glyph (Drive category).
    const auto pt = [&] (float u, float v)
    {
        return juce::Point<float> (b.getX() + u * b.getWidth(), b.getY() + v * b.getHeight());
    };

    juce::Path p;
    p.startNewSubPath (pt (0.1667f, 0.5f));
    p.cubicTo (pt (0.2708f, 0.1667f), pt (0.3542f, 0.1667f), pt (0.4167f, 0.5f));
    p.cubicTo (pt (0.4792f, 0.8333f), pt (0.5625f, 0.8333f), pt (0.6667f, 0.5f));
    p.cubicTo (pt (0.7708f, 0.1667f), pt (0.7708f, 0.1667f), pt (0.8333f, 0.5f));

    g.setColour (juce::Colours::white);
    g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace pedaleira
