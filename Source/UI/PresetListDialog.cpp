#include "PresetListDialog.h"

namespace pedaleira
{

PresetListDialog::PresetListDialog (juce::StringArray existingNames)
    : names (std::move (existingNames))
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("Presets", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (16.0f, juce::Font::bold));

    addAndMakeVisible (nameField);
    nameField.setTextToShowWhenEmpty ("Preset name...", juce::Colours::grey);
    nameField.onReturnKey = [this] { saveButton.triggerClick(); };

    addAndMakeVisible (saveButton);
    saveButton.onClick = [this]
    {
        const auto n = nameField.getText().trim();
        if (n.isNotEmpty() && onSaveRequested)
            onSaveRequested (n);
    };

    addAndMakeVisible (listBox);
    listBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff141414));
    listBox.setRowHeight (44);
    listBox.setVisible (! names.isEmpty());

    addAndMakeVisible (emptyStateLabel);
    emptyStateLabel.setText ("No presets saved yet.", juce::dontSendNotification);
    emptyStateLabel.setJustificationType (juce::Justification::centred);
    emptyStateLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    emptyStateLabel.setVisible (names.isEmpty());

    addAndMakeVisible (loadButton);
    loadButton.onClick = [this]
    {
        const int row = listBox.getSelectedRow();
        if (row >= 0 && row < names.size() && onPresetChosen)
            onPresetChosen (names[row]);
    };

    addAndMakeVisible (deleteButton);
    deleteButton.onClick = [this]
    {
        const int row = listBox.getSelectedRow();
        if (row >= 0 && row < names.size() && onDeleteRequested)
            onDeleteRequested (names[row]);
    };

    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { if (onPopOverlay) onPopOverlay(); };

    setSize (420, 480);
}

int PresetListDialog::getNumRows()
{
    return names.size();
}

void PresetListDialog::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= names.size())
        return;

    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff2d5c56));

    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    g.drawText (names[rowNumber], 14, 0, width - 24, height, juce::Justification::centredLeft);
}

void PresetListDialog::resized()
{
    auto area = getLocalBounds().reduced (14);

    titleLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    auto saveRow = area.removeFromTop (28);
    saveButton.setBounds (saveRow.removeFromRight (120));
    saveRow.removeFromRight (6);
    nameField.setBounds (saveRow);

    area.removeFromTop (8);

    auto bottomRow = area.removeFromBottom (34);
    closeButton.setBounds (bottomRow.removeFromRight (90));
    bottomRow.removeFromRight (8);
    deleteButton.setBounds (bottomRow.removeFromRight (90));
    bottomRow.removeFromRight (8);
    loadButton.setBounds (bottomRow.removeFromRight (140));

    area.removeFromBottom (8);
    listBox.setBounds (area);
    emptyStateLabel.setBounds (area);
}

void PresetListDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
}

} // namespace pedaleira
