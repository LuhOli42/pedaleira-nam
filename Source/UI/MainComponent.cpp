#include "MainComponent.h"

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

    addAndMakeVisible (titleLabel);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));

    addAndMakeVisible (cpuLabel);
    cpuLabel.setJustificationType (juce::Justification::centredRight);

    chainViewport.setViewedComponent (&chainContainer, false);
    chainViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (chainViewport);

    chainContainer.addAndMakeVisible (addButton);
    addButton.onClick = [this] { showAddEffectMenu(); };

    parameterPanel.onRemoveRequested = [this] (EffectProcessor* p) { removeEffect (p); };
    addAndMakeVisible (parameterPanel);

    if (! audioEngine.start())
        titleLabel.setText ("Pedaleira NAM (failed to open audio device)", juce::dontSendNotification);

    layoutChain();
    setSize (960, 560);
    startTimer (200);
}

MainComponent::~MainComponent()
{
    audioEngine.stop(); // must happen before chain's processors are destroyed by the member destructors below
}

void MainComponent::addEffect (const juce::String& registryName)
{
    auto processor = registry.create (registryName);
    if (processor == nullptr)
        return;

    auto* raw = processor.get();
    chain.push_back (std::move (processor));

    auto block = std::make_unique<EffectBlockComponent> (*raw);
    block->onClicked = [this, raw] { selectBlock (raw); };
    chainContainer.addAndMakeVisible (*block);
    blocks.add (block.release());

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
    for (auto* block : blocks)
    {
        block->setBounds (x, 0, blockWidth, blockHeight);
        x += blockWidth + blockGap;
    }
    addButton.setBounds (x, 0, blockWidth, blockHeight);
    x += blockWidth;

    chainContainer.setSize (juce::jmax (x, chainViewport.getWidth()), blockHeight);
}

void MainComponent::showAddEffectMenu()
{
    auto names = registry.getRegisteredNames();

    juce::PopupMenu menu;
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i]);

    menu.showMenuAsync (juce::PopupMenu::Options(),
        [this, names] (int result)
        {
            if (result > 0 && result - 1 < names.size())
                addEffect (names[result - 1]);
        });
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
    auto area = getLocalBounds().reduced (12);

    auto top = area.removeFromTop (32);
    cpuLabel.setBounds (top.removeFromRight (120));
    titleLabel.setBounds (top);

    area.removeFromTop (8);
    chainViewport.setBounds (area.removeFromTop (blockHeight + 12));
    layoutChain();

    area.removeFromTop (8);
    parameterPanel.setBounds (area);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141414));
}

} // namespace pedaleira
