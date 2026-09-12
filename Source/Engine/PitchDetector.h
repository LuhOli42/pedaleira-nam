#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <vector>

namespace openguitarmultifx
{

/**
    A YIN-algorithm pitch detector for the tuner (`FooterBar`'s tuner
    gauge, wired through `AudioEngine::getDetectedFrequencyHz()`).

    Realtime-safe by construction: every buffer is sized once in
    prepare() (never reallocated from pushSamples()), and the O(window *
    maxLag) YIN analysis itself only runs once every ~1/15th of a second
    of new audio (a tuner doesn't need to update faster than that, and
    running the full analysis every single callback would be needless
    CPU burn for no perceptible benefit) -- pushSamples() just accumulates
    into a ring buffer the rest of the time.

    YIN (de Cheveigné & Kawahara, 2002) rather than plain autocorrelation:
    plain autocorrelation on a guitar's harmonically-rich signal easily
    locks onto an overtone instead of the fundamental (a classic "octave
    error"); YIN's cumulative-mean-normalized difference function is
    specifically the standard fix for that failure mode.
*/
class PitchDetector
{
public:
    /** Control thread (called from AudioEngine::audioDeviceAboutToStart()). */
    void prepare (double sampleRateToUse);

    /** Audio thread. Never allocates -- all buffers are sized in prepare(). */
    void pushSamples (const float* data, int numSamples) noexcept;

    /** Safe to read from any thread. 0 means "no clear pitch" (silence or noise). */
    float getDetectedFrequencyHz() const noexcept { return detectedFrequencyHz.load (std::memory_order_relaxed); }

private:
    void runAnalysis() noexcept;

    static constexpr float minFreqHz = 70.0f;  // below standard low E (82.4Hz), covers drop-D too
    static constexpr float maxFreqHz = 1200.0f;
    static constexpr float yinThreshold = 0.15f; // standard YIN absolute threshold
    static constexpr float silenceRmsThreshold = 0.01f;
    static constexpr double updateRateHz = 15.0; // plenty responsive for a tuner display

    std::vector<float> ringBuffer, analysisBuffer, diffBuffer, cmndBuffer;
    int ringSize = 0;
    int windowSize = 0;
    int tauMin = 0;
    int tauMax = 0;
    int writePos = 0;
    int samplesSinceAnalysis = 0;
    int analysisIntervalSamples = 0;
    double sampleRate = 0.0;

    std::atomic<float> detectedFrequencyHz { 0.0f };
};

} // namespace openguitarmultifx
