#include "ModelFolder.h"

ModelFolder::ModelFolder(juce::File folder) : rootFolder(std::move(folder)) {}

juce::File ModelFolder::defaultLocation()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Ton of Able").getChildFile("Modelle");
}

juce::File ModelFolder::folderFor(const LibraryManager& library) const
{
    return rootFolder.getChildFile(library.itemType() == "nam" ? "NAM" : "IR");
}

juce::Array<juce::File> ModelFolder::collect(const juce::Array<juce::File>& selection, const juce::String& extension)
{
    juce::Array<juce::File> files;
    const auto pattern = "*" + extension;
    for (const auto& item : selection)
    {
        if (item.isDirectory())
        {
            auto found = item.findChildFiles(juce::File::findFiles, true, pattern);
            found.sort();
            for (const auto& file : found)
                files.addIfNotAlreadyThere(file);
        }
        else if (item.existsAsFile() && item.hasFileExtension(extension))
            files.addIfNotAlreadyThere(item);
    }
    return files;
}

juce::File ModelFolder::copyIn(const juce::File& source, const juce::File& folder, bool& copied, juce::String& error) const
{
    copied = false;
    if (source.isAChildOf(folder))
        return source; // already in the model folder
    if (! folder.createDirectory())
    {
        error = "Modellordner kann nicht angelegt werden: " + folder.getFullPathName();
        return {};
    }
    const auto sameName = folder.getChildFile(source.getFileName());
    if (sameName.existsAsFile() && sameName.hasIdenticalContentTo(source))
        return sameName;
    // A different file with the same name: keep both ("Name (2).nam").
    const auto target = sameName.existsAsFile()
        ? folder.getNonexistentChildFile(source.getFileNameWithoutExtension(), source.getFileExtension(), true)
        : sameName;
    if (! source.copyFileTo(target))
    {
        error = "Kopieren fehlgeschlagen: " + target.getFullPathName();
        return {};
    }
    copied = true;
    return target;
}

ModelFolder::ImportResult ModelFolder::importFiles(LibraryManager& library, const juce::Array<juce::File>& selection, const juce::StringArray& tags) const
{
    ImportResult result;
    const auto extension = library.itemType() == "nam" ? juce::String(".nam") : juce::String(".wav");
    const auto folder = folderFor(library);
    for (const auto& file : collect(selection, extension))
    {
        bool copied = false;
        juce::String error;
        const auto stored = copyIn(file, folder, copied, error);
        if (stored == juce::File())
        {
            result.failed.add(file.getFileName() + ": " + error);
            continue;
        }
        const auto id = library.importLink(stored, tags, "{}", &error);
        if (! id)
        {
            result.failed.add(file.getFileName() + ": " + error);
            continue;
        }
        result.imported.add(*id);
        if (copied)
            ++result.copied;
        else
            ++result.alreadyPresent;
    }
    return result;
}

int ModelFolder::sync(LibraryManager& library) const
{
    const auto extension = library.itemType() == "nam" ? juce::String(".nam") : juce::String(".wav");
    const auto folder = folderFor(library);
    if (! folder.isDirectory())
        return 0;
    int added = 0;
    for (const auto& file : collect({ folder }, extension))
    {
        bool known = false;
        for (const auto& item : library.items())
            known |= item.linkedFile == file;
        if (! known && library.importLink(file))
            ++added;
    }
    return added;
}

int ModelFolder::adoptExternal(LibraryManager& library, const juce::File& keepInPlace) const
{
    const auto folder = folderFor(library);
    int adopted = 0;
    for (const auto& item : juce::Array<LibraryItem>(library.items()))
    {
        if (item.isMissing() || item.linkedFile.isAChildOf(folder)
            || (keepInPlace != juce::File() && item.linkedFile.isAChildOf(keepInPlace)))
            continue;
        bool copied = false;
        juce::String error;
        if (const auto stored = copyIn(item.linkedFile, folder, copied, error); stored != juce::File() && library.relink(item.id, stored))
            ++adopted;
    }
    return adopted;
}
