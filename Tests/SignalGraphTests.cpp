#include "Engine/SignalGraph.h"

#include <juce_core/juce_core.h>

namespace pedaleira
{

namespace
{
    /** Minimal test-only processor: applies a fixed gain and counts calls. */
    class TestGainProcessor : public EffectProcessor
    {
    public:
        explicit TestGainProcessor (float gainToApply) : gain (gainToApply) {}

        void prepare (double, int, int) override { ++prepareCalls; }

        void process (juce::AudioBuffer<float>& buffer) override
        {
            ++processCalls;
            buffer.applyGain (gain);
        }

        void reset() override { ++resetCalls; }

        juce::AudioProcessorParameterGroup* getParameters() override { return nullptr; }
        std::unique_ptr<juce::XmlElement> getState() const override { return nullptr; }
        void setState (const juce::XmlElement&) override {}
        const char* getName() const override { return "TestGain"; }

        float gain;
        int prepareCalls = 0, processCalls = 0, resetCalls = 0;
    };
}

class SignalGraphTests : public juce::UnitTest
{
public:
    SignalGraphTests() : juce::UnitTest ("SignalGraph", "Engine") {}

    void runTest() override
    {
        beginTest ("serial chain applies processors in order");
        {
            SignalGraph graph;
            graph.addProcessor (std::make_unique<TestGainProcessor> (0.5f));
            graph.addProcessor (std::make_unique<TestGainProcessor> (0.5f));
            graph.prepare (48000.0, 128, 1);

            juce::AudioBuffer<float> buffer (1, 4);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);

            graph.process (buffer);

            expectWithinAbsoluteError (buffer.getSample (0, 0), 0.25f, 1.0e-6f);
        }

        beginTest ("a bypassed processor does not run process()");
        {
            SignalGraph graph;
            auto p = std::make_unique<TestGainProcessor> (0.0f);
            auto* raw = p.get();
            raw->setBypassed (true);
            graph.addProcessor (std::move (p));
            graph.prepare (48000.0, 128, 1);

            juce::AudioBuffer<float> buffer (1, 4);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);

            graph.process (buffer);

            expectEquals (raw->processCalls, 0);
            expectWithinAbsoluteError (buffer.getSample (0, 0), 1.0f, 1.0e-6f);
        }

        beginTest ("an empty graph leaves the buffer untouched (passthrough)");
        {
            SignalGraph graph;
            graph.prepare (48000.0, 128, 1);

            juce::AudioBuffer<float> buffer (1, 4);
            buffer.clear();
            buffer.setSample (0, 0, 0.75f);

            graph.process (buffer);

            expectWithinAbsoluteError (buffer.getSample (0, 0), 0.75f, 1.0e-6f);
        }
    }
};

static SignalGraphTests signalGraphTests;

} // namespace pedaleira
