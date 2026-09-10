#include "MainComponent.h"

#include "PresetListDialog.h"
#include "Tone3000Panel.h"

#include <algorithm>

namespace pedaleira
{

namespace
{
    constexpr int blockWidth = 110;
    constexpr int blockHeight = 78;
    constexpr int blockGap = 8;

    // I/O selectors are a fraction of a block's footprint -- they hold two
    // short lines of text, not an icon, and don't need a block-sized tile.
    constexpr int ioWidth = 56;
    constexpr int ioHeight = 50;

    // The chain wraps into rows instead of scrolling sideways once it fills
    // the available width -- up to this many visible at once, matching the
    // Quad Cortex Grid reference's fixed 4-row layout (see AGENT.md's UI/UX
    // Design Philosophy). More than that scrolls vertically as a fallback,
    // not a wall.
    constexpr int maxVisibleRows = 4;
}

MainComponent::MainComponent()
{
    registerBuiltInEffects (registry);

    addAndMakeVisible (presetBadge);
    presetBadge.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c1c1c));
    presetBadge.setColour (juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    presetBadge.onClick = [this] { showPresetsPanel(); };

    addAndMakeVisible (titleLabel);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));

    addAndMakeVisible (cpuLabel);
    cpuLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (settingsButton);
    settingsButton.onClick = [this] { showSettingsPanel(); };

    // Not made visible -- OverlayHost manages its own visibility as cards
    // get pushed/popped (see AGENT.md's UI/UX Design Philosophy).
    addChildComponent (overlayHost);

    // Wraps into rows instead of growing sideways now (see layoutChain()),
    // so it's the vertical scrollbar that's the overflow fallback.
    chainViewport.setViewedComponent (&chainContainer, false);
    chainViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (chainViewport);

    chainContainer.addAndMakeVisible (addButton);
    addButton.onClick = [this] { showAddEffectMenu(); }; // append at the end

    chainContainer.onSlotClicked = [this] (int index) { showAddEffectMenu (index); };

    parameterPanel.onRemoveRequested = [this] (EffectProcessor* p) { removeEffect (p); };
    parameterPanel.setModelsDirectory (getModelsDirectory());
    parameterPanel.setTone3000Manager (tone3000);
    parameterPanel.onPushOverlay = [this] (std::unique_ptr<juce::Component> c) { overlayHost.pushOverlay (std::move (c)); };
    parameterPanel.onPopOverlay  = [this] { overlayHost.popOverlay(); };
    addAndMakeVisible (parameterPanel);

    if (! audioEngine.start())
        titleLabel.setText ("Pedaleira NAM (failed to open audio device)", juce::dontSendNotification);

    addAndMakeVisible (inputSelector);
    auto inputNames = audioEngine.getAvailableInputChannelNames();
    if (inputNames.isEmpty())
        inputNames.add ("Default");
    inputSelector.setOptions (inputNames, audioEngine.getInputChannel());
    inputSelector.onSelectionChanged = [this] (int index) { audioEngine.setInputChannel (index); };

    addAndMakeVisible (outputSelector);
    auto outputNames = audioEngine.getAvailableOutputPairNames();
    if (outputNames.isEmpty())
        outputNames.add ("Default");
    outputSelector.setOptions (outputNames, audioEngine.getOutputChannelPair() / 2);
    outputSelector.onSelectionChanged = [this] (int index) { audioEngine.setOutputChannelPair (index * 2); };

    layoutChain();
    setSize (1000, 640); // tall enough for 4 chain rows plus the parameter drawer without clipping
    startTimer (200);
}

MainComponent::~MainComponent()
{
    audioEngine.stop(); // must happen before chain's processors are destroyed by the member destructors below
}

void MainComponent::addEffect (const juce::String& registryName, int insertAtIndex)
{
    auto processor = registry.create (registryName);
    if (processor == nullptr)
        return;

    auto* raw = processor.get();
    const int index = (insertAtIndex < 0 || insertAtIndex > (int) chain.size()) ? (int) chain.size() : insertAtIndex;

    chain.insert (chain.begin() + index, std::move (processor));

    auto block = std::make_unique<EffectBlockComponent> (*raw);
    // Tapping the already-open block closes the drawer instead of just
    // re-opening it on itself -- same block, second tap, gone.
    block->onClicked = [this, raw] { selectBlock (selectedProcessor == raw ? nullptr : raw); };
    block->onDragEnded = [this] (EffectBlockComponent& b) { handleBlockDragEnded (b); };
    chainContainer.addAndMakeVisible (*block);
    blocks.insert (index, block.release());

    rebuildSignalGraph();
    layoutChain();
    selectBlock (raw);
}

