#include "CuratedAmpLibrary.h"

namespace
{
juce::String stringProperty(const juce::DynamicObject& object, const char* name)
{
    return object.getProperty(name).toString();
}
}

bool CuratedAmpLibrary::load(const juce::File& catalogFile, juce::String* error, bool append)
{
    if (!append) entries.clear();
    if (! catalogFile.existsAsFile())
    {
        if (error != nullptr) *error = "Curated amp catalog does not exist.";
        return false;
    }

    const auto document = juce::JSON::parse(catalogFile.loadFileAsString());
    auto* root = document.getDynamicObject();
    if (root == nullptr)
    {
        if (error != nullptr) *error = "Curated amp catalog is not a JSON object.";
        return false;
    }

    auto* models = root->getProperty("models").getArray();
    if (models == nullptr)
    {
        if (error != nullptr) *error = "Curated amp catalog has no models array.";
        return false;
    }

    for (const auto& value : *models)
    {
        auto* object = value.getDynamicObject();
        if (object == nullptr)
            continue;

        CuratedAmpModel model;
        model.ampId = stringProperty(*object, "ampId");
        model.ampName = stringProperty(*object, "ampName");
        model.manufacturer = stringProperty(*object, "manufacturer");
        model.configurationId = stringProperty(*object, "configurationId");
        model.configurationName = stringProperty(*object, "configurationName");
        model.captureType = stringProperty(*object, "captureType");
        model.category = stringProperty(*object, "category");
        model.gainLevel = stringProperty(*object, "gainLevel");
        model.namFile = stringProperty(*object, "namFile");
        model.recommendedIrFile = stringProperty(*object, "recommendedIrFile");
        model.description = stringProperty(*object, "description");
        model.metadataJson = juce::JSON::toString(value, true);
        if (auto* tags = object->getProperty("tags").getArray())
            for (const auto& tag : *tags)
                model.tags.add(tag.toString());

        if (model.ampId.isNotEmpty() && model.configurationId.isNotEmpty() && model.namFile.isNotEmpty())
            entries.add(std::move(model));
    }

    if (entries.isEmpty())
    {
        if (error != nullptr) *error = "Curated amp catalog contains no valid models.";
        return false;
    }
    return true;
}

const CuratedAmpModel* CuratedAmpLibrary::find(const juce::String& ampId,
                                               const juce::String& configurationId) const noexcept
{
    for (const auto& model : entries)
        if (model.ampId == ampId && model.configurationId == configurationId)
            return &model;
    return nullptr;
}

const CuratedAmpModel* CuratedAmpLibrary::findByFile(const juce::String& fileName) const noexcept
{
    for (const auto& model : entries)
        if (model.namFile == fileName)
            return &model;
    return nullptr;
}
