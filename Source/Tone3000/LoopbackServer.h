#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <thread>

namespace pedaleira
{

/**
    A one-shot local HTTP listener, just enough to catch an OAuth redirect.

    TONE3000's docs say localhost origins are auto-allowed without
    pre-registering a redirect URI, which is what makes this the simplest
    viable flow for a desktop app (their other documented native pattern is
    a custom URI scheme, which needs OS-level registration).

    Accepts exactly one connection, parses the query string off the request
    line, answers with a small "you can close this tab" page, and stops.
    The callback fires on this object's own background thread -- the caller
    is responsible for marshalling to the message thread.
*/
class LoopbackServer
{
public:
    ~LoopbackServer();

    bool start (int port, std::function<void (const juce::StringPairArray&)> onRequest);
    void stop();

private:
    void run();

    juce::StreamingSocket listener;
    std::thread worker;
    std::atomic<bool> shouldStop { false };
    std::function<void (const juce::StringPairArray&)> callback;
};

} // namespace pedaleira
