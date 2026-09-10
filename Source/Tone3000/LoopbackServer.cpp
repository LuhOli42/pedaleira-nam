#include "LoopbackServer.h"

#include <cstring>

namespace pedaleira
{

LoopbackServer::~LoopbackServer()
{
    stop();
}

bool LoopbackServer::start (int port, std::function<void (const juce::StringPairArray&)> onRequest)
{
    callback = std::move (onRequest);

    if (! listener.createListener (port, "127.0.0.1"))
        return false;

    shouldStop = false;
    worker = std::thread ([this] { run(); });
    return true;
}

void LoopbackServer::stop()
{
    shouldStop = true;
    listener.close();

    if (worker.joinable())
        worker.join();
}

void LoopbackServer::run()
{
    std::unique_ptr<juce::StreamingSocket> connection (listener.waitForNextConnection());

    if (connection == nullptr || shouldStop.load())
        return;

    char chunk[4096] = {};
    const int bytesRead = connection->read (chunk, (int) sizeof (chunk) - 1, false);

    juce::StringPairArray params;

    if (bytesRead > 0)
    {
        const auto request = juce::String::fromUTF8 (chunk, bytesRead);

        // Request line looks like: GET /callback?code=...&state=... HTTP/1.1
        const auto firstLine = request.upToFirstOccurrenceOf ("\r\n", false, false);
        const auto path = firstLine.fromFirstOccurrenceOf (" ", false, false)
                                    .upToFirstOccurrenceOf (" ", false, false);
        const auto query = path.fromFirstOccurrenceOf ("?", false, false);

        for (const auto& pair : juce::StringArray::fromTokens (query, "&", ""))
        {
            if (pair.isEmpty())
                continue;

            const auto key = pair.upToFirstOccurrenceOf ("=", false, false);
            const auto value = pair.fromFirstOccurrenceOf ("=", false, false);
            params.set (juce::URL::removeEscapeChars (key), juce::URL::removeEscapeChars (value));
        }
    }

    static const char* const response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n\r\n"
        "<html><body style=\"font-family:sans-serif;background:#141414;color:#eee;padding:40px\">"
        "<h2>Pedaleira NAM</h2><p>Login complete &mdash; you can close this tab.</p></body></html>";

    connection->write (response, (int) std::strlen (response));
    connection->close();

    if (callback)
        callback (params);
}

} // namespace pedaleira
