#include "RigLibrary.h"

bool RigLibrary::load(const juce::File& catalog, const juce::File& resourcesRoot, juce::String* error)
{
    const auto fail = [&](const juce::String& message)
    {
        if (error != nullptr) *error = message;
        return false;
    };
    if (!catalog.existsAsFile()) return fail("Rig catalog does not exist: " + catalog.getFullPathName());
    if (!resourcesRoot.isDirectory()) return fail("Rig resources root is not a directory");
    juce::var document;
    const auto parsed = juce::JSON::parse(catalog.loadFileAsString(), document);
    if (parsed.failed()) return fail("Invalid rig JSON: " + parsed.getErrorMessage());
    const auto* root = document.getDynamicObject();
    if (root == nullptr) return fail("Rig catalog root must be an object");
    const auto* rigs = root->getProperty("rigs").getArray();
    if (rigs == nullptr) return fail("Rig catalog requires a rigs array");
    juce::Array<RigEntry> loaded;
    juce::StringArray ids;
    int index = 0;
    for (const auto& value : *rigs)
    {
        const auto prefix = "Rig entry " + juce::String(index++) + ": ";
        const auto* object = value.getDynamicObject();
        if (object == nullptr) return fail(prefix + "expected object");
        RigEntry entry;
        const auto stringField = [&](const char* key, juce::String& target)
        {
            const auto& property = object->getProperty(key);
            if (!property.isString()) return false;
            target = property.toString();
            return true;
        };
        if (!stringField("id", entry.id) || !stringField("name", entry.name)
            || !stringField("description", entry.description) || !stringField("provenance", entry.provenance)
            || !stringField("routing", entry.routing) || !stringField("pedalFile", entry.pedalRelativePath)
            || !stringField("ampFile", entry.ampRelativePath) || !stringField("cabFile", entry.cabRelativePath))
            return fail(prefix + "id/name/description/provenance/routing/pedalFile/ampFile/cabFile must be strings");
        if (entry.id.trim().isEmpty() || entry.name.trim().isEmpty()) return fail(prefix + "id and name are required");
        if (ids.contains(entry.id)) return fail(prefix + "duplicate id: " + entry.id);
        ids.add(entry.id);
        if (entry.provenance.trim().isEmpty())
            return fail(prefix + "provenance must be nonempty");
        const auto* labels = object->getProperty("embeddedEffects").getArray();
        if (labels == nullptr) return fail(prefix + "embeddedEffects must be a string array");
        for (const auto& label : *labels)
        {
            if (!label.isString() || label.toString().trim().isEmpty())
                return fail(prefix + "embeddedEffects labels must be nonempty strings");
            entry.embeddedEffects.add(label.toString());
        }
        if (entry.ampRelativePath.isEmpty()) return fail(prefix + "ampFile is required");
        if (entry.routing == "full-rig")
        {
            if (entry.pedalRelativePath.isNotEmpty() || entry.cabRelativePath.isNotEmpty())
                return fail(prefix + "full-rig forbids external pedal/cab (prevents double processing)");
        }
        else if (entry.routing == "amp-cab")
        {
            if (entry.cabRelativePath.isEmpty() || entry.pedalRelativePath.isNotEmpty())
                return fail(prefix + "amp-cab requires cabFile and forbids pedalFile");
        }
        else if (entry.routing == "pedal-amp-cab")
        {
            if (entry.pedalRelativePath.isEmpty() || entry.cabRelativePath.isEmpty())
                return fail(prefix + "pedal-amp-cab requires pedalFile and cabFile");
        }
        else return fail(prefix + "unsupported routing: " + entry.routing);

        const auto resolve = [&](const juce::String& relative, juce::File& absolute)
        {
            if (relative.isEmpty()) return true;
            // Resource mappings must be relative and stay within this root.
            // Reject traversal/drive/UNC syntax rather than silently normalising.
            const auto normal = relative.replaceCharacter('\\', '/');
            if (juce::File::isAbsolutePath(relative) || normal.startsWithChar('/') || normal.containsChar(':')) return false;
            const auto parts = juce::StringArray::fromTokens(normal, "/", "");
            if (parts.contains("..") || parts.contains(".")) return false;
            const auto file = resourcesRoot.getChildFile(relative);
            if (!file.isAChildOf(resourcesRoot) || !file.existsAsFile()) return false;
            absolute = file;
            return true;
        };
        if (!resolve(entry.ampRelativePath, entry.ampFile)) return fail(prefix + "ampFile missing or not a resource-relative file: " + entry.ampRelativePath);
        if (!resolve(entry.pedalRelativePath, entry.pedalFile)) return fail(prefix + "pedalFile missing or not a resource-relative file: " + entry.pedalRelativePath);
        if (!resolve(entry.cabRelativePath, entry.cabFile)) return fail(prefix + "cabFile missing or not a resource-relative file: " + entry.cabRelativePath);
        entry.ampPath = entry.ampFile.getFullPathName();
        if (entry.pedalRelativePath.isNotEmpty()) entry.pedalPath = entry.pedalFile.getFullPathName();
        if (entry.cabRelativePath.isNotEmpty()) entry.cabPath = entry.cabFile.getFullPathName();
        loaded.add(std::move(entry));
    }
    records.swapWith(loaded);
    if (error != nullptr) error->clear();
    return true;
}

const RigEntry* RigLibrary::find(const juce::String& id) const noexcept
{
    for (const auto& entry : records) if (entry.id == id) return &entry;
    return nullptr;
}

SoundState RigLibrary::makeSound(const RigEntry& entry, const SoundState& current)
{
    auto sound = current;
    sound.rigId = entry.id;
    sound.namId.clear(); sound.irId.clear(); // UI assigns the new library IDs.
    sound.ampId.clear(); sound.configurationId.clear();
    sound.bassDb = sound.midDb = sound.trebleDb = 0.0f;
    sound.namInputTrimDb = 0.0f;
    sound.namBypassed = false;
    sound.irBypassed = entry.routing == "full-rig";
    sound.namPedalBypassed = entry.routing != "pedal-amp-cab";
    sound.namPedalPath = sound.namPedalBypassed ? juce::String{} : entry.pedalFile.getFullPathName();
    sound.namPedalInputDb = sound.namPedalOutputDb = 0.0f;
    sound.driveBypassed = sound.autoWahBypassed = sound.chorusBypassed = sound.delayBypassed = sound.reverbBypassed = true;
    // The chain and its plugins stay, the plugins switched off ("extra effects off").
    sound.layout = current.layout;
    for (auto* list : { &sound.layout.serial, &sound.layout.side })
        for (auto& node : *list)
        {
            node.plugin = node.plugin.createCopy(); // the rig's own copy, not shared with the live sound
            if (! node.isBuiltin())
                node.bypassed = true;
        }
    return sound;
}
