#include "MainComponent.h"

#include "PedaleiraLookAndFeel.h"
#include "PresetListDialog.h"
#include "Tone3000Panel.h"
#include "TouchSizing.h"

#include <algorithm>
#include <map>
#include <numeric>

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

    // Wraps into rows instead of growing sideways now (see layoutChain()),
    // so it's the vertical scrollbar that's the overflow fallback.
    chainViewport.setViewedComponent (&chainContainer, false);
    chainViewport.setScrollBarsShown (false, false); // driven by chainScrollBar instead -- see its member comment
    chainViewport.onScrolled = [this] { syncChainScrollBar(); repaint(); };
    addAndMakeVisible (chainViewport);

    chainScrollBar.setAutoHide (false);
    chainScrollBar.addListener (this);
    addAndMakeVisible (chainScrollBar);
    chainContainer.setMaxVisibleRows (numRows);

    // The only way to add a block now -- hover the grid, a "+" appears
    // exactly under the cursor, click it. No permanent dashed-box tile
    // sitting there all the time any more -- see ChainContainer.h.
    chainContainer.onSlotClicked = [this] (int index) { showAddEffectMenu (index); };

    parameterPanel.onRemoveRequested = [this] (EffectProcessor* p) { removeEffect (p); };
    parameterPanel.setModelsDirectory (getModelsDirectory());
    parameterPanel.setTone3000Manager (tone3000);
    parameterPanel.onPushOverlay = [this] (std::unique_ptr<juce::Component> c) { overlayHost.pushOverlay (std::move (c)); };
    parameterPanel.onPopOverlay  = [this] { overlayHost.popOverlay(); };
    addAndMakeVisible (parameterPanel);

    addAndMakeVisible (footerBar);

    if (! audioEngine.start())
        titleLabel.setText ("Audio device failed to open", juce::dontSendNotification);

    // Row 0 starts wired device-in -> device-out so the app still makes
    // sound out of the box; rows 1-3 start unrouted, showing a "+" at both
    // ends. Rows are independent until you say otherwise (user decision
    // 2026-09-11).
    rowRouting[0].inputChannel = audioEngine.getInputChannel();
    rowRouting[0].dest = RowRouting::Dest::device;
    rowRouting[0].destOutputPair = audioEngine.getOutputChannelPair();

    for (int row = 0; row < numRows; ++row)
    {
        addAndMakeVisible (rowInputBlocks[(size_t) row]);
        rowInputBlocks[(size_t) row].onClicked = [this, row] { showRowInputMenu (row); };

        addAndMakeVisible (rowOutputBlocks[(size_t) row]);
        rowOutputBlocks[(size_t) row].onClicked = [this, row] { showRowOutputMenu (row); };
    }
    refreshRowEndpoints();

    layoutChain();
    // ~1280x850: close to a realistic 10" touch panel candidate resolution
    // (see ARCHITECTURE.md's display section -- exact panel TBD, this is
    // our best stand-in), bumped from a plain 800 to fit the footer bar
    // added 2026-09-10 without re-opening the original clipping bug: the
    // chain area reserves a fixed height for numRows regardless of
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

