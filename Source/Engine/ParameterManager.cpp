#include "ParameterManager.h"

namespace openguitarmultifx
{

ParameterManager::ParameterManager (int capacity)
    // juce::AbstractFifo always keeps one slot free internally (to tell full
    // apart from empty without a separate flag), so we over-allocate by one
    // to make `capacity` mean what it says: the number of changes that
    // actually fit.
    : fifo (capacity + 1), buffer ((size_t) (capacity + 1))
{
}

bool ParameterManager::push (uint32_t parameterId, float value)
{
    int start1, size1, start2, size2;
    fifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 + size2 == 0)
        return false; // queue full -- drop it, never block

    if (size1 > 0)
        buffer[(size_t) start1] = { parameterId, value };
    else
        buffer[(size_t) start2] = { parameterId, value };

    fifo.finishedWrite (size1 + size2);
    return true;
}

} // namespace openguitarmultifx
