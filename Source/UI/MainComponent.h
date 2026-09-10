#pragma once

#include "ChainContainer.h"
#include "EffectBlockComponent.h"
#include "IOSelectorBlock.h"
#include "OverlayHost.h"
#include "ParameterPanel.h"
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
    Dev-facing chain builder: add any registered effect, in any order, as
    many as you like (Quad Cortex's Grid is the reference point, minus
    presets and split/merge -- those come later). This is the first thing
    in the project that lets you SEE the chain instead of just trusting
    that AudioEngine/SignalGraph work from test output.

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
    /** insertAtIndex < 0 (the default) means "append at the end". */
    void addEffect (const juce::String& registryName, int insertAtIndex = -1);
    void removeEffect (EffectProcessor* processor);
    void handleBlockDragEnded (EffectBlockComponent& block);
    void selectBlock (EffectProcessor* processor);
    void rebuildSignalGraph();
    void layoutChain();
    void showAddEffectMenu (int insertAtIndex = -1);
    void showSettingsPanel();
    void showPresetsPanel();
    std::unique_ptr<juce::XmlElement> buildPresetXml() const;
    void applyPresetXml (const juce::XmlElement& xml);
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

    // How many blocks fit per row at the chain's current width -- set by
    // layoutChain(), read back by handleBlockDragEnded() to translate a 2D
    // drop position into a flat chain index. Always >= 1.
    int chainColumns = 1;

    juce::Viewport chainViewport;
    ChainContainer chainContainer;
    AddBlockButton addButton;

    // Fixed at either end of the row (outside the scrolling viewport) --
    // device I/O routing, not part of the signal graph. See AudioEngine's
    // setInputChannel/setOutputRouting.
    IOSelectorBlock inputSelector { "IN" };
    IOSelectorBlock outputSelector { "OUT" };

    ParameterPanel parameterPanel;
    PresetManager presets { getPresetsDirectory() };
    juce::String currentPresetName; // empty = no preset loaded/saved since the last change

    // Shows the current preset's name once one is loaded/saved -- tapping
    // it opens PresetListDialog (see showPresetsPanel()).
    juce::TextButton presetBadge { juce::String::fromUTF8 ("\xe2\x80\x94") }; // em dash placeholder
    juce::Label titleLabel { {}, "Pedaleira NAM" };
    juce::Label cpuLabel;
    juce::TextButton settingsButton { juce::String::fromUTF8 ("\xe2\x8b\xae") }; // vertical ellipsis "..."

    // Every popup that used to be a separate OS window (TONE3000 login,
    // search, installed-model list) is a card drawn on top of this window
    // instead -- see OverlayHost and AGENT.md's UI/UX Design Philosophy.
    OverlayHost overlayHost;
};

} // namespace pedaleira
