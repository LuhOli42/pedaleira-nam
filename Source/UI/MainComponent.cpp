#include "MainComponent.h"

#include "Tone3000Panel.h"
#include "../Tone3000/GearRouting.h"

#include <algorithm>

namespace pedaleira
{

namespace
{
    constexpr int blockWidth = 130;
    constexpr int blockHeight = 90;
    constexpr int blockGap = 10;

    // GearRouting.h gives back an EffectRegistry key ("NAMAmp", "Cab", ...);
    // there's no reverse lookup from a live EffectProcessor* to the key it
    // was created from, so this leans on each concrete processor's
    // getName() being kept in sync with its registry key by construction
    // (see EffectRegistry::registerBuiltInEffects). Good enough without a
    // wider refactor to track registry keys alongside `chain`.
    bool matchesRegistryRole (EffectProcessor* p, const juce::String& registryRole)
    {
        if (p == nullptr)
            return false;

        const juce::String actualName (p->getName());

        if (registryRole == "NAMAmp")      return actualName == "NAM Amp";
        if (registryRole == "NeuralDrive") return actualName == "Neural Drive";
        if (registryRole == "Cab")         return actualName == "Cab";
        if (registryRole == "Reverb")      return actualName == "Reverb";
        return false;
    }
}

MainComponent::MainComponent()
{
    registerBuiltInEffects (registry);

    addAndMakeVisible (titleLabel);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));

    addAndMakeVisible (cpuLabel);
    cpuLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (tone3000Button);
    tone3000Button.onClick = [this] { showTone3000Panel(); };

    chainViewport.setViewedComponent (&chainContainer, false);
    chainViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (chainViewport);

    chainContainer.addAndMakeVisible (addButton);
    addButton.onClick = [this] { showAddEffectMenu(); };

    parameterPanel.onRemoveRequested = [this] (EffectProcessor* p) { removeEffect (p); };
    parameterPanel.setModelsDirectory (getModelsDirectory());
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

juce::File MainComponent::getModelsDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("PedaleiraNAM")
               .getChildFile ("models");
}

void MainComponent::showTone3000Panel()
{
    auto panel = std::make_unique<Tone3000Panel> (tone3000, getModelsDirectory());
    auto* panelPtr = panel.get();

    panelPtr->onModelDownloaded = [this] (juce::File file, juce::String gear, juce::String format)
    {
        loadDownloadedModel (file, gear, format);
    };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (panel.release());
    options.dialogTitle = "TONE3000";
    options.dialogBackgroundColour = juce::Colour (0xff141414);
    options.escapeKeyTriggersCloseButton = true;
    // JUCE-drawn decorations, not native -- same reasoning as MainWindow:
    // native decorations forwarded through distrobox/Wayland are the
    // suspected reason the main window's close button was easy to hit
    // unintentionally, and here they apparently made the dialog un-closable
    // (no working title bar at all) instead.
    options.useNativeTitleBar = false;
    options.resizable = true;

    auto* window = options.launchAsync();
    panelPtr->onRequestClose = [window] { if (window != nullptr) window->exitModalState (0); };
}

void MainComponent::loadDownloadedModel (const juce::File& file, const juce::String& gear, const juce::String& format)
{
    // Tone3000Panel already refused unsupported formats before downloading,
    // so route.supported is expected true here -- this call just decides
    // WHICH block type the file belongs in.
    const auto route = tone3000routing::routeFor (gear, format);

    EffectProcessor* target = nullptr;

    if (route.registryRole.isNotEmpty() && matchesRegistryRole (selectedProcessor, route.registryRole))
    {
        target = selectedProcessor; // already have the right kind of block selected
    }
    else if (route.registryRole.isNotEmpty())
    {
        addEffect (route.registryRole); // creates it, adds to the chain, and selects it
        target = selectedProcessor;
    }
    else if (selectedProcessor != nullptr && selectedProcessor->wantsModelFile())
    {
        target = selectedProcessor; // outboard/experimental gear -- no fixed role, use whatever's selected
    }

    if (target == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon, "Model downloaded",
            "Saved to:\n" + file.getFullPathName()
                + "\n\nNo matching block type in the chain -- add one manually (\"" + gear
                + "\" gear), then load it from there.");
        return;
    }

    try
    {
        target->loadModelFile (file);
        parameterPanel.refresh();
    }
    catch (const std::exception& e)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon, "Failed to load model", e.what());
    }
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
    tone3000Button.setBounds (top.removeFromRight (110).reduced (4, 0));
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
