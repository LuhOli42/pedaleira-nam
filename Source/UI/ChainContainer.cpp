#include "ChainContainer.h"

namespace pedaleira
{

void ChainContainer::setBlockBounds (std::vector<juce::Rectangle<float>> bounds)
{
    blockBounds = std::move (bounds);
    repaint();
}

void ChainContainer::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::white.withAlpha (0.35f));

    for (size_t i = 0; i + 1 < blockBounds.size(); ++i)
    {
        const auto a = blockBounds[i];
        const auto b = blockBounds[i + 1];
        g.drawLine (a.getRight(), a.getCentreY(), b.getX(), b.getCentreY(), 2.0f);
    }
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
