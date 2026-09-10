#include "PresetBadge.h"

namespace pedaleira
{

void PresetBadge::setNumber (int number)
{
    if (currentNumber != number)
    {
        currentNumber = number;
        repaint();
    }
}

void PresetBadge::setDisplayTypeface (juce::Typeface::Ptr typeface)
{
    displayTypeface = std::move (typeface);
    repaint();
}

void PresetBadge::mouseUp (const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && onClicked)
        onClicked();
}

void PresetBadge::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff1c1c1c));
    g.fillRoundedRectangle (bounds, 10.0f);

    auto font = displayTypeface != nullptr
                    ? juce::Font (juce::FontOptions (bounds.getHeight() * 0.6f).withTypeface (displayTypeface))
                    : juce::Font (juce::FontOptions (bounds.getHeight() * 0.6f, juce::Font::bold));
    g.setFont (font);

    g.setColour (currentNumber > 0 ? juce::Colours::white : juce::Colours::grey);
    const auto text = currentNumber > 0 ? juce::String (currentNumber) : juce::String::fromUTF8 ("\xe2\x80\x94");
    g.drawText (text, bounds, juce::Justification::centred);
}

} // namespace pedaleira
