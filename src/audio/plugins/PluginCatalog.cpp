#include "PluginCatalog.h"

namespace
{
juce::String utf8(const char* text) { return juce::String(juce::CharPointer_UTF8(text)); }

bool isInside(const juce::String& file, const juce::String& folder)
{
    return juce::File(file).isAChildOf(juce::File(folder));
}
} // namespace

juce::Array<juce::File> PluginCatalog::standardFolders()
{
    const auto appDirectory = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    return { appDirectory, appDirectory.getChildFile("plugins"),
             juce::File("C:/Program Files/Common Files/VST3"),
             juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("VST3"),
             juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".vst3") };
}

juce::StringArray PluginCatalog::filesToScan() const
{
    juce::FileSearchPath paths;
    for (const auto& directory : standardFolders())
        if (directory.isDirectory())
            paths.add(directory);
    for (const auto& folder : folders)
        if (juce::File(folder).isDirectory())
            paths.add(juce::File(folder));
    juce::StringArray result;
   #if JUCE_PLUGINHOST_VST3
    result = juce::VST3PluginFormat().searchPathsForPlugins(paths, true, false);
   #endif
    for (const auto& file : files)
        if (juce::File(file).exists())
            result.addIfNotAlreadyThere(file);
    return result;
}

juce::File PluginCatalog::bundleOf(const juce::File& file)
{
    for (auto parent = file.getParentDirectory(); parent != parent.getParentDirectory(); parent = parent.getParentDirectory())
        if (parent.isDirectory() && parent.hasFileExtension("vst3"))
            return parent;
    return file;
}

PluginCatalog::ScanResult PluginCatalog::scan(const juce::StringArray& filesToRead)
{
    ScanResult result;
    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName();
    for (const auto& file : filesToRead)
    {
        const auto report = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("ton-plugin-scan", ".xml");
        juce::ChildProcess child;
        juce::String reason;
        if (! child.start(juce::StringArray { executable, "--scan-vst3-all", file, report.getFullPathName() }))
            reason = utf8("Der Scanner ließ sich nicht starten.");
        else if (! child.waitForProcessToFinish(30000))
            reason = utf8("Keine Antwort nach 30 Sekunden – das Plugin hängt beim Laden.");
        else if (const auto xml = juce::XmlDocument::parse(report); xml == nullptr)
            reason = utf8("Das Plugin ist beim Laden abgestürzt.");
        else
        {
            const auto before = result.found.size();
            for (auto* item = xml->getFirstChildElement(); item != nullptr; item = item->getNextElement())
                if (juce::PluginDescription description; description.loadFromXml(*item))
                    result.found.add(description);
            if (result.found.size() == before)
                reason = utf8("Kein ladbares VST3-Plugin (falsche Architektur, fehlende Dateien oder kein Plugin).");
        }
        child.kill();
        report.deleteFile();
        if (reason.isNotEmpty())
            result.failures.push_back({ file, reason });
    }
    return result;
}

void PluginCatalog::merge(const juce::StringArray& scannedFiles, const ScanResult& result)
{
    const auto scanned = [&scannedFiles](const juce::String& file) { return scannedFiles.contains(file); };
    plugins.removeIf([&scanned](const juce::PluginDescription& description) { return scanned(description.fileOrIdentifier); });
    failures.erase(std::remove_if(failures.begin(), failures.end(), [&scanned](const Failure& failure) { return scanned(failure.file); }),
                   failures.end());
    for (const auto& description : result.found)
        plugins.add(description);
    failures.insert(failures.end(), result.failures.begin(), result.failures.end());
}

void PluginCatalog::removeFile(const juce::String& file)
{
    files.removeString(file);
    merge({ file }, {});
}

void PluginCatalog::removeFolder(const juce::String& folder)
{
    folders.removeString(folder);
    plugins.removeIf([&](const juce::PluginDescription& d) { return isInside(d.fileOrIdentifier, folder) && ! files.contains(d.fileOrIdentifier); });
    failures.erase(std::remove_if(failures.begin(), failures.end(), [&](const Failure& f) { return isInside(f.file, folder) && ! files.contains(f.file); }),
                   failures.end());
}

void PluginCatalog::setHidden(const juce::PluginDescription& description, bool hide)
{
    const auto id = description.createIdentifierString();
    if (hide)
        hidden.addIfNotAlreadyThere(id);
    else
        hidden.removeString(id);
}

bool PluginCatalog::isHidden(const juce::PluginDescription& description) const
{
    return hidden.contains(description.createIdentifierString());
}

juce::Array<juce::PluginDescription> PluginCatalog::visible() const
{
    juce::Array<juce::PluginDescription> result;
    for (const auto& description : plugins)
        if (! isHidden(description))
            result.add(description);
    return result;
}

juce::Array<juce::PluginDescription> PluginCatalog::instruments() const
{
    auto result = visible();
    result.removeIf([](const juce::PluginDescription& d) { return ! d.isInstrument; });
    return result;
}

juce::ValueTree PluginCatalog::toValueTree() const
{
    juce::ValueTree tree("pluginCatalog");
    const auto addStrings = [&tree](const char* type, const juce::StringArray& values)
    {
        juce::ValueTree list(type);
        for (const auto& value : values)
            list.appendChild(juce::ValueTree("item").setProperty("value", value, nullptr), nullptr);
        tree.appendChild(list, nullptr);
    };
    addStrings("folders", folders);
    addStrings("files", files);
    addStrings("hidden", hidden);
    juce::ValueTree list("plugins");
    for (const auto& description : plugins)
        if (const auto xml = description.createXml())
            list.appendChild(juce::ValueTree::fromXml(*xml), nullptr);
    tree.appendChild(list, nullptr);
    juce::ValueTree failed("failures");
    for (const auto& failure : failures)
        failed.appendChild(juce::ValueTree("failure").setProperty("file", failure.file, nullptr).setProperty("reason", failure.reason, nullptr), nullptr);
    tree.appendChild(failed, nullptr);
    return tree;
}

PluginCatalog PluginCatalog::fromUiState(const juce::ValueTree& uiState)
{
    PluginCatalog catalog;
    const auto readPlugins = [&catalog](const juce::ValueTree& list)
    {
        for (const auto& item : list)
            if (const auto xml = item.createXml())
                if (juce::PluginDescription description; description.loadFromXml(*xml))
                    catalog.plugins.add(description);
    };
    const auto tree = uiState.getChildWithName("pluginCatalog");
    if (! tree.isValid())
    {
        readPlugins(uiState.getChildWithName("vst3Catalog"));
        return catalog;
    }
    const auto readStrings = [&tree](const char* type, juce::StringArray& into)
    {
        for (const auto& item : tree.getChildWithName(type))
            if (const auto value = item.getProperty("value").toString(); value.isNotEmpty())
                into.addIfNotAlreadyThere(value);
    };
    readStrings("folders", catalog.folders);
    readStrings("files", catalog.files);
    readStrings("hidden", catalog.hidden);
    readPlugins(tree.getChildWithName("plugins"));
    for (const auto& item : tree.getChildWithName("failures"))
        catalog.failures.push_back({ item.getProperty("file").toString(), item.getProperty("reason").toString() });
    return catalog;
}
