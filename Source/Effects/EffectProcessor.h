#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>

namespace pedaleira
{

/**
    The contract every stage of the signal chain implements (see
    ARCHITECTURE.md, section C).

    process() runs exclusively on the audio thread: it never allocates,
    never blocks, never does I/O. prepare()/reset()/getState()/setState()
    run on the control thread, before the processor enters a SignalGraph
    that gets published (see Engine/DeferredReclaimer.h).
*/
class EffectProcessor
{
public:
    virtual ~EffectProcessor() = default;

    virtual void prepare (double sampleRate, int maxBlockSize, int numChannels) = 0;
    virtual void process (juce::AudioBuffer<float>& buffer) = 0;
    virtual void reset() = 0;

    void setBypassed (bool shouldBypass) noexcept { bypassed.store (shouldBypass, std::memory_order_relaxed); }
    bool isBypassed() const noexcept { return bypassed.load (std::memory_order_relaxed); }

    virtual juce::AudioProcessorParameterGroup* getParameters() = 0;
    virtual std::unique_ptr<juce::XmlElement> getState() const = 0;
    virtual void setState (const juce::XmlElement& state) = 0;

    virtual const char* getName() const = 0;

private:
    std::atomic<bool> bypassed { false };
};

} // namespace pedaleira
