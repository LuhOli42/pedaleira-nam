#include "MainComponent.h"

#include "Tone3000Panel.h"

#include <algorithm>

namespace pedaleira
{

namespace
{
    constexpr int blockWidth = 130;
    constexpr int blockHeight = 90;
    constexpr int blockGap = 10;
}

MainComponent::MainComponent()
{
    registerBuiltInEffects (registry);

    addAndMakeVisible (presetBadge);
    presetBadge.setJustificationType (juce::Justification::centred);
    presetBadge.setFont (juce::Font (20.0f, juce::Font::bold));
    presetBadge.setColour (juce::Label::textColourId, juce::Colours::grey);
    presetBadge.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1c1c1c));

    addAndMakeVisible (titleLabel);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));

    addAndMakeVisible (cpuLabel);
    cpuLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (settingsButton);
    settingsButton.onClick = [this] { showSettingsPanel(); };

    // Not made visible -- OverlayHost manages its own visibility as cards
    // get pushed/popped (see AGENT.md's UI/UX Design Philosophy).
    addChildComponent (overlayHost);

    chainViewport.setViewedComponent (&chainContainer, false);
    chainViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (chainViewport);

    chainContainer.addAndMakeVisible (addButton);
    addButton.onClick = [this] { showAddEffectMenu(); }; // append at the end

    chainContainer.onLineClicked = [this] (int clickX)
    {
        // Which slot the click landed in, translated to an insertion index:
        // a click in the gap between block k-1 and block k inserts at k.
        const int index = juce::jlimit (0, blocks.size(), clickX / (blockWidth + blockGap));
        showAddEffectMenu (index);
    };

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
    setSize (960, 560);
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
    block->onClicked = [this, raw] { selectBlock (raw); };
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
    int x = 0;
    std::vector<juce::Rectangle<float>> bounds;

    for (auto* block : blocks)
    {
        block->setBounds (x, 0, blockWidth, blockHeight);
        bounds.emplace_back (block->getBounds().toFloat());
        x += blockWidth + blockGap;
    }
    addButton.setBounds (x, 0, blockWidth, blockHeight);
    x += blockWidth;

    chainContainer.setSize (juce::jmax (x, chainViewport.getWidth()), blockHeight);
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

    // Where it was dropped, translated back into a slot index on the clean grid.
    const int centreX = blockComp.getBounds().getCentreX();
    const int newIndex = juce::jlimit (0, blocks.size() - 1, centreX / (blockWidth + blockGap));

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
    auto names = registry.getRegisteredNames();

    juce::PopupMenu menu;
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i]);

    menu.showMenuAsync (juce::PopupMenu::Options(),
        [this, names, insertAtIndex] (int result)
        {
            if (result > 0 && result - 1 < names.size())
                addEffect (names[result - 1], insertAtIndex);
        });
}

juce::File MainComponent::getModelsDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("PedaleiraNAM")
               .getChildFile ("models");
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
    auto chainRow = area.removeFromTop (blockHeight + 12);
    inputSelector.setBounds (chainRow.removeFromLeft (blockWidth).withHeight (blockHeight));
    chainRow.removeFromLeft (8);
    outputSelector.setBounds (chainRow.removeFromRight (blockWidth).withHeight (blockHeight));
    chainRow.removeFromRight (8);
    chainViewport.setBounds (chainRow);
    layoutChain();

    area.removeFromTop (8);

    // The detail drawer only exists once a block is selected, and even
    // then it's capped at a quarter of the window -- this used to fill all
    // remaining space like a desktop utility panel, which is exactly what
    // AGENT.md's UI/UX Design Philosophy says a pedalboard shouldn't do.
    const int maxPanelHeight = (int) (getHeight() * 0.25f);
    const int panelHeight = selectedProcessor != nullptr ? juce::jmin (area.getHeight(), maxPanelHeight) : 0;
    parameterPanel.setBounds (area.removeFromBottom (panelHeight));
    parameterPanel.setVisible (panelHeight > 0);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141414));
}

} // namespace pedaleira
