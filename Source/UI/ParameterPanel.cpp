#include "ParameterPanel.h"

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

    addAndMakeVisible (loadModelButton);
    loadModelButton.onClick = [this] { chooseAndLoadModelFile(); };

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
        titleLabel.setText ("No block selected -- click one in the chain above, or add one", juce::dontSendNotification);
        statusLabel.setText ({}, juce::dontSendNotification);
        bypassToggle.setVisible (false);
        loadModelButton.setVisible (false);
        removeButton.setVisible (false);
        resized();
        return;
    }

    titleLabel.setText (current->getName(), juce::dontSendNotification);
    bypassToggle.setVisible (true);
    bypassToggle.setToggleState (current->isBypassed(), juce::dontSendNotification);
    loadModelButton.setVisible (current->wantsModelFile());
    removeButton.setVisible (true);

    if (auto* group = current->getParameters())
    {
        for (auto* p : group->getParameters (true))
        {
            if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*> (p))
            {
                SliderRow row;
                row.param = floatParam;
                row.slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
                row.label = std::make_unique<juce::Label> (juce::String(), floatParam->getName (64));

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

void ParameterPanel::chooseAndLoadModelFile()
{
    if (current == nullptr)
        return;

    const juce::String processorName (current->getName());

    // Defaults into the category subfolder a TONE3000 download of this
    // type would have landed in (see GearRouting.h) -- if it exists and
    // has something in it, you shouldn't need to hunt for the file you
    // just downloaded.
    auto startDirectory = modelsDir;
    const auto subfolder = tone3000routing::subfolderForProcessorName (processorName);
    if (subfolder.isNotEmpty())
    {
        const auto candidate = modelsDir.getChildFile (subfolder);
        if (candidate.isDirectory())
            startDirectory = candidate;
    }

    fileChooser = std::make_unique<juce::FileChooser> (
        "Select a file...", startDirectory, tone3000routing::fileWildcardForProcessorName (processorName));

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();

            if (file != juce::File() && current != nullptr)
            {
                try
                {
                    current->loadModelFile (file);
                }
                catch (const std::exception& e)
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon, "Failed to load model", e.what());
                }
            }

            refresh();
        });
}

void ParameterPanel::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto top = area.removeFromTop (28);
    removeButton.setBounds (top.removeFromRight (80));
    bypassToggle.setBounds (top.removeFromRight (110));
    titleLabel.setBounds (top);

    statusLabel.setBounds (area.removeFromTop (20));

    if (loadModelButton.isVisible())
        loadModelButton.setBounds (area.removeFromTop (28).removeFromLeft (200));

    area.removeFromTop (8);

    for (auto& row : sliders)
    {
        // 26px was too short for the default LookAndFeel's slider thumb to
        // sit centred on the track -- it rendered detached, floating above
        // the (barely visible) track line. 34px gives it room.
        auto rowArea = area.removeFromTop (34);
        row.label->setBounds (rowArea.removeFromLeft (110));
        row.slider->setBounds (rowArea);
        area.removeFromTop (6);
    }
}

void ParameterPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));
}

} // namespace pedaleira
