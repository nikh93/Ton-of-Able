#include "AppSettings.h"

namespace
{
constexpr auto audioDeviceXmlKey = "audioDeviceXml";
constexpr auto audioSelectionKey = "audioSelection";
constexpr auto namLibraryKey = "namLibrary";
constexpr auto irLibraryKey = "irLibrary";
constexpr auto presetLibraryKey = "presetLibrary";
constexpr auto lastPresetKey = "lastPreset";
constexpr auto windowBoundsKey = "windowBounds";
constexpr auto uiStateKey = "uiState";

juce::String treeToText(const juce::ValueTree& tree)
{
    if (! tree.isValid())
        return {};

    return tree.createXml()->toString();
}

juce::ValueTree textToTree(const juce::String& text)
{
    if (text.isEmpty())
        return {};

    const auto xml = juce::parseXML(text);
    return xml != nullptr ? juce::ValueTree::fromXml(*xml) : juce::ValueTree {};
}
}

juce::ValueTree AppSettings::AudioSelection::toValueTree() const
{
    juce::ValueTree state { "audioSelection" };
    state.setProperty("deviceType", deviceType, nullptr);
    state.setProperty("inputDevice", inputDevice, nullptr);
    state.setProperty("outputDevice", outputDevice, nullptr);
    state.setProperty("sampleRate", sampleRate, nullptr);
    state.setProperty("bufferSize", bufferSize, nullptr);
    return state;
}

AppSettings::AudioSelection AppSettings::AudioSelection::fromValueTree(const juce::ValueTree& state)
{
    AudioSelection selection;
    if (! state.isValid() || ! state.hasType("audioSelection"))
        return selection;

    selection.deviceType = state.getProperty("deviceType").toString();
    selection.inputDevice = state.getProperty("inputDevice").toString();
    selection.outputDevice = state.getProperty("outputDevice").toString();
    selection.sampleRate = static_cast<double>(state.getProperty("sampleRate", 0.0));
    selection.bufferSize = static_cast<int>(state.getProperty("bufferSize", 0));
    return selection;
}

juce::PropertiesFile::Options AppSettings::defaultOptions()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Ton of Able";
    options.filenameSuffix = ".settings";
    options.folderName = "Ton of Able";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    options.millisecondsBeforeSaving = 1000;
    return options;
}

AppSettings::AppSettings(juce::PropertiesFile::Options options)
    : properties(std::make_unique<juce::PropertiesFile>(std::move(options)))
{
}

AppSettings::~AppSettings()
{
    flush();
}

void AppSettings::saveAudioDeviceState(const juce::XmlElement& state)
{
    properties->setValue(audioDeviceXmlKey, state.toString());
}

std::unique_ptr<juce::XmlElement> AppSettings::loadAudioDeviceState() const
{
    return juce::parseXML(properties->getValue(audioDeviceXmlKey));
}

void AppSettings::clearAudioDeviceState()
{
    properties->removeValue(audioDeviceXmlKey);
}

void AppSettings::saveAudioSelection(const AudioSelection& selection) { saveTree(audioSelectionKey, selection.toValueTree()); }
AppSettings::AudioSelection AppSettings::loadAudioSelection() const { return AudioSelection::fromValueTree(loadTree(audioSelectionKey)); }
void AppSettings::saveNamLibrary(const juce::ValueTree& library) { saveTree(namLibraryKey, library); }
juce::ValueTree AppSettings::loadNamLibrary() const { return loadTree(namLibraryKey); }
void AppSettings::saveIrLibrary(const juce::ValueTree& library) { saveTree(irLibraryKey, library); }
juce::ValueTree AppSettings::loadIrLibrary() const { return loadTree(irLibraryKey); }
void AppSettings::savePresetLibrary(const juce::ValueTree& presets) { saveTree(presetLibraryKey, presets); }
juce::ValueTree AppSettings::loadPresetLibrary() const { return loadTree(presetLibraryKey); }

void AppSettings::saveLastPreset(const juce::Uuid& presetId)
{
    properties->setValue(lastPresetKey, presetId.toString());
}

void AppSettings::clearLastPreset()
{
    properties->removeValue(lastPresetKey);
}

std::optional<juce::Uuid> AppSettings::loadLastPreset() const
{
    const auto id = properties->getValue(lastPresetKey).trim();
    if (id.isEmpty())
        return std::nullopt;

    const juce::Uuid parsed { id };
    return parsed.isNull() ? std::nullopt : std::optional<juce::Uuid> { parsed };
}

void AppSettings::saveWindowBounds(juce::Rectangle<int> bounds)
{
    properties->setValue(windowBoundsKey, bounds.toString());
}

std::optional<juce::Rectangle<int>> AppSettings::loadWindowBounds() const
{
    const auto text = properties->getValue(windowBoundsKey);
    if (text.isEmpty())
        return std::nullopt;

    const auto bounds = juce::Rectangle<int>::fromString(text);
    return bounds.isEmpty() ? std::nullopt : std::optional<juce::Rectangle<int>> { bounds };
}

void AppSettings::saveUiState(const juce::ValueTree& state) { saveTree(uiStateKey, state); }
juce::ValueTree AppSettings::loadUiState() const { return loadTree(uiStateKey); }

bool AppSettings::flush() { return properties->saveIfNeeded(); }
juce::File AppSettings::settingsFile() const { return properties->getFile(); }

void AppSettings::saveTree(const juce::String& key, const juce::ValueTree& tree)
{
    properties->setValue(key, treeToText(tree));
}

juce::ValueTree AppSettings::loadTree(const juce::String& key) const
{
    return textToTree(properties->getValue(key));
}
