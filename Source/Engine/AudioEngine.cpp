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

    // An empty graph means total bypass: copy the selected input channel to
    // every output channel before anything else, and let the SignalGraph
    // process on top of that if there's anything in it. Output routing
    // (silencing L or R) happens AFTER the graph -- every processor still
    // always sees a full buffer to work with; routing is purely what
    // reaches the physical outputs.
    const int inChIndex = (numInputChannels > 0)
                               ? juce::jlimit (0, numInputChannels - 1, selectedInputChannel.load (std::memory_order_relaxed))
                               : 0;
    const float* in = (numInputChannels > 0) ? inputChannelData[inChIndex] : nullptr;

    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        auto* out = outputChannelData[ch];

        if (in != nullptr)
            juce::FloatVectorOperations::copy (out, in, numSamples);
        else
            juce::FloatVectorOperations::clear (out, numSamples);
    }

    juce::AudioBuffer<float> buffer (outputChannelData, numOutputChannels, numSamples);

    if (auto* graph = graphSlot.currentRaw())
        graph->process (buffer);

    const auto routing = outputRouting.load (std::memory_order_relaxed);
    if (routing == OutputRouting::leftOnly && numOutputChannels > 1)
        juce::FloatVectorOperations::clear (outputChannelData[1], numSamples);
    else if (routing == OutputRouting::rightOnly && numOutputChannels > 0)
        juce::FloatVectorOperations::clear (outputChannelData[0], numSamples);

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

juce::StringArray AudioEngine::getAvailableInputChannelNames() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getInputChannelNames();
    return {};
}

juce::StringArray AudioEngine::getAvailableOutputChannelNames() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getOutputChannelNames();
    return {};
}

} // namespace pedaleira
