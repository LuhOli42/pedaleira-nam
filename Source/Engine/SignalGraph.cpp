#include "SignalGraph.h"

namespace openguitarmultifx
{

void SignalGraph::addProcessor (EffectProcessor* processor)
{
    processors.push_back (processor);
}

void SignalGraph::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    for (auto* p : processors)
        p->prepare (sampleRate, maxBlockSize, numChannels);
}

void SignalGraph::process (juce::AudioBuffer<float>& buffer)
{
    for (auto* p : processors)
        if (! p->isBypassed())
            p->process (buffer);
}

void SignalGraph::reset()
{
    for (auto* p : processors)
        p->reset();
}

} // namespace openguitarmultifx