void MainComponent::removeEffect (EffectProcessor* processor)
{
    for (int i = 0; i < blocks.size(); ++i)
    {
        if (&blocks[i]->processor == processor)
        {
            blocks.remove (i);
            break;
        }
    }

    for (auto it = chain.begin(); it != chain.end(); ++it)
    {
        if (it->get() == processor)
        {
            graveyard.push_back ({ std::move (*it), juce::Time::getMillisecondCounter() });
            chain.erase (it);
            break;
        }
    }

    if (selectedProcessor == processor)
        selectBlock (nullptr);

    rebuildSignalGraph();
    layoutChain();
}

void MainComponent::selectBlock (EffectProcessor* processor)
{
    selectedProcessor = processor;

    for (auto* block : blocks)
        block->setSelected (&block->processor == processor);

    parameterPanel.setProcessor (processor);
    resized(); // the detail drawer only exists (and only takes up space) once something is selected
}

void MainComponent::rebuildSignalGraph()
{
    auto graph = std::make_unique<SignalGraph>();
    for (auto& p : chain)
        graph->addProcessor (p.get());
    audioEngine.setSignalGraph (std::move (graph));
}

void MainComponent::layoutChain()
{
    // Wraps into rows once it fills the available width, instead of
    // growing sideways forever -- see AGENT.md's UI/UX Design Philosophy
    // (the Quad Cortex Grid reference this is modelled on is a fixed grid,
    // not a horizontally-scrolling strip).
    const int viewportWidth = juce::jmax (blockWidth, chainViewport.getWidth());
    chainColumns = juce::jmax (1, (viewportWidth + blockGap) / (blockWidth + blockGap));

    std::vector<juce::Rectangle<float>> bounds;
    int index = 0;

    for (auto* block : blocks)
    {
        const int row = index / chainColumns;
        const int col = index % chainColumns;
        block->setBounds (col * (blockWidth + blockGap), row * (blockHeight + blockGap), blockWidth, blockHeight);
        bounds.emplace_back (block->getBounds().toFloat());
        ++index;
    }

    const int addRow = index / chainColumns;
    const int addCol = index % chainColumns;
    addButton.setBounds (addCol * (blockWidth + blockGap), addRow * (blockHeight + blockGap), blockWidth, blockHeight);
    ++index;

    const int totalRows = juce::jmax (1, (index + chainColumns - 1) / chainColumns);
    chainContainer.setSize (viewportWidth, totalRows * (blockHeight + blockGap) - blockGap);
    chainContainer.setRowMetrics (chainColumns, blockWidth, blockHeight, blockGap);
    chainContainer.setBlockBounds (std::move (bounds));
}

void MainComponent::handleBlockDragEnded (EffectBlockComponent& blockComp)
{
    const int oldIndex = blocks.indexOf (&blockComp);
    if (oldIndex < 0)
    {
        layoutChain();
        return;
    }

    // Where it was dropped, translated back into a slot index on the clean
    // grid -- row from Y now that dragging isn't locked to one row anymore,
    // column from X, same as layoutChain()'s own row/col math.
    const auto centre = blockComp.getBounds().getCentre();
    const int row = juce::jmax (0, centre.y / (blockHeight + blockGap));
    const int col = juce::jlimit (0, chainColumns - 1, centre.x / (blockWidth + blockGap));
    const int newIndex = juce::jlimit (0, blocks.size() - 1, row * chainColumns + col);

    if (newIndex != oldIndex)
    {
        blocks.move (oldIndex, newIndex);

        // `chain` (the actual processor order SignalGraph reads) has to move
        // in exact lockstep with `blocks` -- std::rotate over the same
        // [old, new] span is what OwnedArray::move does internally, mirrored
        // here by hand since chain is a plain std::vector.
        if (newIndex > oldIndex)
            std::rotate (chain.begin() + oldIndex, chain.begin() + oldIndex + 1, chain.begin() + newIndex + 1);
        else
            std::rotate (chain.begin() + newIndex, chain.begin() + oldIndex, chain.begin() + oldIndex + 1);

        rebuildSignalGraph();
    }

    layoutChain(); // snaps every block, including the dragged one, back onto the clean grid
}

