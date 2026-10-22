#pragma once

#include <juce_core/juce_core.h>
#include "presets/PresetManager.h"

struct RigEntry
{
    juce::String id, name, description, provenance, routing;
    // Resolved resource paths; default File{} for unused stages.
    juce::File pedalFile, ampFile, cabFile;
    // Absolute-string aliases for the existing UI asset registration callbacks.
    juce::String pedalPath, ampPath, cabPath;
    juce::String pedalRelativePath, ampRelativePath, cabRelativePath;
    juce::StringArray embeddedEffects; // Labels only; never enables host effects.
};

class RigLibrary final
{
public:
    // Transactional: any invalid entry fails the load and preserves the old list.
    bool load(const juce::File& catalog, const juce::File& resourcesRoot, juce::String* error = nullptr);
    const juce::Array<RigEntry>& rigs() const noexcept { return records; }
    const juce::Array<RigEntry>& entries() const noexcept { return records; }
    const RigEntry* find(const juce::String& id) const noexcept;
    // Accepts a validated entry from this library. Does not load audio assets.
    // UI registers ampFile/cabFile into namId/irId after this call. Existing VST3
    // instances should be bypassed in place; their saved opaque state is retained.
    static SoundState makeSound(const RigEntry& entry, const SoundState& current);

private:
    juce::Array<RigEntry> records;
};
