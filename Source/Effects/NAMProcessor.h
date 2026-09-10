#pragma once

#include "EffectProcessor.h"
#include "../Engine/DeferredReclaimer.h"

#include <NAM/dsp.h>

#include <filesystem>
#include <vector>

namespace pedaleira
{

/**
    Realtime-safe wrapper over NeuralAmpModelerCore (nam::DSP). The same
    class fills two different chain roles -- an amp position and a "neural
    drive" position -- the inference engine itself doesn't know or care
    which; only the trained .nam file loaded into it differs. See
    EffectRegistry::registerBuiltInEffects for how the two roles get
    registered under different names.

    Model loading (nam::get_dsp(), which allocates) always happens on the
    control thread, inside loadModel(). The freshly built model is handed
    to the audio thread through the same DeferredReclaimer pattern used for
    the SignalGraph itself: an atomic pointer swap, with the previous model
    freed later by this object's own Timer, never on the audio thread.
*/
class NAMProcessor : public EffectProcessor,
                      private juce::Timer
{
public:
    explicit NAMProcessor (juce::String chainRoleName = "NAM Amp");
    ~NAMProcessor() override;

    /** Control thread only. Throws nam::NamFileValidationError on a bad/malformed file. */
    void loadModel (const std::filesystem::path& namFilePath);
    void clearModel();
    bool hasModel() const noexcept { return modelSlot.currentRaw() != nullptr; }
    juce::String getLoadedModelName() const;

    void prepare (double sampleRate, int maxBlockSize, int numChannels) override;
    void process (juce::AudioBuffer<float>& buffer) override;
    void reset() override;

    juce::AudioProcessorParameterGroup* getParameters() override { return parameters.get(); }
    const char* getName() const override { return name.toRawUTF8(); }

    bool wantsModelFile() const override { return true; }
    void loadModelFile (const juce::File& file) override
    {
        loadModel (std::filesystem::path (file.getFullPathName().toStdString()));
    }
    juce::String getStatusText() const override
    {
        return hasModel() ? "Loaded: " + getLoadedModelName() : juce::String ("No model loaded");
    }

private:
    void timerCallback() override { modelSlot.sweep(); }

    juce::String name;
    std::unique_ptr<juce::AudioProcessorParameterGroup> parameters;
    juce::AudioParameterFloat* inputGainDb = nullptr;
    juce::AudioParameterFloat* outputGainDb = nullptr;

    DeferredReclaimer<nam::DSP> modelSlot;
    std::filesystem::path lastLoadedPath;

    double sampleRate = 0.0;
    int preparedBlockSize = 0;

    // Fixed-size scratch for NAM's NAM_SAMPLE**-per-channel API -- sized in
    // prepare(), never touched again on the audio thread. The guitar path
    // is mono going in (see AudioEngine); NAM models are inherently mono
    // nonlinear transforms, so we sum channel 0 in and broadcast back out.
    std::vector<float> inputScratch, outputScratch;
};

} // namespace pedaleira
