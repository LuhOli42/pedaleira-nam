#include "ParameterPanel.h"

#include "ModelListDialog.h"
#include "Tone3000SearchDialog.h"
#include "../Tone3000/GearRouting.h"

namespace pedaleira
{

ParameterPanel::ParameterPanel()
{
    addAndMakeVisible (titleLabel);
    titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));

    addAndMakeVisible (statusLabel);
    statusLabel.setFont (juce::Font (13.0f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible (bypassToggle);
    bypassToggle.onClick = [this] { if (current != nullptr) current->setBypassed (bypassToggle.getToggleState()); };

    addAndMakeVisible (browseInstalledButton);
    browseInstalledButton.onClick = [this] { browseInstalledModels(); };

    addAndMakeVisible (searchTone3000Button);
    searchTone3000Button.onClick = [this] { openTone3000Search(); };

    addAndMakeVisible (removeButton);
    removeButton.onClick = [this] { if (current != nullptr && onRemoveRequested) onRemoveRequested (current); };

    rebuildForCurrentProcessor();
}

void ParameterPanel::setProcessor (EffectProcessor* processorToEdit)
{
    current = processorToEdit;
    rebuildForCurrentProcessor();
}

void ParameterPanel::refresh()
{
    if (current != nullptr)
    {
        statusLabel.setText (current->getStatusText(), juce::dontSendNotification);
        bypassToggle.setToggleState (current->isBypassed(), juce::dontSendNotification);
    }
}

void ParameterPanel::rebuildForCurrentProcessor()
{
    for (auto& row : sliders)
    {
        removeChildComponent (row.slider.get());
        removeChildComponent (row.label.get());
    }
    sliders.clear();

    if (current == nullptr)
    {
        // Nothing to show at all -- no title, no hint text, just the flat
        // black background from paint(). Per AGENT.md's UI/UX Design
        // Philosophy: this panel is a detail drawer that only exists once
        // you've tapped a block, not a permanent toolbar with an idle state.
        titleLabel.setText ({}, juce::dontSendNotification);
        statusLabel.setText ({}, juce::dontSendNotification);
        bypassToggle.setVisible (false);
        browseInstalledButton.setVisible (false);
        searchTone3000Button.setVisible (false);
        removeButton.setVisible (false);
        resized();
        return;
    }

    titleLabel.setText (current->getName(), juce::dontSendNotification);
    bypassToggle.setVisible (true);
    bypassToggle.setToggleState (current->isBypassed(), juce::dontSendNotification);
    browseInstalledButton.setVisible (current->wantsModelFile());
    searchTone3000Button.setVisible (current->wantsModelFile() && tone3000 != nullptr);
    removeButton.setVisible (true);

    if (auto* group = current->getParameters())
    {
        for (auto* p : group->getParameters (true))
        {
            if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*> (p))
            {
                SliderRow row;
                row.param = floatParam;
                row.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                               juce::Slider::TextBoxBelow);
                row.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);

                row.label = std::make_unique<juce::Label> (juce::String(), floatParam->getName (64));
                row.label->setJustificationType (juce::Justification::centred);
                row.label->setFont (11.5f);

                const auto range = floatParam->getNormalisableRange();
                row.slider->setRange (range.start, range.end, range.interval > 0.0f ? range.interval : 0.01);
                row.slider->setValue (floatParam->get(), juce::dontSendNotification);

                auto* rawSlider = row.slider.get();
                row.slider->onValueChange = [floatParam, rawSlider]
                {
                    *floatParam = (float) rawSlider->getValue();
                };

                addAndMakeVisible (*row.label);
                addAndMakeVisible (*row.slider);
                sliders.push_back (std::move (row));
            }
        }
    }

    refresh();
    resized();
}

