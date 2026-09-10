#include "EffectRegistry.h"

#include "Effects/CompressorProcessor.h"
#include "Effects/GateProcessor.h"
#include "Effects/IRLoaderProcessor.h"
#include "Effects/NAMProcessor.h"
#include "Effects/OverdriveProcessor.h"

namespace pedaleira
{

void registerBuiltInEffects (EffectRegistry& registry)
{
    registry.registerType ("NoiseGate", [] { return std::make_unique<GateProcessor>(); });
    registry.registerType ("Compressor", [] { return std::make_unique<CompressorProcessor>(); });
    registry.registerType ("Overdrive", [] { return std::make_unique<OverdriveProcessor>(); });

    // Same wrapper class, three chain roles -- only the .nam file loaded
    // into each instance determines whether it sounds like an amp, an
    // amp+cab, or a pedal. Amp and Amp+Cab process identically (see
    // NAMProcessor.h) and only exist as separate blocks so TONE3000 search
    // can be scoped to one gear at a time, per user request.
    registry.registerType ("NeuralAmp", [] { return std::make_unique<NAMProcessor> ("Neural Amp"); });
    registry.registerType ("NeuralAmpCab", [] { return std::make_unique<NAMProcessor> ("Neural Amp + Cab"); });
    registry.registerType ("NeuralPedal", [] { return std::make_unique<NAMProcessor> ("Neural Pedal"); });

    // Same story, one class, two roles -- IRLoaderProcessor convolves with
    // a WAV impulse response either way; "Cab" vs "Reverb" is just which
    // TONE3000 gear (cab vs space) the loaded IR came from. See
    // IRLoaderProcessor.h -- this is NOT the same technology as NAM above.
    registry.registerType ("Cab", [] { return std::make_unique<IRLoaderProcessor> ("Cab"); });
    registry.registerType ("Reverb", [] { return std::make_unique<IRLoaderProcessor> ("Reverb"); });
}

void EffectRegistry::registerType (const juce::String& key, Creator creator)
{
    // Probing with a real (immediately discarded) instance instead of
    // asking the call site to also type the display name by hand: that
    // second string would have no way to stay in sync if a processor's
    // constructor default ever changes, and preset save/load's reverse
    // lookup (keyForDisplayName) needs this to always exactly match what
    // getName() really returns at runtime.
    if (auto probe = creator())
    {
        const juce::String displayName (probe->getName());
        displayNamesByKey[key] = displayName;
        keysByDisplayName[displayName] = key;
    }

    creators[key] = std::move (creator);
}

std::unique_ptr<EffectProcessor> EffectRegistry::create (const juce::String& key) const
{
    const auto it = creators.find (key);
    return it != creators.end() ? it->second() : nullptr;
}

juce::String EffectRegistry::displayNameForKey (const juce::String& key) const
{
    const auto it = displayNamesByKey.find (key);
    return it != displayNamesByKey.end() ? it->second : key;
}

juce::String EffectRegistry::keyForDisplayName (const juce::String& displayName) const
{
    const auto it = keysByDisplayName.find (displayName);
    return it != keysByDisplayName.end() ? it->second : juce::String();
}

juce::StringArray EffectRegistry::getRegisteredNames() const
{
    juce::StringArray names;
    for (const auto& [name, creator] : creators)
        names.add (name);
    return names;
}

} // namespace pedaleira
