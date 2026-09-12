#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace openguitarmultifx
{

/**
    Bottom bar: tuner, BPM/tap-tempo, and IN/OUT level meters -- always on
    screen alongside the chain, per user request 2026-09-10 ("o footer ali
    embaixo q vai ter o afinador, e o bpm... e um meter de entrada de audio
    e saida"). Tuner is a horizontal gauge with the note letter next to it;
    IN/OUT meters are horizontal bars stacked IN-over-OUT, each labelled
    directly on the bar -- both per follow-up user correction the same day
    (originally vertical bars/no gauge).

    VISUAL PLACEHOLDER ONLY, by explicit user choice (asked: real DSP now
    vs. reserve the layout -- picked the latter). Nothing here reads real
    audio: no pitch detection, no actual level metering, no tap-tempo
    timing logic. That's Phase 5 (`AGENTS.md` roadmap table -- "Looper,
    tuner, MIDI, advanced routing" -- not started). When Phase 5 lands,
    this becomes the real thing; until then every value here is a static
    mock so the final screen layout is visible and stable now instead of
    growing a new region later. See setMockLevels()/setMockTuning() for the
    hooks already wired for real data later without a layout change.
*/
class FooterBar : public juce::Component
{
public:
    FooterBar();

    void resized() override;
    void paint (juce::Graphics& g) override;

    /** Placeholder hook: 0-1 level for the IN/OUT meter bars. Not fed by
        AudioEngine yet -- wire this up when real metering lands (Phase 5)
        instead of adding a new meter component from scratch. */
    void setMockLevels (float inLevel, float outLevel);

    /** Placeholder hook: note name text + gauge needle position (-1 flat
        .. 0 in tune .. +1 sharp). Not fed by a real pitch detector yet --
        same idea as setMockLevels(). */
    void setMockTuning (const juce::String& note, float deviation);

private:
    juce::Label tunerNoteLabel { {}, "--" };
    juce::Label bpmValueLabel { {}, "120" };
    juce::Label bpmUnitLabel { {}, "BPM" };
    juce::TextButton tapButton { "TAP" };

    float inLevel = 0.0f, outLevel = 0.0f;
    float tuningDeviation = 0.0f;

    juce::Rectangle<float> tunerGaugeBounds, inMeterBounds, outMeterBounds;

    void drawHorizontalMeter (juce::Graphics& g, juce::Rectangle<float> bounds, float level, const juce::String& label) const;
    void drawTunerGauge (juce::Graphics& g, juce::Rectangle<float> bounds) const;
};

} // namespace openguitarmultifx
