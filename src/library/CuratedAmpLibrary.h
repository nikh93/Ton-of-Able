#pragma once

#include <juce_core/juce_core.h>

struct CuratedAmpModel
{
    juce::String ampId;
    juce::String ampName;
    juce::String manufacturer;
    juce::String configurationId;
    juce::String configurationName;
    juce::String captureType;
    juce::String category;
    juce::String gainLevel;
    juce::String namFile;
    juce::String recommendedIrFile;
    juce::String description;
    juce::String metadataJson;
    juce::StringArray tags;
};

class CuratedAmpLibrary final
{
public:
    bool load(const juce::File& catalogFile, juce::String* error = nullptr, bool append = false);
    const juce::Array<CuratedAmpModel>& models() const noexcept { return entries; }
    const CuratedAmpModel* find(const juce::String& ampId, const juce::String& configurationId) const noexcept;
    const CuratedAmpModel* findByFile(const juce::String& fileName) const noexcept;

private:
    juce::Array<CuratedAmpModel> entries;
};
