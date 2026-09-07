#include "EffectRegistry.h"

namespace pedaleira
{

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
