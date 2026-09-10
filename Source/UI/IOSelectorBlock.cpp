#include "IOSelectorBlock.h"

namespace pedaleira
{

IOSelectorBlock::IOSelectorBlock (juce::String labelText)
    : label (std::move (labelText))
{
}

void IOSelectorBlock::setOptions (juce::StringArray optionNames, int currentIndex)
{
    options = std::move (optionNames);
    selectionText = (currentIndex >= 0 && currentIndex < options.size()) ? options[currentIndex] : juce::String ("Default");
    repaint();
}

void IOSelectorBlock::mouseUp (const juce::MouseEvent&)
{
    if (options.isEmpty())
        return;

    juce::PopupMenu menu;
    for (int i = 0; i < options.size(); ++i)
        menu.addItem (i + 1, options[i]);

    menu.showMenuAsync (juce::PopupMenu::Options(),
        [this] (int result)
        {
            if (result > 0 && onSelectionChanged)
                onSelectionChanged (result - 1);
        });
}

void IOSelectorBlock::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (3.0f);

    g.setColour (juce::Colour (0xff262626));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    g.setColour (juce::Colours::white);
    g.setFont (12.5f);
    g.drawFittedText (label, bounds.reduced (6.0f).toNearestInt(), juce::Justification::centredTop, 1);

    g.setColour (juce::Colours::lightgrey);
    g.setFont (10.5f);
    g.drawFittedText (selectionText, bounds.reduced (6.0f).toNearestInt(), juce::Justification::centred, 2);
}

} // namespace pedaleira