void ParameterPanel::browseInstalledModels()
{
    if (current == nullptr)
        return;

    const juce::String processorName (current->getName());

    // Every block only ever sees its OWN category subfolder -- a Neural
    // Pedal block's list can never show an amp capture, because it never
    // looks in the amps/ folder at all. BUG FIXED HERE: this directory used
    // to only be used if it already existed on disk -- never true before
    // the first download of that category, so every block silently fell
    // back to sharing one flat folder and categories looked unseparated.
    const auto subfolder = tone3000routing::subfolderForProcessorName (processorName);
    const auto folder = subfolder.isNotEmpty() ? modelsDir.getChildFile (subfolder) : modelsDir;
    folder.createDirectory();

    const auto wildcard = tone3000routing::fileWildcardForProcessorName (processorName);
    juce::Array<juce::File> files;
    for (auto& f : folder.findChildFiles (juce::File::findFiles, false, wildcard))
        files.add (f);

    auto dialog = std::make_unique<ModelListDialog> (processorName + " -- installed", files, folder, wildcard);

    dialog->onFileChosen = [this] (juce::File file)
    {
        if (current == nullptr)
            return;

        try
        {
            current->loadModelFile (file);
            refresh();
        }
        catch (const std::exception& e)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon, "Failed to load model", e.what());
        }
    };
    dialog->onPopOverlay = [this] { if (onPopOverlay) onPopOverlay(); };

    if (onPushOverlay)
        onPushOverlay (std::move (dialog));
}

void ParameterPanel::openTone3000Search()
{
    if (current == nullptr || tone3000 == nullptr)
        return;

    const juce::String processorName (current->getName());
    const auto gearFilter = tone3000routing::gearFilterForProcessorName (processorName);
    const auto subfolder = tone3000routing::subfolderForProcessorName (processorName);
    const auto destinationFolder = subfolder.isNotEmpty() ? modelsDir.getChildFile (subfolder) : modelsDir;

    auto dialog = std::make_unique<Tone3000SearchDialog> (*tone3000, gearFilter, destinationFolder);

    dialog->onFileReady = [this] (juce::File file)
    {
        if (current == nullptr)
            return;

        try
        {
            current->loadModelFile (file);
            refresh();
        }
        catch (const std::exception& e)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon, "Failed to load model", e.what());
        }
    };
    dialog->onPopOverlay = [this] { if (onPopOverlay) onPopOverlay(); };

    if (onPushOverlay)
        onPushOverlay (std::move (dialog));
}

void ParameterPanel::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto top = area.removeFromTop (28);
    removeButton.setBounds (top.removeFromRight (80));
    bypassToggle.setBounds (top.removeFromRight (110));
    titleLabel.setBounds (top);

    statusLabel.setBounds (area.removeFromTop (20));

    if (browseInstalledButton.isVisible() || searchTone3000Button.isVisible())
    {
        auto fileRow = area.removeFromTop (28);
        if (searchTone3000Button.isVisible())
            searchTone3000Button.setBounds (fileRow.removeFromRight (170));
        if (browseInstalledButton.isVisible())
        {
            if (searchTone3000Button.isVisible())
                fileRow.removeFromRight (8);
            browseInstalledButton.setBounds (fileRow.removeFromLeft (170));
        }
    }

    area.removeFromTop (8);

    // A pedal-panel-style row of knobs, wrapping to a new row if the panel
    // is narrow -- not the stacked full-width sliders this used to be.
    constexpr int cellWidth = 92;
    constexpr int cellHeight = 106;
    constexpr int knobSize = 66;

    int x = area.getX();
    int y = area.getY();

    for (auto& row : sliders)
    {
        if (x + cellWidth > area.getRight() && x > area.getX())
        {
            x = area.getX();
            y += cellHeight;
        }

        row.label->setBounds (x, y, cellWidth, 16);
        row.slider->setBounds (x + (cellWidth - knobSize) / 2, y + 18, knobSize, knobSize + 22);
        x += cellWidth;
    }
}

void ParameterPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));
}

} // namespace pedaleira