void MainComponent::addEffect (const juce::String& registryName, int targetGridSlot)
{
    auto processor = registry.create (registryName);
    if (processor == nullptr)
        return;

    auto* raw = processor.get();

    // < 0 means "no preference" -- append right after the highest occupied
    // slot, same as what clicking the "+" past the current content used to
    // mean before slots could be sparse.
    int slot = targetGridSlot;
    if (slot < 0)
    {
        int maxSlot = -1;
        for (auto* b : blocks)
            maxSlot = juce::jmax (maxSlot, b->gridSlot);
        slot = maxSlot + 1;
    }

    // `blocks`/`chain` stay sorted ascending by gridSlot -- that sort order
    // IS the signal processing order SignalGraph reads directly (see
    // rebuildSignalGraph()), so a block landing at any clicked grid cell
    // (not just the next sequential one, per user request 2026-09-10) just
    // means finding where it falls in that order, not literally inserting
    // at its own slot number as an array index.
    int arrayIndex = 0;
    while (arrayIndex < blocks.size() && blocks[arrayIndex]->gridSlot < slot)
        ++arrayIndex;

    chain.insert (chain.begin() + arrayIndex, std::move (processor));

    auto block = std::make_unique<EffectBlockComponent> (*raw);
    block->gridSlot = slot;
    // Tapping the already-open block closes the drawer instead of just
    // re-opening it on itself -- same block, second tap, gone.
    block->onClicked = [this, raw] { selectBlock (selectedProcessor == raw ? nullptr : raw); };
    block->onDragEnded = [this] (EffectBlockComponent& b) { handleBlockDragEnded (b); };
    chainContainer.addAndMakeVisible (*block);
    blocks.insert (arrayIndex, block.release());

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

void MainComponent::refreshRowEndpoints()
{
    const auto inputNames = audioEngine.getAvailableInputChannelNames();
    const auto outputNames = audioEngine.getAvailableOutputPairNames();

    for (int row = 0; row < numRows; ++row)
    {
        const auto& routing = rowRouting[(size_t) row];

        // Left: the device channel feeding this row, or nothing yet.
        if (routing.inputChannel < 0)
            rowInputBlocks[(size_t) row].setDisplay ({}, {});
        else
            rowInputBlocks[(size_t) row].setDisplay ("IN",
                routing.inputChannel < inputNames.size() ? inputNames[routing.inputChannel] : juce::String ("Default"));

        // Right: a device output pair, another row, or nothing yet.
        switch (routing.dest)
        {
            case RowRouting::Dest::device:
            {
                const int pairIndex = routing.destOutputPair / 2;
                rowOutputBlocks[(size_t) row].setDisplay ("OUT",
                    pairIndex < outputNames.size() ? outputNames[pairIndex] : juce::String ("Default"));
                break;
            }
            case RowRouting::Dest::row:
                rowOutputBlocks[(size_t) row].setDisplay ("TO", "Line " + juce::String (routing.destRow + 1));
                break;
            case RowRouting::Dest::none:
            default:
                rowOutputBlocks[(size_t) row].setDisplay ({}, {});
                break;
        }
    }
}

bool MainComponent::rowLinkWouldLoop (int row, int candidateTarget) const
{
    // Walk forward from the proposed target: if the chain of row->row links
    // leads back to `row`, the audio would have to feed itself.
    int current = candidateTarget;
    for (int guard = 0; guard < numRows + 1; ++guard)
    {
        if (current < 0 || current >= numRows)
            return false;
        if (current == row)
            return true;

        const auto& routing = rowRouting[(size_t) current];
        if (routing.dest != RowRouting::Dest::row)
            return false;
        current = routing.destRow;
    }
    return true; // ran out of guard -- treat as a loop rather than risk one
}

std::vector<int> MainComponent::rowsFeedingInto (int row) const
{
    // Walk BACKWARDS from `row` to whichever row has a device input, then
    // return that path front-to-back. Each row has at most one feeder (a
    // row's dest is a single choice), so this is a simple walk, not a search.
    std::vector<int> path;
    int current = row;

    for (int guard = 0; guard < numRows + 1; ++guard)
    {
        path.insert (path.begin(), current);

        if (rowRouting[(size_t) current].inputChannel >= 0)
            return path; // reached a row that's actually fed by the device

        int feeder = -1;
        for (int candidate = 0; candidate < numRows; ++candidate)
        {
            const auto& routing = rowRouting[(size_t) candidate];
            if (routing.dest == RowRouting::Dest::row && routing.destRow == current)
            {
                feeder = candidate;
                break;
            }
        }

        if (feeder < 0)
            return {}; // nothing feeds this row -- it's silent
        current = feeder;
    }

    return {};
}

void MainComponent::showRowInputMenu (int row)
{
    // Physical inputs only. Receiving FROM another row is expressed on that
    // other row's output tile instead, so one link is never two contradictory
    // settings (user decision 2026-09-11).
    auto inputNames = audioEngine.getAvailableInputChannelNames();
    if (inputNames.isEmpty())
        inputNames.add ("Default");

    juce::PopupMenu menu;
    menu.addItem (1, "Not connected", true, rowRouting[(size_t) row].inputChannel < 0);
    menu.addSeparator();
    for (int i = 0; i < inputNames.size(); ++i)
        menu.addItem (i + 2, inputNames[i], true, rowRouting[(size_t) row].inputChannel == i);

    menu.showMenuAsync (juce::PopupMenu::Options().withStandardItemHeight (touch::minTapTarget),
        [this, row] (int result)
        {
            if (result <= 0)
                return;

            rowRouting[(size_t) row].inputChannel = result == 1 ? -1 : result - 2;

            // Row 0's input is also the device's -- the engine only opens one
            // input channel today, so a second fed row shares it. Selecting
            // per-row physical inputs properly is an engine change (multiple
            // input channels), not a UI one; see AGENTS.md.
            if (rowRouting[(size_t) row].inputChannel >= 0)
                audioEngine.setInputChannel (rowRouting[(size_t) row].inputChannel);

            refreshRowEndpoints();
            rebuildSignalGraph();
            layoutChain();
        });
}

void MainComponent::showRowOutputMenu (int row)
{
    auto outputNames = audioEngine.getAvailableOutputPairNames();
    if (outputNames.isEmpty())
        outputNames.add ("Default");

    const auto& routing = rowRouting[(size_t) row];

    juce::PopupMenu menu;
    menu.addItem (1, "Not connected", true, routing.dest == RowRouting::Dest::none);
    menu.addSeparator();

    for (int i = 0; i < outputNames.size(); ++i)
        menu.addItem (i + 2, "Output " + outputNames[i], true,
                       routing.dest == RowRouting::Dest::device && routing.destOutputPair / 2 == i);

    // ...and the other rows. A row that would loop back into this one is
    // shown greyed rather than hidden, so it's clear WHY it isn't offered.
    menu.addSeparator();
    constexpr int rowItemBase = 100;
    for (int target = 0; target < numRows; ++target)
    {
        if (target == row)
            continue;

        const bool allowed = ! rowLinkWouldLoop (row, target);
        menu.addItem (rowItemBase + target, "Line " + juce::String (target + 1), allowed,
                       routing.dest == RowRouting::Dest::row && routing.destRow == target);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withStandardItemHeight (touch::minTapTarget),
        [this, row, outputCount = outputNames.size()] (int result)
        {
            if (result <= 0)
                return;

            auto& r = rowRouting[(size_t) row];

            if (result == 1)
            {
                r.dest = RowRouting::Dest::none;
            }
            else if (result >= rowItemBase)
            {
                r.dest = RowRouting::Dest::row;
                r.destRow = result - rowItemBase;
            }
            else if (result - 2 < outputCount)
            {
                r.dest = RowRouting::Dest::device;
                r.destOutputPair = (result - 2) * 2;
                audioEngine.setOutputChannelPair (r.destOutputPair);
            }

            refreshRowEndpoints();
            rebuildSignalGraph();
            layoutChain();
        });
}

void MainComponent::rebuildSignalGraph()
{
    // Processing order follows the ROW LINKS, not the flat block array: the
    // blocks of a row run left to right, then whatever row that row feeds,
    // and so on. A row nothing feeds contributes nothing at all -- which is
    // exactly what its dimmed, unrouted endpoints already say on screen.
    auto signalGraph = std::make_unique<SignalGraph>();

    // Start from the row that reaches the device output and walk back to
    // whichever row the device input feeds. Only one row can be the terminus
    // today (the engine has a single output pair); a second one routed to
    // the device is simply not reached.
    std::vector<int> orderedRows;
    for (int row = 0; row < numRows; ++row)
    {
        if (rowRouting[(size_t) row].dest == RowRouting::Dest::device)
        {
            orderedRows = rowsFeedingInto (row);
            break;
        }
    }

    for (const int row : orderedRows)
    {
        // `blocks` is sorted by gridSlot and gridSlot == row * chainColumns +
        // col, so a row's blocks are already contiguous and in column order.
        for (auto* block : blocks)
            if (block->gridSlot / chainColumns == row)
                signalGraph->addProcessor (&block->processor);
    }

    audioEngine.setSignalGraph (std::move (signalGraph));
}

void MainComponent::layoutChain()
{
    // chainColumns is a fixed policy (8) now -- see the member's comment --
    // and blockWidth/blockHeight/rowGap are derived in resized() from the
    // available space before this runs. Wraps into rows once a row fills
    // up, instead of growing sideways forever -- see AGENT.md's UI/UX
    // Design Philosophy (the Quad Cortex Grid reference this is modelled
    // on is a fixed grid, not a horizontally-scrolling strip).
    const int viewportWidth = juce::jmax (blockWidth, chainViewport.getWidth());

    // Each block is positioned by its OWN gridSlot now, not by its index in
    // `blocks` -- a block can sit at any grid cell you clicked (per user
    // request 2026-09-10, "ta adicionando ainda sequencialmente em vez
    // daonde eu clico"), while `blocks`/`chain` themselves stay sorted
    // ascending by gridSlot (see addEffect()/handleBlockDragEnded()), which
    // is what makes that sort order double as the actual signal processing
    // order SignalGraph reads (rebuildSignalGraph()) with no extra
    // bookkeeping.
    std::vector<juce::Rectangle<float>> bounds;
    int maxSlot = -1;

    for (auto* block : blocks)
    {
        const int row = block->gridSlot / chainColumns;
        const int col = block->gridSlot % chainColumns;
        block->setBounds (col * (blockWidth + blockGap), row * (blockHeight + rowGap), blockWidth, blockHeight);
        bounds.emplace_back (block->getBounds().toFloat());
        maxSlot = juce::jmax (maxSlot, block->gridSlot);
    }

    const int totalRows = juce::jmax (1, maxSlot / chainColumns + 1);
    // At least numRows tall even when real content doesn't fill that
    // many rows yet -- otherwise the grid's dim placeholder rows
    // (ChainContainer::paint()) would be clipped off (JUCE clips paint()
    // to the component's own bounds). Still grows past that if there's
    // genuinely more content (the scrollbar fallback).
    const int drawnRows = juce::jmax (totalRows, numRows);
    chainContainer.setSize (viewportWidth, drawnRows * (blockHeight + rowGap) - rowGap);
    chainContainer.setRowMetrics (chainColumns, blockWidth, blockHeight, blockGap, rowGap);
    chainContainer.setBlockBounds (std::move (bounds));

    // IN always sits at row 0; OUT sits at whichever row is currently the
    // LAST occupied one (by gridSlot, which can be sparse -- an empty row
    // below some real content still counts as "not the last occupied one"
    // even if it has nothing in it), not a single slot centred across
    // every possible row -- rows in between get a "continues" connector
    // glyph instead, drawn in paint(). Reuses
    // leftGutterColumn/rightGutterColumn/chainRowTop, which only change on
    // an actual window resize (see resized()), so this stays correct when
    // called from addEffect()/removeEffect()/handleBlockDragEnded() too --
    // none of which trigger a full resized() pass. Per user request
    // 2026-09-10 (Quad Cortex-style multi-row IN/OUT routing).
    chainUsedRows = juce::jlimit (1, numRows, totalRows);
    chainContainer.setUsedRows (chainUsedRows);

    auto gutterRowSlot = [&] (juce::Rectangle<int> gutterColumn, int row)
    {
        return juce::Rectangle<int> (gutterColumn.getX(), chainRowTop + row * (blockHeight + rowGap),
                                      gutterColumn.getWidth(), blockHeight);
    };

    // Every row gets its own pair, not just row 0 and the last occupied one
    // -- each row is independently routable now, so each needs somewhere to
    // say so (per user request 2026-09-11, "+ no início e final de cada
    // linha").
    for (int row = 0; row < numRows; ++row)
    {
        rowInputBlocks[(size_t) row].setBounds (gutterRowSlot (leftGutterColumn, row).withSizeKeepingCentre (ioWidth, ioHeight));
        rowOutputBlocks[(size_t) row].setBounds (gutterRowSlot (rightGutterColumn, row).withSizeKeepingCentre (ioWidth, ioHeight));
    }

    // Where a row-to-row link crosses the grid: midway between the two rows'
    // own lines, in ChainContainer's coordinates (it draws the crossing; the
    // gutters either side are outside its bounds -- see paint()).
    std::vector<int> crossings;
    for (int row = 0; row < numRows; ++row)
    {
        const auto& routing = rowRouting[(size_t) row];
        if (routing.dest != RowRouting::Dest::row || routing.destRow < 0)
            continue;

        const int fromY = row * (blockHeight + rowGap) + blockHeight / 2;
        const int toY = routing.destRow * (blockHeight + rowGap) + blockHeight / 2;
        crossings.push_back ((fromY + toY) / 2);
    }
    chainContainer.setCrossings (std::move (crossings));

    repaint(); // the inter-row connector glyphs paint() draws depend on chainUsedRows
}

void MainComponent::handleBlockDragEnded (EffectBlockComponent& blockComp)
{
    if (blocks.indexOf (&blockComp) < 0)
    {
        layoutChain();
        return;
    }

    // Where it was dropped, translated back into a grid cell -- row from Y
    // now that dragging isn't locked to one row anymore, column from X,
    // same as layoutChain()'s own row/col math. Not clamped to existing
    // content -- you can drag a block onto any empty cell, same as adding
    // one there (see EffectBlockComponent::gridSlot's comment).
    const auto centre = blockComp.getBounds().getCentre();
    const int row = juce::jlimit (0, numRows - 1, centre.y / (blockHeight + rowGap));
    const int col = juce::jlimit (0, chainColumns - 1, centre.x / (blockWidth + blockGap));
    const int targetSlot = row * chainColumns + col;

    if (targetSlot != blockComp.gridSlot)
    {
        // Dropped onto an already-occupied cell -- swap slots with whatever
        // was there instead of silently refusing the drop.
        for (auto* other : blocks)
        {
            if (other != &blockComp && other->gridSlot == targetSlot)
            {
                other->gridSlot = blockComp.gridSlot;
                break;
            }
        }
        blockComp.gridSlot = targetSlot;

        // `blocks`/`chain` must stay sorted ascending by gridSlot -- that
        // sort order IS the signal processing order (see
        // rebuildSignalGraph()) -- so re-sort both, in lockstep, by the
        // same permutation rather than the old std::rotate (which only
        // made sense when "move to array index N" and "move to grid slot
        // N" were the same thing).
        std::vector<int> order ((size_t) blocks.size());
        std::iota (order.begin(), order.end(), 0);
        std::sort (order.begin(), order.end(),
                    [this] (int a, int b) { return blocks[a]->gridSlot < blocks[b]->gridSlot; });

        std::vector<std::unique_ptr<EffectProcessor>> newChain;
        newChain.reserve (chain.size());
        juce::Array<EffectBlockComponent*> newBlockOrder;
        for (int i : order)
        {
            newChain.push_back (std::move (chain[(size_t) i]));
            newBlockOrder.add (blocks[i]);
        }
        chain = std::move (newChain);

        blocks.clearQuick (false); // releases ownership without deleting -- newBlockOrder already holds every pointer
        for (auto* b : newBlockOrder)
            blocks.add (b);

        rebuildSignalGraph();
    }

    layoutChain(); // snaps every block, including the dragged one, back onto the clean grid
}

void MainComponent::showAddEffectMenu (int targetGridSlot)
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
        [this, idToKey, targetGridSlot] (int result)
        {
            if (result > 0 && result - 1 < (int) idToKey.size())
                addEffect (idToKey[(size_t) result - 1], targetGridSlot);
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
        // kept in lockstep everywhere (see AGENTS.md). gridSlot is its grid
        // cell, which can be sparse now (not necessarily == i) -- see
        // EffectBlockComponent::gridSlot's comment.
        blockXml->setAttribute ("gridSlot", blocks[i]->gridSlot);
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
        // Falls back to `blocks.size()` (this block's about-to-be array
        // index) for presets saved before gridSlot existed -- same
        // contiguous layout they always had.
        block->gridSlot = blockXml->getIntAttribute ("gridSlot", blocks.size());
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
        rowRouting[0].inputChannel = inCh;
        rowRouting[0].dest = RowRouting::Dest::device;
        rowRouting[0].destOutputPair = outPair;
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
    // Tiles are SQUARE (blockWidth == blockHeight) sized so exactly
    // chainColumns (8) fit the available width, per user request
    // 2026-09-10 ("ele tem q ser quadrados n retangulos"). Almost always
    // more vertical room than 4 square tiles need, though -- rather than
    // stretch them into rectangles again, that leftover becomes rowGap
    // between rows instead ("n tem problema se tiver espaço entre eles"),
    // so the 4-row BLOCK as a whole still spans the full height even
    // though the individual tiles stay a sensible size.
    const int chainContentWidth = area.getWidth() - 2 * ioWidth - 16; // the two 8px gaps after each gutter
    blockWidth = juce::jmax (60, (chainContentWidth - (chainColumns - 1) * blockGap) / chainColumns);
    blockHeight = blockWidth;

    const int rowsContentHeight = numRows * blockHeight;
    const int leftoverHeight = area.getHeight() - 12 - rowsContentHeight; // the -12 mirrors the old fudge-padding
    rowGap = leftoverHeight > (numRows - 1) * blockGap ? leftoverHeight / (numRows - 1) : blockGap;

    auto chainRow = area;
    chainRowTop = chainRow.getY();

    // Scrollbar first, off the far right of the window -- everything else
    // (gutters, viewport) lays out inside what's left.
    constexpr int scrollBarWidth = 10;
    chainScrollBar.setBounds (chainRow.removeFromRight (scrollBarWidth));
    chainRow.removeFromRight (6);

    leftGutterColumn = chainRow.removeFromLeft (ioWidth);
    chainRow.removeFromLeft (8);
    rightGutterColumn = chainRow.removeFromRight (ioWidth);
    chainRow.removeFromRight (8);
    chainViewport.setBounds (chainRow);
    layoutChain();

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
        parameterPanel.toFront (false); // overlays the chain -- must paint after it, see chainViewport's add order

        // The drawer covers the bottom panelHeight px of the SAME chain
        // area it overlays (see above) without shrinking chainViewport's
        // own bounds -- so without this, any row underneath it would just
        // be permanently hidden with no way to reach it. Padding
        // chainContainer's content height by exactly the covered amount
        // gives the viewport genuine scrollable overflow: scrolling up
        // shifts the covered row(s) into the visible, non-overlaid part of
        // the same viewport rectangle. Per user request 2026-09-11
        // ("quando abrir tipo o tab por cima... coloca em ver um
        // scrollbar").
        chainContainer.setSize (chainContainer.getWidth(), chainContainer.getHeight() + panelHeight);
    }

    // Closing the drawer shrinks chainContainer back down (the padding
    // above only applies while panelHeight > 0), but the viewport's
    // current scroll position doesn't necessarily snap back to 0 on its
    // own in the same call -- left stale for one frame, getViewPositionY()
    // in paint()'s connector code would read a leftover non-zero offset
    // against content that no longer has anywhere for it to point,
    // visibly detaching the connector from the rows it's supposed to
    // touch. Explicitly clamped here so it's never stale. Fixed 2026-09-11
    // ("lado direito ali ainda ta bugado").
    chainViewport.setViewPosition (chainViewport.getViewPositionX(),
                                    juce::jlimit (0, juce::jmax (0, chainContainer.getHeight() - chainViewport.getHeight()),
                                                  chainViewport.getViewPositionY()));
    syncChainScrollBar();
}

