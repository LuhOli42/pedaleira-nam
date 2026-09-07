#include "AudioEngine.h"

namespace pedaleira
{

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    stop();
}

bool AudioEngine::start()
{
    // 1 input (guitar, mono) / 2 outputs (stereo) -- adjustable once a real
    // audio interface replaces the PC's default device.
    const auto err = deviceManager.initialiseWithDefaultDevices (1, 2);

    if (err.isNotEmpty())
    {
        juce::Logger::writeToLog ("AudioEngine: failed to open device -- " + err);
        return false;
    }

    deviceManager.addAudioCallback (this);
    startTimer (100); // DeferredReclaimer sweep at 10Hz -- comfortably above the 500ms safety margin
    return true;
}

void AudioEngine::stop()
{
    stopTimer();
    deviceManager.removeAudioCallback (this);
}

void AudioEngine::setSignalGraph (std::unique_ptr<SignalGraph> newGraph)
{
    if (newGraph != nullptr)
    {
        const auto sr = sampleRate.load (std::memory_order_relaxed);
        const auto bs = blockSize.load (std::memory_order_relaxed);
        if (sr > 0.0)
            newGraph->prepare (sr, bs, 2);
    }

    graphSlot.publish (std::move (newGraph));
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate.store (device->getCurrentSampleRate(), std::memory_order_relaxed);
    blockSize.store (device->getCurrentBufferSizeSamples(), std::memory_order_relaxed);
}

void AudioEngine::audioDeviceStopped()
{
    sampleRate.store (0.0, std::memory_order_relaxed);
    blockSize.store (0, std::memory_order_relaxed);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                     float* const* outputChannelData, int numOutputChannels,
                                                     int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    const auto startTicks = juce::Time::getHighResolutionTicks();

    // An empty graph means total bypass: copy input straight to output
    // before anything else, and let the SignalGraph process on top of that
    // if there's anything in it.
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        auto* out = outputChannelData[ch];
        const float* in = (numInputChannels > 0)
                               ? inputChannelData[juce::jmin (ch, numInputChannels - 1)]
                               : nullptr;

        if (in != nullptr)
            juce::FloatVectorOperations::copy (out, in, numSamples);
        else
            juce::FloatVectorOperations::clear (out, numSamples);
    }

    juce::AudioBuffer<float> buffer (outputChannelData, numOutputChannels, numSamples);

    if (auto* graph = graphSlot.currentRaw())
        graph->process (buffer);

    const auto elapsedSeconds = juce::Time::highResolutionTicksToSeconds (
        juce::Time::getHighResolutionTicks() - startTicks);
    const auto sr = sampleRate.load (std::memory_order_relaxed);
    const auto blockSeconds = numSamples / (sr > 0.0 ? sr : 48000.0);

    lastCpuUsage.store (blockSeconds > 0.0 ? elapsedSeconds / blockSeconds : 0.0,
                         std::memory_order_relaxed);
}

void AudioEngine::timerCallback()
{
    graphSlot.sweep();
}

} // namespace pedaleira
