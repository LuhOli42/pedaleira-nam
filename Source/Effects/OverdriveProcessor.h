#pragma once

#include "EffectProcessor.h"

namespace pedaleira
{

/**
    Soft-clipping overdrive: input gain into tanh() waveshaping, then an
    output trim. tanh is odd-symmetric, so no DC blocking stage is needed
    for a symmetric drive -- kept out on purpose (see AGENT.md: don't add
    what isn't needed).
*/
class OverdriveProcessor : public EffectProcessor
{
public:
    OverdriveProcessor();

    void prepare (double sampleRate, int maxBlockSize, int numChannels) override;
    void process (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    juce::AudioProcessorParameterGroup* getParameters() override { return parameters.get(); }
    const char* getName() const override { return "Overdrive"; }
    juce::Colour getAccentColour() const override { return juce::Colour (0xffb8622a); }
    void drawIcon (juce::Graphics& g, juce::Rectangle<float> b) const override;

private:
    std::unique_ptr<juce::AudioProcessorParameterGroup> parameters;
    juce::AudioParameterFloat* drive = nullptr;
    juce::AudioParameterFloat* levelDb = nullptr;
};

} // namespace pedaleira