void MainComponent::showAddEffectMenu (int insertAtIndex)
{
    auto keys = registry.getRegisteredNames();

    juce::PopupMenu menu;
    for (int i = 0; i < keys.size(); ++i)
        menu.addItem (i + 1, registry.displayNameForKey (keys[i])); // a human wrote this, not a raw registry key

    menu.showMenuAsync (juce::PopupMenu::Options(),
        [this, keys, insertAtIndex] (int result)
        {
            if (result > 0 && result - 1 < keys.size())
                addEffect (keys[result - 1], insertAtIndex);
        });
}

juce::File MainComponent::getModelsDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("PedaleiraNAM")
               .getChildFile ("models");
}

juce::File MainComponent::getPresetsDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("PedaleiraNAM")
               .getChildFile ("presets");
}

void MainComponent::showSettingsPanel()
{
    // The app's one Settings screen, reached through the "..." button --
    // just login/key right now, search and download live in ParameterPanel,
    // contextual to whichever block is selected. See Tone3000Panel.h and
    // AGENT.md's UI/UX Design Philosophy for why this is a card pushed onto
    // overlayHost rather than a separate OS window.
    auto panel = std::make_unique<Tone3000Panel> (tone3000);
    panel->onPushOverlay = [this] (std::unique_ptr<juce::Component> c) { overlayHost.pushOverlay (std::move (c)); };
    panel->onPopOverlay  = [this] { overlayHost.popOverlay(); };

    overlayHost.pushOverlay (std::move (panel));
}

void MainComponent::showPresetsPanel()
{
    auto dialog = std::make_unique<PresetListDialog> (presets.listPresetNames());

    dialog->onSaveRequested = [this] (juce::String name)
    {
        presets.savePreset (name, *buildPresetXml());
        currentPresetName = name;
        presetBadge.setButtonText (name);
        overlayHost.popOverlay();
    };

    dialog->onPresetChosen = [this] (juce::String name)
    {
        if (auto xml = presets.loadPreset (name))
        {
            applyPresetXml (*xml);
            currentPresetName = name;
            presetBadge.setButtonText (name);
        }
        overlayHost.popOverlay();
    };

    dialog->onDeleteRequested = [this] (juce::String name)
    {
        presets.deletePreset (name);
        if (currentPresetName == name)
        {
            currentPresetName.clear();
            presetBadge.setButtonText (juce::String::fromUTF8 ("\xe2\x80\x94"));
        }
        overlayHost.popOverlay();
        showPresetsPanel(); // reopen with a refreshed list -- simplest way to reflect the deletion
    };

    dialog->onPopOverlay = [this] { overlayHost.popOverlay(); };

    overlayHost.pushOverlay (std::move (dialog));
}

std::unique_ptr<juce::XmlElement> MainComponent::buildPresetXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("Preset");

    for (auto& p : chain)
    {
        const auto key = registry.keyForDisplayName (p->getName());
        if (key.isEmpty())
            continue; // shouldn't happen -- every chain block was created via the registry

        auto* blockXml = xml->createNewChildElement ("Block");
        blockXml->setAttribute ("key", key);
        blockXml->setAttribute ("bypassed", p->isBypassed());
        blockXml->addChildElement (p->getState().release());
    }

    auto* ioXml = xml->createNewChildElement ("IO");
    ioXml->setAttribute ("inputChannel", audioEngine.getInputChannel());
    ioXml->setAttribute ("outputPairStart", audioEngine.getOutputChannelPair());

    return xml;
}

