#include "EffectBlockComponent.h"

namespace pedaleira
{

EffectBlockComponent::EffectBlockComponent (EffectProcessor& processorToShow)
    : processor (processorToShow)
{
}

void EffectBlockComponent::setSelected (bool shouldBeSelected)
{
    selected = shouldBeSelected;
    repaint();
}

void EffectBlockComponent::mouseUp (const juce::MouseEvent&)
{
    if (onClicked != nullptr)
        onClicked();
}

void EffectBlockComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (3.0f);
    const bool bypassed = processor.isBypassed();

    auto fill = bypassed ? juce::Colour (0xff3a3a3a) : juce::Colour (0xff2d5c56);
    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (selected ? juce::Colour (0xff6fe0c8) : juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds, 8.0f, selected ? 2.5f : 1.0f);

    g.setColour (juce::Colours::white.withAlpha (bypassed ? 0.5f : 1.0f));
    g.setFont (14.0f);
    g.drawFittedText (processor.getName(), bounds.reduced (6.0f).toNearestInt(),
                       juce::Justification::centredTop, 3);

    if (bypassed)
    {
        g.setFont (11.0f);
        g.drawFittedText ("BYPASSED", bounds.reduced (6.0f).toNearestInt(),
                           juce::Justification::centredBottom, 1);
    }
}

} // namespace pedaleira
