#pragma once

#include <juce_core/juce_core.h>

#include <memory>

namespace pedaleira
{

/**
    Plain file I/O for presets -- one XML file per preset, named
    "<preset name>.xml" under a presets directory. Deliberately knows
    nothing about EffectProcessor/EffectRegistry/the signal chain itself;
    MainComponent builds and interprets the XmlElement (see
    MainComponent::buildPresetXml()/applyPresetXml()) -- same separation
    Tone3000Manager keeps from the UI that drives it.

    Control-thread only, like every other file-I/O helper in this project.
*/
class PresetManager
{
public:
    explicit PresetManager (juce::File directoryToUse);

    /** Sorted, derived from *.xml file stems in the presets directory.
        Empty (not an error) if the directory doesn't exist yet -- no
        presets saved is a normal starting state. */
    juce::StringArray listPresetNames() const;

    /** nullptr if the file doesn't exist or fails to parse. */
    std::unique_ptr<juce::XmlElement> loadPreset (const juce::String& name) const;

    /** Creates the presets directory if needed. */
    bool savePreset (const juce::String& name, const juce::XmlElement& xml) const;

    bool deletePreset (const juce::String& name) const;

private:
    juce::File fileFor (const juce::String& name) const;

    juce::File directory;
};

} // namespace pedaleira
