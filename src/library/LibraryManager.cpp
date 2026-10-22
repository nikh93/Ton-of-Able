#include "LibraryManager.h"

namespace
{
constexpr auto rootType = "linkedLibrary";
constexpr auto itemNode = "item";
}

bool LibraryItem::isMissing() const noexcept { return ! linkedFile.existsAsFile(); }

juce::ValueTree LibraryItem::toValueTree() const
{
    juce::ValueTree node { itemNode };
    node.setProperty("id", id.toString(), nullptr);
    node.setProperty("path", linkedFile.getFullPathName(), nullptr);
    node.setProperty("name", displayName, nullptr);
    node.setProperty("metadata", metadataJson, nullptr);
    node.setProperty("tags", tags.joinIntoString("\n"), nullptr);
    return node;
}

std::optional<LibraryItem> LibraryItem::fromValueTree(const juce::ValueTree& node, juce::String* error)
{
    if (! node.hasType(itemNode)) { if (error != nullptr) *error = "Not a library item."; return {}; }
    const auto idString = node.getProperty("id").toString();
    const auto path = node.getProperty("path").toString();
    if (idString.isEmpty() || path.isEmpty()) { if (error != nullptr) *error = "Library item has no id or path."; return {}; }
    LibraryItem result { juce::Uuid(idString), juce::File(path), node.getProperty("name").toString(), {}, node.getProperty("metadata", "{}").toString() };
    result.tags.addLines(node.getProperty("tags").toString());
    if (result.displayName.isEmpty()) result.displayName = result.linkedFile.getFileNameWithoutExtension();
    return result;
}

LibraryManager::LibraryManager(juce::String itemType, juce::StringArray permittedExtensions)
    : type(std::move(itemType)), extensions(std::move(permittedExtensions))
{
    extensions.trim(); for (auto& extension : extensions) extension = extension.toLowerCase(); extensions.removeEmptyStrings();
}

const LibraryItem* LibraryManager::find(const juce::Uuid& id) const noexcept
{
    for (const auto& item : entries) if (item.id == id) return &item;
    return nullptr;
}

bool LibraryManager::accepts(const juce::File& file, juce::String* error) const
{
    if (! file.existsAsFile()) { if (error != nullptr) *error = "The linked file does not exist."; return false; }
    if (! extensions.contains(file.getFileExtension().toLowerCase())) { if (error != nullptr) *error = "Unsupported file extension."; return false; }
    return true;
}

bool LibraryManager::isJson(const juce::String& value) { return juce::JSON::parse(value).isVoid() == false; }
juce::StringArray LibraryManager::normaliseTags(juce::StringArray tags)
{
    tags.trim(); tags.removeEmptyStrings(); tags.removeDuplicates(false); return tags;
}

std::optional<juce::Uuid> LibraryManager::importLink(const juce::File& file, juce::StringArray tags, juce::String metadata, juce::String* error)
{
    if (! accepts(file, error) || ! isJson(metadata)) { if (error != nullptr && ! isJson(metadata)) *error = "Metadata must be valid JSON."; return {}; }
    for (const auto& entry : entries) if (entry.linkedFile == file) return entry.id;
    LibraryItem item { juce::Uuid(), file, file.getFileNameWithoutExtension(), normaliseTags(std::move(tags)), std::move(metadata) };
    entries.add(item); return item.id;
}

bool LibraryManager::rename(const juce::Uuid& id, const juce::String& name) { for (auto& x : entries) if (x.id == id) { x.displayName = name.trim(); return ! x.displayName.isEmpty(); } return false; }
bool LibraryManager::setTags(const juce::Uuid& id, juce::StringArray tags) { for (auto& x : entries) if (x.id == id) { x.tags = normaliseTags(std::move(tags)); return true; } return false; }
bool LibraryManager::setMetadataJson(const juce::Uuid& id, const juce::String& json, juce::String* error)
{ if (! isJson(json)) { if (error != nullptr) *error = "Metadata must be valid JSON."; return false; } for (auto& x : entries) if (x.id == id) { x.metadataJson = json; return true; } return false; }
bool LibraryManager::relink(const juce::Uuid& id, const juce::File& file, juce::String* error)
{ if (! accepts(file, error)) return false; for (auto& x : entries) if (x.id == id) { x.linkedFile = file; return true; } return false; }
bool LibraryManager::erase(const juce::Uuid& id) { for (int i = 0; i < entries.size(); ++i) if (entries.getReference(i).id == id) { entries.remove(i); return true; } return false; }

juce::ValueTree LibraryManager::toValueTree() const
{
    juce::ValueTree root { rootType }; root.setProperty("type", type, nullptr); root.setProperty("version", 1, nullptr);
    for (const auto& item : entries) root.addChild(item.toValueTree(), -1, nullptr); return root;
}
bool LibraryManager::loadValueTree(const juce::ValueTree& root, juce::String* error)
{
    if (! root.hasType(rootType) || root.getProperty("type").toString() != type) { if (error != nullptr) *error = "Wrong library document type."; return false; }
    juce::Array<LibraryItem> parsed;
    for (int i = 0; i < root.getNumChildren(); ++i) { auto item = LibraryItem::fromValueTree(root.getChild(i), error); if (! item) return false; parsed.add(*item); }
    entries = std::move(parsed); return true;
}
juce::var LibraryManager::toJson() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty("type", type); root->setProperty("version", 1);
    juce::Array<juce::var> items;
    for (const auto& item : entries)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("id", item.id.toString()); object->setProperty("path", item.linkedFile.getFullPathName());
        object->setProperty("name", item.displayName); object->setProperty("metadata", item.metadataJson);
        juce::Array<juce::var> tags; for (const auto& tag : item.tags) tags.add(tag); object->setProperty("tags", tags);
        items.add(juce::var(object));
    }
    root->setProperty("items", items); return juce::var(root);
}
bool LibraryManager::loadJson(const juce::var& json, juce::String* error)
{
    auto* root = json.getDynamicObject();
    if (root == nullptr || root->getProperty("type").toString() != type) { if (error != nullptr) *error = "Wrong library JSON type."; return false; }
    juce::ValueTree tree { rootType }; tree.setProperty("type", type, nullptr);
    const auto items = root->getProperty("items");
    if (auto* array = items.getArray()) for (const auto& value : *array)
    {
        auto* object = value.getDynamicObject(); if (object == nullptr) { if (error != nullptr) *error = "Invalid library JSON item."; return false; }
        juce::ValueTree child { itemNode };
        for (const auto key : { "id", "path", "name", "metadata" }) child.setProperty(key, object->getProperty(key), nullptr);
        juce::StringArray tags; if (auto* tagArray = object->getProperty("tags").getArray()) for (const auto& tag : *tagArray) tags.add(tag.toString());
        child.setProperty("tags", tags.joinIntoString("\n"), nullptr); tree.addChild(child, -1, nullptr);
    }
    return loadValueTree(tree, error);
}
