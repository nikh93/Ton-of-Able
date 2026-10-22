#pragma once

#include <array>
#include <optional>

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

/** The complete non-audio state for one sound. Field names form the persistent
    schema; values are deliberately primitive for stable ValueTree/JSON storage. */
struct SoundState
{
    juce::String namId, irId;
    juce::String ampId, configurationId;
    juce::String rigId;
    float inputGainDb = 0.0f, outputGainDb = 0.0f;
    float bassDb = 0.0f, midDb = 0.0f, trebleDb = 0.0f;
    bool namBypassed = false, irBypassed = false;
    float namInputTrimDb = 0.0f;
    juce::String namPedalPath;
    bool namPedalBypassed = true;
    float namPedalInputDb = 0.0f, namPedalOutputDb = 0.0f;
    juce::ValueTree preVst3State, postVst3State;
    bool driveBypassed = true;
    int driveType = 0;
    float driveAmount = 0.35f, driveTone = 0.5f, driveWarmth = 0.5f, drivePresence = 0.5f, driveSustain = 0.5f, driveBias = 0.35f, driveLevel = 0.8f, driveMix = 1.0f;
    bool autoWahBypassed = true, chorusBypassed = true, delayBypassed = true, reverbBypassed = true;
    float autoWahSensitivity = 0.5f, autoWahAttackMs = 10.0f, autoWahReleaseMs = 150.0f, autoWahMinimumHz = 250.0f, autoWahMaximumHz = 1800.0f, autoWahQ = 2.0f, autoWahMix = 1.0f;
    bool autoWahUpward = true;
    float chorusRateHz = 0.8f, chorusDepthMs = 3.0f, chorusDelayMs = 12.0f, chorusFeedback = 0.0f, chorusMix = 0.3f;
    float delayTimeMs = 380.0f, delayFeedback = 0.35f, delayMix = 0.25f, delayLowCutHz = 80.0f, delayHighCutHz = 7000.0f;
    float reverbSize = 0.5f, reverbDecay = 0.5f, reverbDamping = 0.5f, reverbPreDelayMs = 0.0f, reverbMix = 0.2f, reverbLowCutHz = 80.0f, reverbHighCutHz = 12000.0f;
    // Ten-band graphic EQ after the amp EQ: 31.5 Hz ... 16 kHz, +/-12 dB.
    bool graphicEqBypassed = true;
    std::array<float, 10> graphicEqDb {};
    // Live pitch shifter (Rubber Band) in front of the rig, e.g. an octave down.
    bool pitchBypassed = true;
    float pitchSemitones = -12.0f, pitchMix = 1.0f;
    bool pitchFormantPreserved = false, pitchHighQuality = false;

    juce::ValueTree toValueTree() const;
    static std::optional<SoundState> fromValueTree(const juce::ValueTree&, juce::String* error = nullptr);
    static SoundState defaults() { return {}; }
    // Rack edits carry a snapshot; retain live amp and NAM pedal controls/assets.
    void retainAmpFrom(const SoundState& live)
    {
        inputGainDb = live.inputGainDb; outputGainDb = live.outputGainDb;
        bassDb = live.bassDb; midDb = live.midDb; trebleDb = live.trebleDb;
        namBypassed = live.namBypassed; irBypassed = live.irBypassed;
        namId = live.namId; irId = live.irId;
        ampId = live.ampId; configurationId = live.configurationId;
        rigId = live.rigId;
        namPedalPath = live.namPedalPath; namPedalBypassed = live.namPedalBypassed;
        namPedalInputDb = live.namPedalInputDb; namPedalOutputDb = live.namPedalOutputDb;
        graphicEqBypassed = live.graphicEqBypassed; graphicEqDb = live.graphicEqDb;
        pitchBypassed = live.pitchBypassed; pitchSemitones = live.pitchSemitones; pitchMix = live.pitchMix;
        pitchFormantPreserved = live.pitchFormantPreserved; pitchHighQuality = live.pitchHighQuality;
    }
};

struct Preset
{
    juce::Uuid id;
    juce::String name;
    SoundState sound;
    juce::Time created;
    juce::Time modified;
    juce::ValueTree toValueTree() const;
    static std::optional<Preset> fromValueTree(const juce::ValueTree&, juce::String* error = nullptr);
};

/** Value object for instant A/B control. A state controller may asynchronously
    prepare the referenced NAM/IR before sending either state to the audio path. */
struct ABState { SoundState a, b; bool usingA = true; juce::ValueTree toValueTree() const; static std::optional<ABState> fromValueTree(const juce::ValueTree&, juce::String* error = nullptr); };

class PresetManager
{
public:
    const juce::Array<Preset>& presets() const noexcept { return entries; }
    const Preset* find(const juce::Uuid&) const noexcept;
    std::optional<juce::Uuid> create(juce::String name, const SoundState&);
    bool replace(const juce::Uuid&, const SoundState&);
    bool rename(const juce::Uuid&, juce::String name);
    bool erase(const juce::Uuid&);
    juce::ValueTree toValueTree() const;
    bool loadValueTree(const juce::ValueTree&, juce::String* error = nullptr);
    static juce::var valueTreeToJson(const juce::ValueTree&);
    static std::optional<juce::ValueTree> valueTreeFromJson(const juce::var&, juce::String* error = nullptr);
private: juce::Array<Preset> entries;
};
