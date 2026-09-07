#pragma once

#include "EffectProcessor.h"
#include "EnvelopeFollower.h"

namespace pedaleira
{

/**
    Simple noise gate: a fast internal level detector decides whether the
    input is above or below the threshold, and a second envelope follower
    (this one driven by the user-facing Attack/Release parameters) smooths
    the resulting 0/1 gate gain so opening and closing never clicks.
*/
class GateProcessor : public EffectProcessor
{
public:
    GateProcessor();

    void prepare (double sampleRate, int maxBlockSize, int numChannels) override;
    void process (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    juce::AudioProcessorParameterGroup* getParameters() override { return parameters.get(); }
    const char* getName() const override { return "Noise Gate"; }

private:
    std::unique_ptr<juce::AudioProcessorParameterGroup> parameters;
    juce::AudioParameterFloat* thresholdDb = nullptr;
    juce::AudioParameterFloat* attackMs = nullptr;
    juce::AudioParameterFloat* releaseMs = nullptr;

    EnvelopeFollower levelDetector; // fixed, fast timing -- not user-exposed
    EnvelopeFollower gainSmoother;  // timing == the Attack/Release parameters above
};

} // namespace pedaleira
