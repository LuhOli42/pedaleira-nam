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

void EffectBlockComponent::mouseDown (const juce::MouseEvent& event)
{
    dragger.startDraggingComponent (this, event);
    isDragging = false; // only becomes true in mouseDrag, once past a small threshold
}

void EffectBlockComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (! isDragging && event.getDistanceFromDragStart() > 4)
    {
        isDragging = true;
        toFront (true); // stay above sibling blocks while dragging over them
    }

    if (isDragging)
    {
        dragger.dragComponent (this, event, nullptr);
        setTopLeftPosition (getX(), 0); // horizontal reordering only -- stay on the row
    }
}

void EffectBlockComponent::mouseUp (const juce::MouseEvent&)
{
    if (isDragging)
    {
        isDragging = false;
        if (onDragEnded != nullptr)
            onDragEnded (*this);
    }
    else if (onClicked != nullptr)
    {
        onClicked();
    }
}

void EffectBlockComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (3.0f);
    const bool bypassed = processor.isBypassed();

    const auto fill = bypassed ? juce::Colour (0xff2e2e2e) : processor.getAccentColour();
    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (selected ? juce::Colour (0xff6fe0c8) : juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds, 8.0f, selected ? 2.5f : 1.0f);

    // Icon on top, name label along the bottom -- same two-zone layout as
    // the reference (Quad Cortex's Grid blocks): glyph first, text second.
    auto iconArea = bounds.reduced (6.0f);
    auto nameArea = iconArea.removeFromBottom (18.0f);
    iconArea.removeFromBottom (2.0f);

    if (bypassed)
        g.beginTransparencyLayer (0.4f);

    processor.drawIcon (g, iconArea.reduced (iconArea.getWidth() * 0.18f, iconArea.getHeight() * 0.12f));

    if (bypassed)
        g.endTransparencyLayer();

    g.setColour (juce::Colours::white.withAlpha (bypassed ? 0.5f : 1.0f));
    g.setFont (12.5f);
    g.drawFittedText (processor.getName(), nameArea.toNearestInt(), juce::Justification::centred, 2);
}

} // namespace pedaleira
