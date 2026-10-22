#pragma once

#include <memory>
#include <optional>

#include <juce_audio_utils/juce_audio_utils.h>

/**
    Persistent, non-realtime application state.

    AppSettings is intended for the message thread or a dedicated persistence
    worker only. It performs allocations, XML conversion and filesystem I/O;
    no method is safe to call from an audio callback.
*/
class AppSettings final
{
public:
    struct AudioSelection
    {
        juce::String deviceType;
        juce::String inputDevice;
        juce::String outputDevice;
        double sampleRate = 0.0;
        int bufferSize = 0;

        [[nodiscard]] juce::ValueTree toValueTree() const;
        [[nodiscard]] static AudioSelection fromValueTree(const juce::ValueTree& state);
    };

    /** Creates settings in the platform's user application-data directory. */
    explicit AppSettings(juce::PropertiesFile::Options options = defaultOptions());
    ~AppSettings();

    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;

    [[nodiscard]] static juce::PropertiesFile::Options defaultOptions();

    // Audio device state is the XML returned by AudioDeviceManager::createStateXml().
    void saveAudioDeviceState(const juce::XmlElement& state);
    [[nodiscard]] std::unique_ptr<juce::XmlElement> loadAudioDeviceState() const;
    void clearAudioDeviceState();

    void saveAudioSelection(const AudioSelection& selection);
    [[nodiscard]] AudioSelection loadAudioSelection() const;

    // The supplied roots are copied and stored as XML, preserving ValueTree types.
    void saveNamLibrary(const juce::ValueTree& library);
    [[nodiscard]] juce::ValueTree loadNamLibrary() const;
    void saveIrLibrary(const juce::ValueTree& library);
    [[nodiscard]] juce::ValueTree loadIrLibrary() const;
    void savePresetLibrary(const juce::ValueTree& presets);
    [[nodiscard]] juce::ValueTree loadPresetLibrary() const;

    void saveLastPreset(const juce::Uuid& presetId);
    void clearLastPreset();
    [[nodiscard]] std::optional<juce::Uuid> loadLastPreset() const;

    void saveWindowBounds(juce::Rectangle<int> bounds);
    [[nodiscard]] std::optional<juce::Rectangle<int>> loadWindowBounds() const;

    // UI state is deliberately schema-free, so individual views can evolve independently.
    void saveUiState(const juce::ValueTree& state);
    [[nodiscard]] juce::ValueTree loadUiState() const;

    /** Forces PropertiesFile to flush pending changes. Call during orderly shutdown. */
    bool flush();
    [[nodiscard]] juce::File settingsFile() const;

private:
    void saveTree(const juce::String& key, const juce::ValueTree& tree);
    [[nodiscard]] juce::ValueTree loadTree(const juce::String& key) const;

    std::unique_ptr<juce::PropertiesFile> properties;
};
