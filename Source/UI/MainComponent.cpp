#include "MainComponent.h"

#include "PedaleiraLookAndFeel.h"
#include "PresetListDialog.h"
#include "Tone3000Panel.h"
#include "TouchSizing.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <set>

namespace pedaleira
{

namespace
{
    constexpr int blockGap = 8; // fixed horizontal gap between columns -- see MainComponent::rowGap for the (dynamic) vertical one

    // I/O selectors are a fraction of a block's footprint -- they hold two
    // short lines of text, not an icon, and don't need a block-sized tile.
    constexpr int ioWidth = 64;
    constexpr int ioHeight = 58;

    // Taller than touch::minTapTarget -- this is where the preset
    // number/name live, and per the user's explicit ask they need to be
    // readable from a few feet away on a stage, not just tappable.
    constexpr int topBarHeight = 92;

    // Tuner/BPM-tap/meters bar pinned at the bottom -- see FooterBar.h.
    // Bumped alongside topBarHeight per user request 2026-09-10 ("a parte
    // em vermelho maior e a parte em roxo tbm" -- top bar and footer).
    constexpr int footerHeight = 92;

    // The chain wraps into rows instead of scrolling sideways once it fills
    // the available width -- up to this many visible at once, matching the
    // Quad Cortex Grid reference's fixed 4-row layout (see AGENT.md's UI/UX
    // Design Philosophy). More than that scrolls vertically as a fallback,
    // not a wall.
    constexpr int maxVisibleRows = 4;

    // Same categories as the icon reference sheet (see
    // docs/icons/AGENT-icon-notes.md, whose category names come from the
    // original PT-language reference sheet) -- the "Add effect" menu
    // groups into these submenus once there are enough effect types that a
    // flat list stops being manageable. Order here is the order submenus
    // appear in. Translated to English 2026-09-11 per user request (all
    // app-visible text in English) -- the icon doc keeps the original PT
    // labels since that's what the actual reference sheet uses, but
    // nothing user-facing in the running app should be in Portuguese.
    const juce::StringArray categoryOrder { "Amplifiers", "Dynamics", "Drive", "Modulation",
                                             "Delay", "Reverb", "Filter/FX", "Utility" };

