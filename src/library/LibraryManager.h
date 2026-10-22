#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

/** Persistent, UI-agnostic index for files which are deliberately never copied.
    Callers own presentation and file selection; this class only manages links and
    serialisable metadata. */
struct LibraryItem
{
    juce::Uuid id;
    juce::File linkedFile;
    juce::String displayName;
    juce::StringArray tags;
    juce::String metadataJson { "{}" };

    bool isMissing() const noexcept;
    juce::ValueTree toValueTree() const;
    static std::optional<LibraryItem> fromValueTree(const juce::ValueTree&, juce::String* error = nullptr);
};

class LibraryManager
{
public:
    explicit LibraryManager(juce::String itemType, juce::StringArray permittedExtensions);

    const juce::String& itemType() const noexcept { return type; }
    const juce::Array<LibraryItem>& items() const noexcept { return entries; }
    const LibraryItem* find(const juce::Uuid&) const noexcept;

    /** Adds a link. The target must exist and have a permitted extension. */
    std::optional<juce::Uuid> importLink(const juce::File&, juce::StringArray tags = {},
                                         juce::String metadataJson = "{}", juce::String* error = nullptr);
    bool rename(const juce::Uuid&, const juce::String& displayName);
    bool setTags(const juce::Uuid&, juce::StringArray tags);
    bool setMetadataJson(const juce::Uuid&, const juce::String& json, juce::String* error = nullptr);
    bool relink(const juce::Uuid&, const juce::File&, juce::String* error = nullptr);
    bool erase(const juce::Uuid&); // Removes only the library entry, never the linked file.

    juce::ValueTree toValueTree() const;
    bool loadValueTree(const juce::ValueTree&, juce::String* error = nullptr);
    juce::var toJson() const;
    bool loadJson(const juce::var&, juce::String* error = nullptr);

private:
    bool accepts(const juce::File&, juce::String* error) const;
    static bool isJson(const juce::String&);
    static juce::StringArray normaliseTags(juce::StringArray);

    juce::String type;
    juce::StringArray extensions;
    juce::Array<LibraryItem> entries;
};