void MainComponent::applyPresetXml (const juce::XmlElement& xml)
{
    // Tear the current chain down the same way removeEffect() would, one
    // block at a time, so nothing bypasses the graveyard/DeferredReclaimer
    // discipline (rebuildSignalGraph()/layoutChain() re-run every
    // iteration -- wasteful but this only ever happens from a menu tap,
    // never the audio thread).
    while (! blocks.isEmpty())
        removeEffect (&blocks[0]->processor);

    for (int i = 0; i < xml.getNumChildElements(); ++i)
    {
        auto* blockXml = xml.getChildElement (i);
        if (blockXml->getTagName() != "Block")
            continue;

        auto processor = registry.create (blockXml->getStringAttribute ("key"));
        if (processor == nullptr)
            continue; // an unknown key (e.g. a preset from a future build) -- skip, don't fail the whole load

        if (auto* stateXml = blockXml->getChildByName ("EffectState"))
            processor->setState (*stateXml);
        processor->setBypassed (blockXml->getBoolAttribute ("bypassed", false));

        auto* raw = processor.get();
        chain.push_back (std::move (processor));

        auto block = std::make_unique<EffectBlockComponent> (*raw);
        block->onClicked = [this, raw] { selectBlock (selectedProcessor == raw ? nullptr : raw); };
        block->onDragEnded = [this] (EffectBlockComponent& b) { handleBlockDragEnded (b); };
        chainContainer.addAndMakeVisible (*block);
        blocks.add (block.release());
    }

    if (auto* ioXml = xml.getChildByName ("IO"))
    {
        const int inCh = ioXml->getIntAttribute ("inputChannel", 0);
        const int outPair = ioXml->getIntAttribute ("outputPairStart", 0);
        audioEngine.setInputChannel (inCh);
        audioEngine.setOutputChannelPair (outPair);
        inputSelector.setOptions (audioEngine.getAvailableInputChannelNames(), inCh);
        outputSelector.setOptions (audioEngine.getAvailableOutputPairNames(), outPair / 2);
    }

    rebuildSignalGraph();
    layoutChain();
}

void MainComponent::timerCallback()
{
    cpuLabel.setText ("CPU " + juce::String (audioEngine.getCurrentCpuUsage() * 100.0, 1) + "%",
                       juce::dontSendNotification);

    const auto now = juce::Time::getMillisecondCounter();
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(),
                                      [now] (const RetiredProcessor& r) { return now - r.retiredAtMs >= 600; }),
                      graveyard.end());

    parameterPanel.refresh();
}

void MainComponent::resized()
{
    overlayHost.setBounds (getLocalBounds());

    auto area = getLocalBounds().reduced (12);

    auto top = area.removeFromTop (32);
    cpuLabel.setBounds (top.removeFromRight (70));
    settingsButton.setBounds (top.removeFromRight (36));
    top.removeFromRight (8);
    presetBadge.setBounds (top.removeFromLeft (44));
    top.removeFromLeft (8);
    titleLabel.setBounds (top);

    area.removeFromTop (8);

    // Reserves exactly maxVisibleRows worth of height, like the Quad Cortex
    // Grid reference's fixed grid -- not "one row plus however much the
    // content happens to need".
    const int chainRowsHeight = maxVisibleRows * blockHeight + (maxVisibleRows - 1) * blockGap;
    auto chainRow = area.removeFromTop (chainRowsHeight + 12);

    inputSelector.setBounds (chainRow.removeFromLeft (ioWidth).withSizeKeepingCentre (ioWidth, ioHeight));
    chainRow.removeFromLeft (8);
    outputSelector.setBounds (chainRow.removeFromRight (ioWidth).withSizeKeepingCentre (ioWidth, ioHeight));
    chainRow.removeFromRight (8);
    chainViewport.setBounds (chainRow);
    layoutChain();

    area.removeFromTop (8);

    // The detail drawer only exists once a block is selected. Its height
    // fits whatever the current processor actually needs (so a 1-2 knob
    // pedal never scrolls), floored at enough for one knob row and capped
    // at a quarter of the window -- it used to just always fill a quarter
    // of the window regardless of content, which either clipped a
    // knob-heavy processor or left a tiny 1-2 knob one scrolling for no
    // reason. See AGENT.md's UI/UX Design Philosophy: this is a drawer,
    // never a permanent desktop-style panel.
    int panelHeight = 0;
    if (selectedProcessor != nullptr)
    {
        constexpr int floorForOneKnobRow = 220; // header rows + exactly one row of knobs
        const int cap = juce::jmax ((int) (getHeight() * 0.25f), floorForOneKnobRow);
        const int preferred = parameterPanel.getPreferredContentHeight (area.getWidth());
        panelHeight = juce::jmin (area.getHeight(), juce::jmin (preferred, cap));
    }
    parameterPanel.setBounds (area.removeFromBottom (panelHeight));
    parameterPanel.setVisible (panelHeight > 0);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141414));
}

} // namespace pedaleira
