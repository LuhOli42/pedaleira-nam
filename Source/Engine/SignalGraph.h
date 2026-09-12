#pragma once

#include "../Effects/EffectProcessor.h"

#include <vector>

namespace openguitarmultifx
{

/**
    A serial chain of EffectProcessor instances. SignalGraph does NOT own
    the processors -- it's a lightweight, cheaply rebuilt ordering/view over
    objects some longer-lived owner (the UI's chain, in practice) keeps
    alive. That's deliberate: an interactive chain-builder needs to add or
    remove one block without resetting every other block's parameters or
    loaded model, which isn't possible if rebuilding the graph meant
    recreating every processor from scratch.

    Built entirely on the control thread (addProcessor + prepare) and only
    then published into the AudioEngine via DeferredReclaimer -- from that
    point on the ORDERING is effectively immutable (a structural change
    publishes a whole new SignalGraph), and process() is the only thing the
    audio thread ever calls on it. Whoever removes a processor from the
    owning chain must not destroy it until they're sure no in-flight or
    recently-retired SignalGraph can still reference it (see
    MainComponent's graveyard for the pattern this implies).

    Split/merge (see ARCHITECTURE.md section A) doesn't exist here yet --
    still just the serial case.
*/
class SignalGraph
{
public:
    void addProcessor (EffectProcessor* processor);

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    int getNumProcessors() const noexcept { return (int) processors.size(); }

private:
    std::vector<EffectProcessor*> processors;
};

} // namespace openguitarmultifx