    juce::String categoryForDisplayName (const juce::String& displayName)
    {
        if (displayName == "Noise Gate" || displayName == "Compressor")
            return "Dynamics";
        if (displayName == "Overdrive")
            return "Drive";
        if (displayName == "Neural Amp" || displayName == "Neural Amp + Cab"
            || displayName == "Neural Pedal" || displayName == "Cab")
            return "Amplifiers";
        if (displayName == "Reverb" || displayName == "Ambient" || displayName == "Spring")
            return "Reverb";
        if (displayName == "Digital Delay" || displayName == "Tape Delay" || displayName == "Ping Pong"
            || displayName == "Reverse Delay" || displayName == "Hold")
            return "Delay";
        return "Other"; // shouldn't normally happen -- a new effect type that hasn't been categorised yet
    }
}

MainComponent::MainComponent()
{
    registerBuiltInEffects (registry);

    addAndMakeVisible (presetBadge);
    presetBadge.onClicked = [this] { showPresetsPanel(); };

    addChildComponent (quickSaveButton); // only shown once a preset is actually loaded -- see updatePresetDisplay()
    quickSaveButton.onClick = [this] { if (currentPresetName.isNotEmpty()) savePresetAs (currentPresetName); };

    addAndMakeVisible (titleLabel);
    titleLabel.setFont (juce::Font (juce::FontOptions (42.0f, juce::Font::bold)));

    // The heavier display weight (Sora ExtraBold), not just "bold" --
    // readable from a few feet away is the actual requirement here (this
    // is a stage instrument), see PresetBadge.h and AGENT.md's UI/UX
    // Design Philosophy. Falls back to ordinary bold if the LookAndFeel
    // set up in Main.cpp somehow isn't a PedaleiraLookAndFeel.
    if (auto* laf = dynamic_cast<PedaleiraLookAndFeel*> (&juce::LookAndFeel::getDefaultLookAndFeel()))
    {
        presetBadge.setDisplayTypeface (laf->getExtraBoldTypeface());
        titleLabel.setFont (juce::Font (juce::FontOptions (42.0f).withTypeface (laf->getExtraBoldTypeface())));
    }

    addAndMakeVisible (cpuLabel);
    cpuLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (settingsButton);
    settingsButton.onClick = [this] { showSettingsPanel(); };

    // Not made visible -- OverlayHost manages its own visibility as cards
    // get pushed/popped (see AGENT.md's UI/UX Design Philosophy).
    addChildComponent (overlayHost);

    // The two hardware endpoints exist as real graph nodes (stereo L/R),
    // so a cable to the output is an ordinary connection and "does this
    // reach the output?" is the same graph walk as anything else.
    inputNodeId = graph.addNode (NodeKind::audioInput, {}, "IN", 0, 0, 0, 2);
    outputNodeId = graph.addNode (NodeKind::audioOutput, {}, "OUT", 0, 0, 2, 0);

    addAndMakeVisible (routingCanvas);

    routingCanvas.onEmptyCellClicked = [this] (int lane, int column) { showAddEffectMenu (lane, column); };

    routingCanvas.onConnectionRequested = [this] (PortRef source, PortRef target)
    {
        graph.connect (source, target);
        rebuildSignalGraph();
        routingCanvas.repaint();
    };

    routingCanvas.onConnectionRemoved = [this] (ConnectionId id)
    {
        graph.disconnect (id);
        rebuildSignalGraph();
        routingCanvas.repaint();
    };

    routingCanvas.onNodeMoved = [this] (NodeId id, int lane, int column)
    {
        if (auto* node = graph.findNodeMutable (id))
        {
            node->lane = lane;
            node->column = column;
        }
        layoutNodes(); // cables follow automatically -- they're drawn from live positions
    };

    parameterPanel.onRemoveRequested = [this] (EffectProcessor* p) { removeEffect (p); };
    parameterPanel.setModelsDirectory (getModelsDirectory());
    parameterPanel.setTone3000Manager (tone3000);
    parameterPanel.onPushOverlay = [this] (std::unique_ptr<juce::Component> c) { overlayHost.pushOverlay (std::move (c)); };
    parameterPanel.onPopOverlay  = [this] { overlayHost.popOverlay(); };
    addAndMakeVisible (parameterPanel);

    addAndMakeVisible (footerBar);

    if (! audioEngine.start())
        titleLabel.setText ("Audio device failed to open", juce::dontSendNotification);

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

    layoutNodes();
    // ~1280x850: close to a realistic 10" touch panel candidate resolution
    // (see ARCHITECTURE.md's display section -- exact panel TBD, this is
    // our best stand-in), bumped from a plain 800 to fit the footer bar
    // added 2026-09-10 without re-opening the original clipping bug: the
    // chain area reserves a fixed height for maxVisibleRows regardless of
    // how many blocks exist, so anything else added below it (the footer)
    // has to come out of the SAME fixed budget the parameter drawer draws
    // from -- see AGENTS.md's decision entries on both the original
    // 1000x660-was-too-short bug and the knob-geometry/footer follow-ups
    // for the exact numbers this size is chosen to keep working.
    setSize (1280, 850);
    startTimer (200);
}

MainComponent::~MainComponent()
{
    audioEngine.stop(); // must happen before chain's processors are destroyed by the member destructors below
}

EffectProcessor* MainComponent::processorForNode (NodeId id) const
{
    for (auto* b : blocks)
        if (b->nodeId == id)
            return &b->processor;
    return nullptr;
}

EffectBlockComponent* MainComponent::blockForNode (NodeId id) const
{
    for (auto* b : blocks)
        if (b->nodeId == id)
            return b;
    return nullptr;
}

NodeId MainComponent::lastNodeOnLane (int lane, int beforeColumn) const
{
    NodeId best = invalidNode;
    int bestColumn = -1;

    for (const auto& n : graph.getNodes())
        if (n.kind == NodeKind::effect && n.lane == lane && n.column < beforeColumn && n.column > bestColumn)
        {
            best = n.id;
            bestColumn = n.column;
        }

    return best;
}

void MainComponent::addEffect (const juce::String& registryName, int lane, int column)
{
    auto processor = registry.create (registryName);
    if (processor == nullptr)
        return;

    auto* raw = processor.get();

    if (column < 0)
    {
        // No column asked for: land right after whatever's already on this
        // lane, so "add three effects" builds a chain without patching.
        column = 0;
        for (const auto& n : graph.getNodes())
            if (n.kind == NodeKind::effect && n.lane == lane)
                column = juce::jmax (column, n.column + 1);
    }

    const auto nodeId = graph.addNode (NodeKind::effect, registryName, raw->getName(), lane, column, 1, 1);

    chain.push_back (std::move (processor));

    auto block = std::make_unique<EffectBlockComponent> (*raw);
    block->nodeId = nodeId;
    // Tapping the already-open block closes the drawer instead of just
    // re-opening it on itself -- same block, second tap, gone.
    block->onClicked = [this, raw] { selectBlock (selectedProcessor == raw ? nullptr : raw); };
    block->onDragEnded = [this] (EffectBlockComponent& b) { handleBlockDragEnded (b); };
    routingCanvas.addAndMakeVisible (*block);
    blocks.add (block.release());

    // Auto-patch inline: take over whatever the previous block on this lane
    // was feeding (or the input, for the first block on lane 0), so the
    // common case needs no cabling -- everything stays re-patchable after.
    const auto upstream = lastNodeOnLane (lane, column);
    const PortRef myIn { nodeId, 0 };
    const PortRef myOut { nodeId, 0 };

    if (upstream != invalidNode)
    {
        // Splice in: steal the upstream's existing outgoing cable's target.
        PortRef stolenTarget;
        ConnectionId toRemove = invalidConnection;
        for (const auto& c : graph.getConnections())
            if (c.source == PortRef { upstream, 0 })
            {
                stolenTarget = c.target;
                toRemove = c.id;
                break;
            }

        if (toRemove != invalidConnection)
            graph.disconnect (toRemove);

        graph.connect ({ upstream, 0 }, myIn);
        if (stolenTarget.isValid())
            graph.connect (myOut, stolenTarget);
    }
    else
    {
        // First block on this lane: feed it from the input, and land it on
        // whichever output port is still free (each input takes one cable,
        // so lane 2's block goes to OUT R once lane 1's has taken OUT L).
        // If both are taken the block stays unpatched on its output side --
        // visibly dimmed, waiting for you to cable it into a merge.
        graph.connect ({ inputNodeId, 0 }, myIn);

        if (const auto* outNode = graph.findNode (outputNodeId))
            for (int port = 0; port < outNode->numInputs; ++port)
                if (graph.connect (myOut, { outputNodeId, port }) != invalidConnection)
                    break;
    }

    rebuildSignalGraph();
    layoutNodes();
    selectBlock (raw);
}

void MainComponent::removeEffect (EffectProcessor* processor)
{
    NodeId removedNode = invalidNode;

    for (int i = 0; i < blocks.size(); ++i)
    {
        if (&blocks[i]->processor == processor)
        {
            removedNode = blocks[i]->nodeId;
            blocks.remove (i);
            break;
        }
    }

    if (removedNode != invalidNode)
    {
        // Heal the path: if this node sat between two others, reconnect them
        // so pulling a block out of the middle doesn't silently break the
        // chain downstream of it.
        PortRef feeder, fed;
        for (const auto& c : graph.getConnections())
        {
            if (c.target.node == removedNode)
                feeder = c.source;
            else if (c.source.node == removedNode)
                fed = c.target;
        }

        graph.removeNode (removedNode);

        if (feeder.isValid() && fed.isValid())
            graph.connect (feeder, fed);
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
    layoutNodes();
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
    // Order comes from the cables (a topological sort of the routing
    // graph), never from the order blocks were added or where they sit.
    // Nodes that don't reach the output are left out entirely -- an
    // unpatched block is audibly nothing, matching what the canvas already
    // shows by dimming it.
    auto signalGraph = std::make_unique<SignalGraph>();

    const auto active = graph.activeNodes();
    const std::set<NodeId> activeSet (active.begin(), active.end());

    for (const auto nodeId : graph.processingOrder())
    {
        if (activeSet.count (nodeId) == 0)
            continue;

        if (auto* processor = processorForNode (nodeId))
            signalGraph->addProcessor (processor);
    }

    audioEngine.setSignalGraph (std::move (signalGraph));
}

void MainComponent::layoutNodes()
{
    // Blocks are placed purely from their node's lane/column -- the canvas
    // owns that geometry so the ports and cables it draws always line up
    // with the real block components sitting on top of it.
    const auto active = graph.activeNodes();
    const std::set<NodeId> activeSet (active.begin(), active.end());

    for (auto* block : blocks)
    {
        if (const auto* node = graph.findNode (block->nodeId))
            block->setBounds (routingCanvas.boundsForCell (node->lane, node->column));

        // A block that isn't on a path from an input to an output is
        // audibly doing nothing (rebuildSignalGraph() leaves it out), so it
        // reads as dimmed -- the "where is the audio going?" question the
        // canvas exists to answer applies to the blocks too, not just the
        // cables.
        block->setAlpha (activeSet.count (block->nodeId) != 0 ? 1.0f : 0.45f);
    }

    // IN/OUT device-routing selectors sit over the canvas's own IN/OUT
    // node blocks (which draw the ports the cables actually land on).
    if (routingCanvas.getWidth() > 0)
    {
        const auto canvasArea = routingCanvas.getBounds();
        inputSelector.setBounds (juce::Rectangle<int> (canvasArea.getX() + 8, canvasArea.getCentreY() - ioHeight / 2,
                                                        ioWidth, ioHeight));
        outputSelector.setBounds (juce::Rectangle<int> (canvasArea.getRight() - ioWidth - 8,
                                                         canvasArea.getCentreY() - ioHeight / 2, ioWidth, ioHeight));
    }

    routingCanvas.repaint();
}

void MainComponent::handleBlockDragEnded (EffectBlockComponent& blockComp)
{
    if (blocks.indexOf (&blockComp) < 0)
    {
        layoutNodes();
        return;
    }

    // Where it was dropped, translated into a lane/column. This moves the
    // block ONLY -- its cables stay exactly as they were and simply follow
    // it, because the topology lives in the graph, not in the position
    // (the whole point of the 2026-09-11 routing rework).
    const auto centre = blockComp.getBounds().getCentre();
    int lane = 0, column = 0;
    routingCanvas.cellForPosition (centre, lane, column);

    if (auto* node = graph.findNodeMutable (blockComp.nodeId))
    {
        // Dropped onto an occupied cell: swap places, rather than stacking
        // two blocks on top of each other.
        for (const auto& other : graph.getNodes())
            if (other.kind == NodeKind::effect && other.id != node->id
                && other.lane == lane && other.column == column)
            {
                if (auto* otherNode = graph.findNodeMutable (other.id))
                {
                    otherNode->lane = node->lane;
                    otherNode->column = node->column;
                }
                break;
            }

        node->lane = lane;
        node->column = column;
    }

    layoutNodes(); // snaps the dragged block back onto the lane grid
}

void MainComponent::showAddEffectMenu (int lane, int column)
{
    auto keys = registry.getRegisteredNames();

    std::map<juce::String, std::vector<juce::String>> keysByCategory;
    for (auto& key : keys)
        keysByCategory[categoryForDisplayName (registry.displayNameForKey (key))].push_back (key);

    // Item IDs are assigned sequentially across every submenu, and idToKey
    // is that same sequence -- juce::PopupMenu only gives an item's ID
    // back, never which submenu it came from, so this flat lookup is what
    // turns that ID back into a registry key.
    juce::PopupMenu menu;
    std::vector<juce::String> idToKey;

    auto addCategory = [&] (const juce::String& category)
    {
        const auto it = keysByCategory.find (category);
        if (it == keysByCategory.end())
            return;

        juce::PopupMenu submenu;
        for (auto& key : it->second)
        {
            idToKey.push_back (key);
            submenu.addItem ((int) idToKey.size(), registry.displayNameForKey (key));
        }
        menu.addSubMenu (category, submenu);
        keysByCategory.erase (it);
    };

    for (auto& category : categoryOrder)
        addCategory (category);

    // Anything left over (e.g. "Other") -- still shown, just last. Collected
    // into a separate list first: addCategory() erases from keysByCategory,
    // which can't happen safely while range-for is iterating that same map.
    juce::StringArray remainingCategories;
    for (auto& [category, categoryKeys] : keysByCategory)
        remainingCategories.add (category);
    for (auto& category : remainingCategories)
        addCategory (category);

    // Big is fine -- comfortable to tap on a 10" touchscreen matters more
    // than compactness (see AGENT.md's UI/UX Design Philosophy).
    menu.showMenuAsync (juce::PopupMenu::Options().withStandardItemHeight (touch::minTapTarget),
        [this, idToKey, lane, column] (int result)
        {
            if (result > 0 && result - 1 < (int) idToKey.size())
                addEffect (idToKey[(size_t) result - 1], lane, column);
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
        savePresetAs (name);
        overlayHost.popOverlay();
    };

    dialog->onPresetChosen = [this] (juce::String name)
    {
        if (auto xml = presets.loadPreset (name))
        {
            applyPresetXml (*xml);
            currentPresetName = name;
            currentPresetNumber = xml->getIntAttribute ("number", 0);
            updatePresetDisplay();
        }
        overlayHost.popOverlay();
    };

    dialog->onDeleteRequested = [this] (juce::String name)
    {
        presets.deletePreset (name);
        if (currentPresetName == name)
        {
            currentPresetName.clear();
            currentPresetNumber = 0;
            updatePresetDisplay();
        }
        overlayHost.popOverlay();
        showPresetsPanel(); // reopen with a refreshed list -- simplest way to reflect the deletion
    };

    dialog->onPopOverlay = [this] { overlayHost.popOverlay(); };

    overlayHost.pushOverlay (std::move (dialog));
}

void MainComponent::savePresetAs (const juce::String& name)
{
    // Reuse the existing number on a resave (same name = same slot, not a
    // new one) -- see PresetManager::numberForExistingPreset()'s comment.
    const int existingNumber = presets.numberForExistingPreset (name);
    const int number = existingNumber > 0 ? existingNumber : presets.nextAvailableNumber();

    auto xml = buildPresetXml();
    xml->setAttribute ("number", number);
    presets.savePreset (name, *xml);

    currentPresetName = name;
    currentPresetNumber = number;
    updatePresetDisplay();
}

void MainComponent::updatePresetDisplay()
{
    presetBadge.setNumber (currentPresetNumber);
    titleLabel.setText (currentPresetName.isNotEmpty() ? currentPresetName : "No preset loaded",
                         juce::dontSendNotification);
    quickSaveButton.setVisible (currentPresetName.isNotEmpty());
    resized(); // quickSaveButton's visibility changes how much room titleLabel gets
}

std::unique_ptr<juce::XmlElement> MainComponent::buildPresetXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("Preset");

    for (int i = 0; i < (int) chain.size(); ++i)
    {
        auto& p = chain[(size_t) i];
        const auto key = registry.keyForDisplayName (p->getName());
        if (key.isEmpty())
            continue; // shouldn't happen -- every chain block was created via the registry

        auto* blockXml = xml->createNewChildElement ("Block");
        blockXml->setAttribute ("key", key);
        blockXml->setAttribute ("bypassed", p->isBypassed());
        // `blocks[i]` is `chain[i]`'s UI counterpart -- always true, they're
        // kept in lockstep everywhere (see AGENTS.md). The node id ties this
        // block to its entry in the routing graph serialised below, which is
        // what actually carries the topology now.
        blockXml->setAttribute ("nodeId", blocks[i]->nodeId);
        blockXml->addChildElement (p->getState().release());
    }

    // The topology itself: nodes + cables. Saved alongside (not instead of)
    // the blocks, because the blocks carry each processor's own parameter
    // state while the graph carries what feeds what.
    xml->addChildElement (graph.toXml().release());

    auto* ioXml = xml->createNewChildElement ("IO");
    ioXml->setAttribute ("inputChannel", audioEngine.getInputChannel());
    ioXml->setAttribute ("outputPairStart", audioEngine.getOutputChannelPair());

    return xml;
}

void MainComponent::applyPresetXml (const juce::XmlElement& xml)
{
    // Tear the current chain down the same way removeEffect() would, one
    // block at a time, so nothing bypasses the graveyard/DeferredReclaimer
    // discipline (rebuildSignalGraph()/layoutNodes() re-run every
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
        block->nodeId = blockXml->getIntAttribute ("nodeId", invalidNode);
        block->onClicked = [this, raw] { selectBlock (selectedProcessor == raw ? nullptr : raw); };
        block->onDragEnded = [this] (EffectBlockComponent& b) { handleBlockDragEnded (b); };
        routingCanvas.addAndMakeVisible (*block);
        blocks.add (block.release());
    }

    // Restore the topology, then re-find the two hardware endpoints in it
    // (their ids come from the saved graph, not from this session's
    // constructor, so the members have to follow).
    if (auto* graphXml = xml.getChildByName ("RoutingGraph"))
    {
        graph.fromXml (*graphXml);

        inputNodeId = outputNodeId = invalidNode;
        for (const auto& n : graph.getNodes())
        {
            if (n.kind == NodeKind::audioInput)
                inputNodeId = n.id;
            else if (n.kind == NodeKind::audioOutput)
                outputNodeId = n.id;
        }
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
    layoutNodes();
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

    // Footer (tuner/BPM-tap/meters) is pinned at the very bottom, reserved
    // BEFORE the drawer's cap math below so the drawer correctly sees less
    // leftover space with it there -- see FooterBar.h.
    footerBar.setBounds (area.removeFromBottom (footerHeight));
    area.removeFromBottom (8);

    // Tall enough for a stage-readable preset number/name (see
    // topBarHeight's comment) -- cpuLabel/settingsButton/quickSaveButton
    // don't need to be that tall themselves, just centred within it.
    auto top = area.removeFromTop (topBarHeight);
    cpuLabel.setBounds (top.removeFromRight (70).withSizeKeepingCentre (70, 20));
    settingsButton.setBounds (top.removeFromRight (touch::minTapTarget)
                                   .withSizeKeepingCentre (touch::minTapTarget, touch::minTapTarget));
    top.removeFromRight (8);
    presetBadge.setBounds (top.removeFromLeft (80));
    top.removeFromLeft (8);
    if (quickSaveButton.isVisible())
    {
        quickSaveButton.setBounds (top.removeFromRight (80).withSizeKeepingCentre (80, touch::minTapTarget));
        top.removeFromRight (8);
    }
    titleLabel.setBounds (top);

    area.removeFromTop (8);

    // `area` now spans the FULL space between the top bar and the footer.
    // The chain always fills ALL of it -- the parameter drawer, when open,
    // OVERLAYS on top of the bottom portion instead of shrinking this area
    // any more, per user request 2026-09-10 ("a aba que tem os knobs...
    // vai sobrepor as linhas").
    //
    // The patchbay canvas fills ALL of it -- the parameter drawer, when
    // open, OVERLAYS the bottom portion instead of shrinking this area,
    // per user request 2026-09-10 ("a aba que tem os knobs... vai sobrepor
    // as linhas"). The canvas itself works out lane heights and column
    // widths from these bounds (RoutingCanvas::resized()).
    routingCanvas.setBounds (area);
    layoutNodes();

    // The detail drawer only exists once a block is selected. Its height
    // fits whatever the current processor actually needs (so a 1-2 knob
    // pedal never scrolls), floored at enough for one knob row and capped
    // at ~45% of the window -- it used to just always fill a quarter
    // of the window regardless of content, which either clipped a
    // knob-heavy processor or left a tiny 1-2 knob one scrolling for no
    // reason. See AGENT.md's UI/UX Design Philosophy: a drawer, never a
    // permanent desktop-style panel -- and now, per the OVERLAY change
    // above, positioned over the bottom of the SAME area the chain just
    // used rather than carved out of it beforehand.
    int panelHeight = 0;
    if (selectedProcessor != nullptr)
    {
        constexpr int floorForOneKnobRow = 240; // one unified button row + exactly one row of knobs
        const int cap = juce::jmax ((int) (getHeight() * 0.45f), floorForOneKnobRow);
        const int preferred = parameterPanel.getPreferredContentHeight (area.getWidth());
        panelHeight = juce::jmin (area.getHeight(), juce::jmin (preferred, cap));
    }
    parameterPanel.setBounds (area.removeFromBottom (panelHeight));
    parameterPanel.setVisible (panelHeight > 0);
    if (panelHeight > 0)
    {
        parameterPanel.toFront (false); // overlays the canvas -- must paint after it
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141414));

    // The signal path itself is no longer drawn here at all: it's cables
    // between ports, drawn by RoutingCanvas from the graph. The old
    // inter-row connector stubs this used to draw only made sense while
    // "the chain" was a strictly ordered grid that wrapped from the end of
    // one row to the start of the next -- with real routing there is no
    // such implicit row-to-row link to draw.
}

} // namespace pedaleira
