#pragma once

#include "Effects/EffectProcessor.h"

#include <juce_core/juce_core.h>

#include <functional>
#include <map>
#include <memory>

namespace pedaleira
{

/**
    Factory mapping name -> EffectProcessor. Empty in Phase 0; concrete
    pedals (Gate, Compressor, Overdrive, ...) register themselves here in
    Phase 1. SignalGraph and AudioEngine never know about concrete effect
    types -- they only ever go through this factory.
*/
class EffectRegistry
{
public:
    using Creator = std::function<std::unique_ptr<EffectProcessor>()>;

    void registerType (const juce::String& name, Creator creator);
    std::unique_ptr<EffectProcessor> create (const juce::String& name) const;
    juce::StringArray getRegisteredNames() const;

private:
    std::map<juce::String, Creator> creators;
};

/** Registers every built-in effect processor. Concrete types stay decoupled
    from SignalGraph/AudioEngine -- this is the one place that knows them all. */
void registerBuiltInEffects (EffectRegistry& registry);

} // namespace pedaleira