void MainComponent::syncChainScrollBar()
{
    const int contentHeight = chainContainer.getHeight();
    const int visibleHeight = juce::jmax (1, chainViewport.getMaximumVisibleHeight());

    chainScrollBar.setRangeLimits (0.0, (double) juce::jmax (contentHeight, visibleHeight), juce::dontSendNotification);
    chainScrollBar.setCurrentRange ((double) chainViewport.getViewPositionY(), (double) visibleHeight,
                                     juce::dontSendNotification); // never notify back -- this IS the response to a move
    chainScrollBar.setVisible (contentHeight > visibleHeight);
}

void MainComponent::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &chainScrollBar)
        chainViewport.setViewPosition (chainViewport.getViewPositionX(), (int) newRangeStart);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141414));

    // Inter-row connectors: every row before the last occupied one connects
    // to the start of the next with ONE continuous line -- down through the
    // right gutter, across through ChainContainer's own seam line (drawn in
    // its paint(), same Y, spanning its full width so the two halves meet
    // exactly at its edges), then down into the left gutter -- rather than
    // two disconnected glyphs. Per user correction 2026-09-10 ("as linhas
    // se conectarem do final com o início da próxima"). Built as ONE Path
    // per side with a rounded corner at the bend (PathStrokeType::curved
    // plus an explicit quadraticTo) rather than two separate drawLine()
    // calls -- those left a visible notch at the joint (flat line caps
    // don't overlap cleanly at a right angle) and read as sharp/square
    // rather than the rounded look asked for -- per user correction
    // 2026-09-11 ("falta fechar a linha, tamos com um gap ali... deixa
    // mais redondo"). Same white-at-30% line language as ChainContainer's
    // own lines throughout.
    g.setColour (juce::Colours::white.withAlpha (0.3f));

    // No inset needed any more: the scrollbar lives at the window's right
    // edge (chainScrollBar), not on top of the viewport's own, so nothing
    // overlaps the connector's rightmost reach.
    const float rightEdge = (float) chainViewport.getRight();
    const float leftEdge = (float) chainViewport.getX();
    constexpr float cornerRadius = 10.0f;

    // The rows themselves live inside chainViewport and move when it's
    // scrolled (see resized()'s comment on the drawer-open scroll
    // padding); these stub coordinates are absolute/unscrolled, so the
    // current scroll position has to be subtracted to stay lined up with
    // wherever the content actually is right now -- kept in sync via
    // visibleAreaChanged() triggering a repaint(). Fixed 2026-09-11 (was
    // the cause of a small but real gap between the stub and the row it's
    // supposed to touch whenever the chain had scrolled at all).
    const int scrollOffset = chainViewport.getViewPositionY();

    // One connector per row that feeds another row -- NOT one per pair of
    // consecutive rows. Rows are independent until explicitly linked (user
    // decision 2026-09-11: "some -- só conecta o que eu escolher"), so a
    // link can also skip rows or run upwards, and two adjacent rows with no
    // link between them correctly show nothing.
    for (int row = 0; row < numRows; ++row)
    {
        const auto& routing = rowRouting[(size_t) row];
        if (routing.dest != RowRouting::Dest::row || routing.destRow < 0 || routing.destRow >= numRows)
            continue;

        const auto rowCentreY = [this, scrollOffset] (int r)
        {
            return (float) chainRowTop + (float) r * (float) (blockHeight + rowGap)
                    + (float) blockHeight * 0.5f - (float) scrollOffset;
        };

        const float fromY = rowCentreY (row);
        const float toY = rowCentreY (routing.destRow);
        const float crossY = (fromY + toY) * 0.5f; // matches layoutChain()'s crossing Y
        const float bendDir = toY > fromY ? 1.0f : -1.0f; // links can run upwards too

        // Out of the source row's right end, around through the right
        // gutter, across (ChainContainer draws that middle span), then down
        // the left gutter and into the target row's left end.
        const float rightX = (float) rightGutterColumn.getCentreX();
        juce::Path rightSide;
        rightSide.startNewSubPath (rightEdge, fromY);
        rightSide.lineTo (rightX - cornerRadius, fromY);
        rightSide.quadraticTo (rightX, fromY, rightX, fromY + cornerRadius * bendDir);
        rightSide.lineTo (rightX, crossY - cornerRadius * bendDir);
        rightSide.quadraticTo (rightX, crossY, rightX - cornerRadius, crossY);
        rightSide.lineTo (rightEdge, crossY);
        g.strokePath (rightSide, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float leftX = (float) leftGutterColumn.getCentreX();
        juce::Path leftSide;
        leftSide.startNewSubPath (leftEdge, crossY);
        leftSide.lineTo (leftX + cornerRadius, crossY);
        leftSide.quadraticTo (leftX, crossY, leftX, crossY + cornerRadius * bendDir);
        leftSide.lineTo (leftX, toY - cornerRadius * bendDir);
        leftSide.quadraticTo (leftX, toY, leftX + cornerRadius, toY);
        leftSide.lineTo (leftEdge, toY);
        g.strokePath (leftSide, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

} // namespace pedaleira
