#include "Engine/AudioEngine.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

namespace
{
    std::atomic<bool> shouldStop { false };
    void handleSignal (int) { shouldStop.store (true); }
}

int main (int argc, char* argv[])
{
    juce::ignoreUnused (argc, argv);

    // The MessageManager needs to exist for the AudioEngine's juce::Timer
    // (DeferredReclaimer sweep) to fire.
    juce::MessageManager::getInstance();

    std::signal (SIGINT, handleSignal);
    std::signal (SIGTERM, handleSignal);

    pedaleira::AudioEngine engine;

    if (! engine.start())
    {
        juce::Logger::writeToLog ("Pedaleira NAM: failed to start the AudioEngine.");
        juce::MessageManager::deleteInstance();
        return 1;
    }

    juce::Logger::writeToLog (
        "Pedaleira NAM -- Phase 0: engine running, empty graph (passthrough). Ctrl+C to quit.");

    // The signal handler only sets an atomic flag (async-signal-safe); the
    // actual call into MessageManager happens here, on a normal thread.
    std::thread watcher ([]
    {
        while (! shouldStop.load())
            std::this_thread::sleep_for (std::chrono::milliseconds (50));

        juce::MessageManager::getInstance()->stopDispatchLoop();
    });

    juce::MessageManager::getInstance()->runDispatchLoop();
    watcher.join();

    engine.stop();
    juce::MessageManager::deleteInstance();
    return 0;
}
