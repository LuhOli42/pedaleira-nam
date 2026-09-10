#include "ChainContainer.h"

namespace pedaleira
{

void ChainContainer::setBlockBounds (std::vector<juce::Rectangle<float>> bounds)
{
    blockBounds = std::move (bounds);
    repaint();
}

void ChainContainer::mouseUp (const juce::MouseEvent& event)
{
    if (onLineClicked)
        onLineClicked (event.x);
}

void ChainContainer::paint (juce::Graphics& g)
{
    // The signal path, always on screen -- not just gaps between blocks.
    // A plain Component's paint() runs before its children's, so this sits
    // behind the blocks and the "+" tile for free; they visually sit ON it,
    // matching the reference UI's always-visible input-to-output line.
    const float y = (float) getHeight() * 0.5f;
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawLine (0.0f, y, (float) getWidth(), y, 2.0f);
}

AddBlockButton::AddBlockButton() : juce::Button ("addBlock")
{
}

void AddBlockButton::paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced (3.0f);

    g.setColour (juce::Colours::white.withAlpha (isButtonDown ? 0.16f : (isMouseOverButton ? 0.1f : 0.04f)));
    g.fillRoundedRectangle (bounds, 8.0f);

    juce::Path outline;
    outline.addRoundedRectangle (bounds, 8.0f);
    juce::Path dashedOutline;
    const float dashLengths[] = { 5.0f, 4.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (dashedOutline, outline, dashLengths, 2);
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.fillPath (dashedOutline);

    g.setColour (juce::Colours::white.withAlpha (0.75f));
    const float plusSize = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.28f;
    const auto c = bounds.getCentre();
    g.drawLine (c.x - plusSize * 0.5f, c.y, c.x + plusSize * 0.5f, c.y, 2.5f);
    g.drawLine (c.x, c.y - plusSize * 0.5f, c.x, c.y + plusSize * 0.5f, 2.5f);
}

} // namespace pedaleira
