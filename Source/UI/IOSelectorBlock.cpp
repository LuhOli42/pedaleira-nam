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
    // A plain, borderless pill -- same reference as the effect blocks
    // (Quad Cortex's "In 1" / "Out 1/2" tiles): no outline, no icon, just
    // the label on top and the current physical channel below it.
    auto bounds = getLocalBounds().toFloat().reduced (4.0f);

    g.setColour (juce::Colour (0xff1c1c1c));
    g.fillRoundedRectangle (bounds, 10.0f);

    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    g.drawFittedText (label, bounds.reduced (4.0f).toNearestInt(), juce::Justification::centredTop, 1);

    g.setColour (juce::Colours::lightgrey);
    g.setFont (11.5f);
    g.drawFittedText (selectionText, bounds.reduced (4.0f).toNearestInt(), juce::Justification::centredBottom, 2);
}

} // namespace pedaleira
