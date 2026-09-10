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

    // Same wrapper class, two chain roles -- only the .nam file loaded into
    // each instance determines whether it sounds like an amp or a drive
    // pedal. See NAMProcessor.h.
    registry.registerType ("NAMAmp", [] { return std::make_unique<NAMProcessor> ("NAM Amp"); });
    registry.registerType ("NeuralDrive", [] { return std::make_unique<NAMProcessor> ("Neural Drive"); });

    // Same story, one class, two roles -- IRLoaderProcessor convolves with
    // a WAV impulse response either way; "Cab" vs "Reverb" is just which
    // TONE3000 gear (cab vs space) the loaded IR came from. See
    // IRLoaderProcessor.h -- this is NOT the same technology as NAM above.
    registry.registerType ("Cab", [] { return std::make_unique<IRLoaderProcessor> ("Cab"); });
    registry.registerType ("Reverb", [] { return std::make_unique<IRLoaderProcessor> ("Reverb"); });
}

void EffectRegistry::registerType (const juce::String& name, Creator creator)
{
    creators[name] = std::move (creator);
}

std::unique_ptr<EffectProcessor> EffectRegistry::create (const juce::String& name) const
{
    const auto it = creators.find (name);
    return it != creators.end() ? it->second() : nullptr;
}

juce::StringArray EffectRegistry::getRegisteredNames() const
{
    juce::StringArray names;
    for (const auto& [name, creator] : creators)
        names.add (name);
    return names;
}

} // namespace pedaleira
