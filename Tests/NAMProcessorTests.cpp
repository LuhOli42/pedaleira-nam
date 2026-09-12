#include "Effects/NAMProcessor.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <filesystem>

#ifndef OGMFX_TEST_FIXTURES_DIR
#error "OGMFX_TEST_FIXTURES_DIR must be defined by the build (see Tests/CMakeLists.txt)"
#endif

namespace openguitarmultifx
{

namespace
{
    std::filesystem::path lstmFixturePath()
    {
        return std::filesystem::path (OGMFX_TEST_FIXTURES_DIR) / "fixtures" / "lstm.nam";
    }
}

class NAMProcessorTests : public juce::UnitTest
{
public:
    NAMProcessorTests() : juce::UnitTest ("NAMProcessor", "Effects") {}

    void runTest() override
    {
        beginTest ("with no model loaded, process() is a no-op passthrough");
        {
            NAMProcessor nam;
            nam.prepare (48000.0, 512, 1);

            juce::AudioBuffer<float> buffer (1, 16);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 0.3f);

            nam.process (buffer);

            expect (! nam.hasModel());
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                expectWithinAbsoluteError (buffer.getSample (0, i), 0.3f, 1.0e-6f);
        }

        beginTest ("loading the real MIT-licensed lstm.nam fixture succeeds");
        {
            NAMProcessor nam;
            nam.prepare (48000.0, 512, 1);

            expect (std::filesystem::exists (lstmFixturePath()), "fixture file missing -- check Tests/fixtures/lstm.nam");

            nam.loadModel (lstmFixturePath());

            expect (nam.hasModel());
        }

        beginTest ("a bad file path throws instead of silently doing nothing");
        {
            NAMProcessor nam;
            nam.prepare (48000.0, 512, 1);

            bool threw = false;
            try
            {
                nam.loadModel (std::filesystem::path ("/nonexistent/not-a-real-model.nam"));
            }
            catch (const std::exception&)
            {
                threw = true;
            }

            expect (threw);
            expect (! nam.hasModel());
        }

        beginTest ("inference on a loaded model produces finite, non-NaN output");
        {
            NAMProcessor nam;
            nam.prepare (48000.0, 512, 1);
            nam.loadModel (lstmFixturePath());

            juce::AudioBuffer<float> buffer (1, 256);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 0.1f * std::sin ((float) i * 0.1f));

            nam.process (buffer);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float sample = buffer.getSample (0, i);
                expect (std::isfinite (sample), "NAM inference produced a non-finite sample");
            }
        }
    }
};

static NAMProcessorTests namProcessorTests;

} // namespace openguitarmultifx
