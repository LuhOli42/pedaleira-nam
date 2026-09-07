#pragma once

#include "DeferredReclaimer.h"
#include "SignalGraph.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <memory>

namespace pedaleira
{

/**
    Owns the audio device and the block loop. Implements
    juce::AudioIODeviceCallback directly -- no extra layers between the
    driver and the SignalGraph.

    See ARCHITECTURE.md section A/D: audioDeviceIOCallbackWithContext()
    runs on the audio thread and follows the rules that apply there;
    setSignalGraph() runs on the control thread and is the only way to
    change what's playing.
*/
class AudioEngine : private juce::AudioIODeviceCallback,
                     private juce::Timer
{
public:
    AudioEngine();
    ~AudioEngine() override;

    /** Control thread. Opens the default device and starts processing. */
    bool start();
    void stop();

    /** Control thread. The engine takes ownership of the graph. */
    void setSignalGraph (std::unique_ptr<SignalGraph> newGraph);

    /** Safe to read from any thread (it's just telemetry). */
    double getCurrentCpuUsage() const noexcept { return lastCpuUsage.load (std::memory_order_relaxed); }

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                            float* const* outputChannelData, int numOutputChannels,
                                            int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void timerCallback() override;

    juce::AudioDeviceManager deviceManager;
    DeferredReclaimer<SignalGraph> graphSlot;

    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> blockSize { 0 };
    std::atomic<double> lastCpuUsage { 0.0 };
};

} // namespace pedaleira
