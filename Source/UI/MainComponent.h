#pragma once

#include "EffectBlockComponent.h"
#include "RoutingCanvas.h"
#include "RoutingGraph.h"
#include "FooterBar.h"
#include "IOSelectorBlock.h"
#include "OverlayHost.h"
#include "ParameterPanel.h"
#include "PresetBadge.h"
#include "../EffectRegistry.h"
#include "../Engine/AudioEngine.h"
#include "../Presets/PresetManager.h"
#include "../Tone3000/Tone3000Manager.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace pedaleira
{

/**
    Dev-facing patchbay: add any registered effect onto any of four lanes
    and cable them together however you like -- sequential, parallel,
    split, merged. The RoutingGraph is the single source of truth for what
    feeds what; lanes and columns only decide where a block is drawn.

    Replaced a strictly ordered grid (one array, position == processing
    order) 2026-09-11, per user request: "a ordem deve surgir naturalmente
    das conexões... eu conecto o áudio como se estivesse plugando cabos",
    not "eu escolho a posição de cada efeito em uma lista".

    Ownership: MainComponent is the one persistent owner of every
    EffectProcessor for as long as it's in the chain (`chain`). SignalGraph
    itself only ever holds non-owning pointers into that -- see
    SignalGraph.h for why. Removing a block moves it into `graveyard`
    instead of destroying it immediately: the just-superseded SignalGraph
    might still be mid-use on the audio thread for a moment, and that old
    graph still points at this processor.
*/
class MainComponent : public juce::Component,
                       private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    /** Creates the processor, its block, and its graph node at lane/column.
        Auto-cables it inline with whatever already sits to its left on the
        same lane (and to that lane's tail otherwise) so a plain "add three
        effects" still just works without patching by hand -- but nothing
        stops you re-patching it afterwards. */
    void addEffect (const juce::String& registryName, int lane = 0, int column = -1);
    void removeEffect (EffectProcessor* processor);
    void handleBlockDragEnded (EffectBlockComponent& block);
    void selectBlock (EffectProcessor* processor);
    /** Feeds SignalGraph from RoutingGraph::processingOrder() -- evaluation
        order falls out of the cables, not out of any list. Parallel
        branches are still flattened into one serial order here: real
        split/merge DSP is the next stage (the engine work deliberately
        deferred when this layer was built). */
    void rebuildSignalGraph();
    void layoutNodes();
    void showAddEffectMenu (int lane, int column);
    /** Every block's processor, by node -- the canvas and the graph deal in
        NodeIds, the parameter drawer and SignalGraph deal in processors. */
    EffectProcessor* processorForNode (NodeId id) const;
    EffectBlockComponent* blockForNode (NodeId id) const;
    /** The rightmost effect node on `lane`, or invalidNode if it's empty. */
    NodeId lastNodeOnLane (int lane, int beforeColumn) const;
    void showSettingsPanel();
    void showPresetsPanel();
    std::unique_ptr<juce::XmlElement> buildPresetXml() const;
    void applyPresetXml (const juce::XmlElement& xml);
    /** Saves under `name`, reusing its existing number if `name` is already a
        saved preset (a resave, not a new slot) or assigning the next free
        one otherwise. Updates currentPresetName/Number and the top bar either way. */
    void savePresetAs (const juce::String& name);
    void updatePresetDisplay();
    static juce::File getModelsDirectory();
    static juce::File getPresetsDirectory();

    void timerCallback() override;

    EffectRegistry registry;
    AudioEngine audioEngine;
    Tone3000Manager tone3000;

    std::vector<std::unique_ptr<EffectProcessor>> chain;
    juce::OwnedArray<EffectBlockComponent> blocks;
    EffectProcessor* selectedProcessor = nullptr;

    struct RetiredProcessor
    {
        std::unique_ptr<EffectProcessor> processor;
        juce::uint32 retiredAtMs;
    };
    std::vector<RetiredProcessor> graveyard;

    // The real topology. Every block on screen is a node in here; the
    // cables between them are what SignalGraph's order is derived from.
    RoutingGraph graph;
    RoutingCanvas routingCanvas { graph };

    // The hardware endpoints, as graph nodes (stereo: L/R ports each), so
    // "what reaches the output?" is answerable by the same graph walk as
    // everything else instead of being a special case. Created once in the
    // constructor and never removed.
    NodeId inputNodeId = invalidNode;
    NodeId outputNodeId = invalidNode;

    // Device I/O routing (which physical channel feeds the input node,
    // which pair the output node lands on) -- not part of the signal
    // graph itself. See AudioEngine's setInputChannel/setOutputRouting.
    IOSelectorBlock inputSelector { "IN" };
    IOSelectorBlock outputSelector { "OUT" };

    // Tuner/BPM-tap-tempo/IN-OUT-meters bar, always pinned at the bottom --
    // see FooterBar.h. Visual placeholder only until Phase 5.
    FooterBar footerBar;

    ParameterPanel parameterPanel;
    PresetManager presets { getPresetsDirectory() };
    juce::String currentPresetName;   // empty = no preset loaded/saved since the last change
    int currentPresetNumber = 0;      // 0 = none yet; a real preset's number is always >= 1

    // presetBadge shows the preset NUMBER, big (see PresetBadge.h -- this
    // is a stage instrument, has to be readable from a few feet away, not
    // just a toolbar). titleLabel shows its NAME, same size class. Tapping
    // the badge opens PresetListDialog (see showPresetsPanel()). Once
    // scenes exist (a preset's own internal variations, e.g. "1A"/"1B"),
    // the badge's text becomes number+letter in that same slot -- see
    // AGENT.md's UI/UX Design Philosophy -- this is deliberately not built
    // yet, just planned for.
    PresetBadge presetBadge;
    // Saves the currently loaded preset in place, no dialog -- only visible
    // once one is actually loaded (see updatePresetDisplay()).
    juce::TextButton quickSaveButton { "Save" };
    juce::Label titleLabel { {}, "No preset loaded" };
    juce::Label cpuLabel;
    juce::TextButton settingsButton { juce::String::fromUTF8 ("\xe2\x8b\xae") }; // vertical ellipsis "..."

    // Every popup that used to be a separate OS window (TONE3000 login,
    // search, installed-model list) is a card drawn on top of this window
    // instead -- see OverlayHost and AGENT.md's UI/UX Design Philosophy.
    OverlayHost overlayHost;
};

} // namespace pedaleira
