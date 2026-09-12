#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <memory>
#include <vector>

namespace openguitarmultifx
{

/**
    Atomic pointer swap with deferred destruction — the pattern used at
    every realtime boundary in this project (the signal graph today; NAM
    models and IRs in Phase 1).

    Why not std::atomic<std::shared_ptr<T>>: in the most common stdlib
    implementations that isn't truly lock-free (it falls back to an
    internal mutex/spinlock). Why not delete directly in exchange(): the
    audio thread might be in the middle of a process() call holding the old
    pointer -- deleting it there would be a use-after-free. The solution
    here is the simplest one that's actually correct: the control thread
    never deletes the old object on the spot; it holds onto it for a
    generous margin of time (much larger than any audio block), and only
    then frees it, always from the control thread itself, via sweep().
*/
template <typename T>
class DeferredReclaimer
{
public:
    ~DeferredReclaimer()
    {
        delete current.load (std::memory_order_relaxed);
        for (auto& r : retired)
            delete r.ptr;
    }

    /** Control thread. Hands ownership of the object to the reclaimer. */
    void publish (std::unique_ptr<T> next)
    {
        T* raw = next.release();
        T* old = current.exchange (raw, std::memory_order_acq_rel);

        if (old != nullptr)
            retired.push_back ({ old, juce::Time::getMillisecondCounter() });
    }

    /**
        Control thread -- call this periodically (e.g. a juce::Timer at
        10-20 Hz). safetyMarginMs should be much larger than the duration of
        any single audio callback (block size / sample rate); 500ms is
        generous even for large buffers.
    */
    void sweep (uint32_t safetyMarginMs = 500)
    {
        const auto now = juce::Time::getMillisecondCounter();

        auto it = retired.begin();
        while (it != retired.end())
        {
            if (now - it->retiredAtMs >= safetyMarginMs)
            {
                delete it->ptr;
                it = retired.erase (it);
            }
            else
            {
                ++it;
            }
        }
    }

    /** Audio thread -- the only access point allowed. A single load, no allocation. */
    T* currentRaw() const noexcept { return current.load (std::memory_order_acquire); }

private:
    struct Retired
    {
        T* ptr;
        uint32_t retiredAtMs;
    };

    std::atomic<T*> current { nullptr };
    std::vector<Retired> retired; // touched only by the control thread
};

} // namespace openguitarmultifx
