#pragma once

#include "../Effects/EffectProcessor.h"

#include <memory>
#include <vector>

namespace pedaleira
{

/**
    A serial chain of EffectProcessor instances. Built entirely on the
    control thread (addProcessor + prepare) and only then published into
    the AudioEngine via DeferredReclaimer -- from that point on it's
    effectively immutable, and process() is the only thing the audio thread
    ever calls on it.

    Split/merge (see ARCHITECTURE.md section A) doesn't exist here yet --
    Phase 0 only covers the serial case, which is enough to validate the
    realtime boundary end to end.
*/
class SignalGraph
{
public:
    void addProcessor (std::unique_ptr<EffectProcessor> processor);

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    int getNumProcessors() const noexcept { return (int) processors.size(); }

private:
    std::vector<std::unique_ptr<EffectProcessor>> processors;
};

} // namespace pedaleira
