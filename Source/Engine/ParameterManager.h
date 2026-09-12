#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace openguitarmultifx
{

/**
    Lock-free SPSC queue of parameter changes: the control thread writes
    (push), the audio thread reads (drain) at the start of each block. The
    backing array is allocated once, in the constructor -- after that, it
    never allocates.

    A full queue drops the newest change instead of blocking or growing --
    that's the right call for a queue the audio thread depends on: losing a
    parameter update is recoverable (the next push wins), stalling is not.
*/
class ParameterManager
{
public:
    struct Change
    {
        uint32_t parameterId;
        float value;
    };

    explicit ParameterManager (int capacity = 256);

    /** Control thread. Returns false if the queue was full (change dropped). */
    bool push (uint32_t parameterId, float value);

    /** Audio thread. Applies each pending change, in the order it arrived. */
    template <typename Fn>
    void drain (Fn&& applyChange)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (fifo.getNumReady(), start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i)
            applyChange (buffer[(size_t) (start1 + i)]);
        for (int i = 0; i < size2; ++i)
            applyChange (buffer[(size_t) (start2 + i)]);

        fifo.finishedRead (size1 + size2);
    }

private:
    juce::AbstractFifo fifo;
    std::vector<Change> buffer;
};

} // namespace openguitarmultifx
