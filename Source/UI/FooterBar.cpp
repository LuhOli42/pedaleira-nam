#include "FooterBar.h"

#include "OpenGuitarMultiFxLookAndFeel.h"

namespace openguitarmultifx
{

FooterBar::FooterBar()
{
    addAndMakeVisible (tunerNoteLabel);
    tunerNoteLabel.setFont (juce::Font (34.0f, juce::Font::bold));
    tunerNoteLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (bpmValueLabel);
    bpmValueLabel.setFont (juce::Font (28.0f, juce::Font::bold));
    bpmValueLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (bpmUnitLabel);
    bpmUnitLabel.setFont (juce::Font (13.0f));
    bpmUnitLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    bpmUnitLabel.setJustificationType (juce::Justification::centred);

    // Visual only -- no tap-tempo timing logic yet, see class doc comment.
    addAndMakeVisible (tapButton);

    setLevels (0.0f, 0.0f);
    setTuning ("--", 0.0f);
}

void FooterBar::setLevels (float inLevelIn, float outLevelIn)
{
    inLevel = juce::jlimit (0.0f, 1.0f, inLevelIn);
    outLevel = juce::jlimit (0.0f, 1.0f, outLevelIn);
    repaint();
}

void FooterBar::setTuning (const juce::String& note, float deviation)
{
    tunerNoteLabel.setText (note, juce::dontSendNotification);
    tuningDeviation = juce::jlimit (-1.0f, 1.0f, deviation);
    hasDetectedNote = note != "--";
    repaint();
}

void FooterBar::resized()
{
    auto area = getLocalBounds().reduced (16, 10);

    // Tuner zone: note letter, then the horizontal gauge bar filling the
    // rest of the zone's width -- both per user request 2026-09-10 ("o
    // tuner tem q ser uma barrinha horizontal com a letra que é a
    // afinacao").
    auto tunerZone = area.removeFromLeft (260);
    tunerNoteLabel.setBounds (tunerZone.removeFromLeft (56));
    tunerZone.removeFromLeft (10);
    tunerGaugeBounds = tunerZone.withSizeKeepingCentre (tunerZone.getWidth(), 16).toFloat();

    area.removeFromLeft (16);

    // Meters zone: two horizontal bars, IN above OUT -- drawn directly in
    // paint(), no child components there.
    auto meterZone = area.removeFromRight (240);
    inMeterBounds = meterZone.removeFromTop (meterZone.getHeight() / 2).reduced (0, 4).toFloat();
    outMeterBounds = meterZone.reduced (0, 4).toFloat();

    area.removeFromRight (16);

    // Whatever's left in the middle: TAP + BPM readout.
    auto tapZone = area.removeFromRight (76).withSizeKeepingCentre (68, 48);
    tapButton.setBounds (tapZone);
    area.removeFromRight (12);
    auto bpmTextZone = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 100), 58);
    bpmValueLabel.setBounds (bpmTextZone.removeFromTop (36));
    bpmUnitLabel.setBounds (bpmTextZone);
}

void FooterBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (0.0f, 0.0f, (float) getWidth(), 0.0f, 1.0f);

    drawTunerGauge (g, tunerGaugeBounds);
    drawHorizontalMeter (g, inMeterBounds, inLevel, "In");
    drawHorizontalMeter (g, outMeterBounds, outLevel, "Out");
}

void FooterBar::drawTunerGauge (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    g.setColour (juce::Colour (0xff2a2a2a));
    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

    // Centre tick -- "in tune" reference point.
    const float centreX = bounds.getCentreX();
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawLine (centreX, bounds.getY() - 3.0f, centreX, bounds.getBottom() + 3.0f, 2.0f);

    // Needle -- deviation -1..1 mapped across the gauge's width. Sits
    // dead-centre with the neutral accent colour when no pitch is
    // detected -- green here would misleadingly read as "in tune" for
    // silence, not "nothing to tune".
    const float needleX = centreX + tuningDeviation * bounds.getWidth() * 0.5f;
    const bool inTune = hasDetectedNote && std::abs (tuningDeviation) < 0.05f;
    g.setColour (inTune ? juce::Colours::limegreen : OpenGuitarMultiFxLookAndFeel::getAppAccentColour());
    g.fillEllipse (needleX - 8.0f, bounds.getCentreY() - 8.0f, 16.0f, 16.0f);
}

void FooterBar::drawHorizontalMeter (juce::Graphics& g, juce::Rectangle<float> bounds, float level, const juce::String& label) const
{
    g.setColour (juce::Colour (0xff2a2a2a));
    g.fillRoundedRectangle (bounds, 4.0f);

    const auto fill = bounds.withWidth (bounds.getWidth() * level);
    g.setColour (level > 0.85f ? juce::Colours::orangered : OpenGuitarMultiFxLookAndFeel::getAppAccentColour());
    g.fillRoundedRectangle (fill, 4.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText (label, bounds.reduced (8.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
}

} // namespace openguitarmultifx
