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
    // A sine that flattens at the peaks -- soft clipping, drawn.
    const float midY = b.getCentreY();
    const float clip = b.getHeight() * 0.15f;

    juce::Path p;
    p.startNewSubPath (b.getX(), midY);
    p.lineTo (b.getX() + b.getWidth() * 0.15f, midY - b.getHeight() * 0.35f);
    p.lineTo (b.getX() + b.getWidth() * 0.35f, midY - clip);
    p.lineTo (b.getX() + b.getWidth() * 0.5f, midY - clip);
    p.lineTo (b.getX() + b.getWidth() * 0.65f, midY + clip);
    p.lineTo (b.getX() + b.getWidth() * 0.85f, midY + b.getHeight() * 0.35f);
    p.lineTo (b.getRight(), midY);

    g.setColour (juce::Colours::white);
    g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace pedaleira
