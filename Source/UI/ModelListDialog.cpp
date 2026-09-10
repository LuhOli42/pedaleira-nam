#include "ModelListDialog.h"

namespace pedaleira
{

ModelListDialog::ModelListDialog (juce::String titleText, juce::Array<juce::File> installedFiles,
                                   juce::File importFolderToUse, juce::String importWildcardToUse)
    : files (std::move (installedFiles))
    , importFolder (std::move (importFolderToUse))
    , importWildcard (std::move (importWildcardToUse))
{
    addAndMakeVisible (titleLabel);
    titleLabel.setText (titleText, juce::dontSendNotification);
    titleLabel.setFont (juce::Font (16.0f, juce::Font::bold));

    addAndMakeVisible (listBox);
    listBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff141414));
    listBox.setRowHeight (44);
    listBox.setVisible (! files.isEmpty());

    addAndMakeVisible (emptyStateLabel);
    emptyStateLabel.setText ("Nothing installed yet.\nSearch TONE3000 to add some gear.", juce::dontSendNotification);
    emptyStateLabel.setJustificationType (juce::Justification::centred);
    emptyStateLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    emptyStateLabel.setMinimumHorizontalScale (1.0f);
    emptyStateLabel.setVisible (files.isEmpty());

    addAndMakeVisible (importButton);
    importButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Select a file...", importFolder, importWildcard);
        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                auto file = chooser.getResult();
                if (file != juce::File() && onFileChosen)
                    onFileChosen (file);
                if (onPopOverlay)
                    onPopOverlay();
            });
    };

    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { if (onPopOverlay) onPopOverlay(); };

    setSize (420, 480);
}

int ModelListDialog::getNumRows()
{
    return files.size();
}

void ModelListDialog::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= files.size())
        return;

    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff2d5c56));

    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    g.drawText (files.getReference (rowNumber).getFileNameWithoutExtension(), 14, 0, width - 24, height,
                juce::Justification::centredLeft);
}

void ModelListDialog::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= files.size())
        return;

    if (onFileChosen)
        onFileChosen (files.getReference (row));
    if (onPopOverlay)
        onPopOverlay();
}

void ModelListDialog::resized()
{
    auto area = getLocalBounds().reduced (14);

    titleLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    auto bottomRow = area.removeFromBottom (34);
    closeButton.setBounds (bottomRow.removeFromRight (90));
    bottomRow.removeFromRight (8);
    importButton.setBounds (bottomRow);

    area.removeFromBottom (10);
    listBox.setBounds (area);
    emptyStateLabel.setBounds (area);
}

void ModelListDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
}

} // namespace pedaleira
