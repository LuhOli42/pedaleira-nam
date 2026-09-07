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

    /**
        Default implementation: walks getParameters() and serializes every
        juce::AudioParameterFloat by its paramID. Covers every processor
        whose state is fully described by its float parameters (true for
        all the Phase 1 pedals) -- override only if a processor needs more
        than that (e.g. a loaded model reference).
    */
    virtual std::unique_ptr<juce::XmlElement> getState() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("EffectState");

        if (auto* group = const_cast<EffectProcessor*> (this)->getParameters())
            for (auto* param : group->getParameters (true))
                if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*> (param))
                    xml->setAttribute (floatParam->paramID, (double) floatParam->get());

        return xml;
    }

    virtual void setState (const juce::XmlElement& state)
    {
        if (auto* group = getParameters())
            for (auto* param : group->getParameters (true))
                if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*> (param))
                    if (state.hasAttribute (floatParam->paramID))
                        *floatParam = (float) state.getDoubleAttribute (floatParam->paramID);
    }

    virtual const char* getName() const = 0;

private:
    std::atomic<bool> bypassed { false };
};

} // namespace pedaleira
