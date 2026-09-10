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

    addChildComponent (tone3000SearchField);
    tone3000SearchField.setTextToShowWhenEmpty ("Search TONE3000...", juce::Colours::grey);
    tone3000SearchField.onReturnKey = [this] { doTone3000Search(); };

    addChildComponent (tone3000SearchButton);
    tone3000SearchButton.onClick = [this] { doTone3000Search(); };

    addChildComponent (tone3000ResultsList);
    tone3000ResultsList.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff161616));

    addChildComponent (tone3000DownloadButton);
    tone3000DownloadButton.onClick = [this] { doTone3000DownloadSelected(); };

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

    tone3000Results.clear();
    tone3000ResultsList.updateContent();

    if (current == nullptr)
    {
        titleLabel.setText ("No block selected -- click one in the chain above, or add one", juce::dontSendNotification);
        statusLabel.setText ({}, juce::dontSendNotification);
        bypassToggle.setVisible (false);
        loadModelButton.setVisible (false);
        removeButton.setVisible (false);
        tone3000SearchField.setVisible (false);
        tone3000SearchButton.setVisible (false);
        tone3000ResultsList.setVisible (false);
        tone3000DownloadButton.setVisible (false);
        resized();
        return;
    }

    titleLabel.setText (current->getName(), juce::dontSendNotification);
    bypassToggle.setVisible (true);
    bypassToggle.setToggleState (current->isBypassed(), juce::dontSendNotification);
    loadModelButton.setVisible (current->wantsModelFile());
    removeButton.setVisible (true);

    // Contextual search only shows up for blocks that take a file, and only
    // once TONE3000 is wired in at all (MainComponent::setTone3000Manager) --
    // the integration stays fully optional either way.
    const bool showTone3000Search = current->wantsModelFile() && tone3000 != nullptr;
    tone3000SearchField.setVisible (showTone3000Search);
    tone3000SearchButton.setVisible (showTone3000Search);
    tone3000ResultsList.setVisible (showTone3000Search);
    tone3000DownloadButton.setVisible (showTone3000Search);

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

void ParameterPanel::doTone3000Search()
{
    if (current == nullptr || tone3000 == nullptr)
        return;

    const auto query = tone3000SearchField.getText().trim();
    if (query.isEmpty())
        return;

    // The whole point of moving search in here: no manual gear picker --
    // the block you're editing decides what "amp"/"pedal"/"cab"/"space" to
    // filter for. See GearRouting.h.
    const auto gearFilter = tone3000routing::gearFilterForProcessorName (current->getName());

    statusLabel.setText ("Searching TONE3000 for \"" + query + "\"...", juce::dontSendNotification);

    tone3000->searchTones (query, gearFilter,
        [this] (bool success, std::vector<Tone3000Manager::Tone> found, juce::String error)
        {
            if (! success)
            {
                statusLabel.setText (error, juce::dontSendNotification);
                return;
            }

            tone3000Results = std::move (found);
            tone3000ResultsList.updateContent();
            tone3000ResultsList.deselectAllRows();
            statusLabel.setText (juce::String ((int) tone3000Results.size()) + " result(s).", juce::dontSendNotification);
        });
}

void ParameterPanel::doTone3000DownloadSelected()
{
    if (current == nullptr || tone3000 == nullptr)
        return;

    const int row = tone3000ResultsList.getSelectedRow();
    if (row < 0 || row >= (int) tone3000Results.size())
    {
        statusLabel.setText ("Select a result first.", juce::dontSendNotification);
        return;
    }

    const auto& toneResult = tone3000Results[(size_t) row];
    const auto route = tone3000routing::routeFor (toneResult.gear, toneResult.format);

    if (! route.supported)
    {
        statusLabel.setText ("\"" + toneResult.title + "\" is format \"" + toneResult.format
                                  + "\", which this engine can't load.",
                              juce::dontSendNotification);
        return;
    }

    const auto safeName = juce::File::createLegalFileName (
        toneResult.title.isEmpty() ? juce::String (toneResult.id) : toneResult.title);
    const auto destination = modelsDir.getChildFile (route.subfolder).getChildFile (safeName + route.fileExtension);

    statusLabel.setText ("Downloading \"" + toneResult.title + "\"...", juce::dontSendNotification);

    tone3000->downloadFirstModelForTone (toneResult.id, destination,
        [this, destination] (bool success, juce::String error)
        {
            if (! success)
            {
                statusLabel.setText (error, juce::dontSendNotification);
                return;
            }

            // Straight into the block that was already selected when the
            // search started -- no guessing which block a download belongs
            // to, because the search itself was already scoped to this one.
            if (current != nullptr)
            {
                try
                {
                    current->loadModelFile (destination);
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

int ParameterPanel::getNumRows()
{
    return (int) tone3000Results.size();
}

void ParameterPanel::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int) tone3000Results.size())
        return;

    const auto& toneResult = tone3000Results[(size_t) rowNumber];

    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff2d5c56));

    g.setColour (juce::Colours::white);
    g.setFont (13.0f);
    g.drawText (toneResult.title, 8, 0, width - 16, height / 2, juce::Justification::centredLeft);

    g.setColour (juce::Colours::lightgrey);
    g.setFont (10.5f);
    juce::String subtitle = toneResult.author;
    if (toneResult.license.isNotEmpty())
        subtitle += "  ·  " + toneResult.license;
    g.drawText (subtitle, 8, height / 2, width - 16, height / 2, juce::Justification::centredLeft);
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

    if (tone3000SearchField.isVisible())
    {
        area.removeFromTop (8);
        auto searchRow = area.removeFromTop (28);
        tone3000SearchButton.setBounds (searchRow.removeFromRight (150));
        searchRow.removeFromRight (6);
        tone3000SearchField.setBounds (searchRow);

        area.removeFromTop (6);
        auto resultsArea = area.removeFromTop (116);
        tone3000DownloadButton.setBounds (resultsArea.removeFromBottom (26).removeFromRight (170));
        resultsArea.removeFromBottom (4);
        tone3000ResultsList.setBounds (resultsArea);
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
