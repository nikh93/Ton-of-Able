#pragma once

#include <juce_core/juce_core.h>

#include "LibraryManager.h"

/** The user's model folder: imported NAM models and IRs are copied here, so a
    library entry never breaks when the original download is moved or deleted.

        <root>/NAM   imported .nam models
        <root>/IR    imported impulse responses (.wav)

    Files placed into these folders by hand are picked up by sync(). */
class ModelFolder final
{
public:
    struct ImportResult
    {
        juce::Array<juce::Uuid> imported;   // library ids, in selection order
        int copied = 0;                     // new files written to the folder
        int alreadyPresent = 0;             // identical file was already there
        juce::StringArray failed;           // "file: reason"
    };

    explicit ModelFolder(juce::File rootFolder);
    [[nodiscard]] static juce::File defaultLocation();

    [[nodiscard]] const juce::File& root() const noexcept { return rootFolder; }
    [[nodiscard]] juce::File folderFor(const LibraryManager&) const;

    /** Files with the library's extension in the selection; folders are searched recursively. */
    [[nodiscard]] static juce::Array<juce::File> collect(const juce::Array<juce::File>& selection, const juce::String& extension);

    /** Copies every selected file (or the files inside selected folders) into
        the model folder and links the copy in the library. Identical files are
        not copied twice; different files with the same name get a numbered name. */
    ImportResult importFiles(LibraryManager&, const juce::Array<juce::File>& selection, const juce::StringArray& tags = {}) const;

    /** Adds files that are in the model folder but not yet in the library. Returns how many. */
    int sync(LibraryManager&) const;

    /** Copies linked files that live outside the model folder into it and
        relinks them (bundled defaults are left alone). Returns how many. */
    int adoptExternal(LibraryManager&, const juce::File& keepInPlace) const;

private:
    [[nodiscard]] juce::File copyIn(const juce::File& source, const juce::File& folder, bool& copied, juce::String& error) const;

    juce::File rootFolder;
};
