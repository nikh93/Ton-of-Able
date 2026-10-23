#pragma once

#include <array>
#include <map>
#include <memory>
#include <vector>

#include "Drums.h"

namespace daw
{
/** A sampled drum kit read from an SFZ file: velocity layers, panning and
    choke groups (group / off_by) and random round robins (lorand / hirand).
    Only the opcodes drum kits need are read: key, lokey/hikey, lovel/hivel,
    lorand/hirand, sample, pan, volume, group, off_by. Samples may be mono or stereo.

    The decoded samples are shared between all tracks (and export copies)
    that use the same kit at the same sample rate. */
class SampleKit final : public TrackInstrument
{
public:
    struct Region
    {
        int loKey = 0, hiKey = 127, loVel = 1, hiVel = 127;
        float loRand = 0.0f, hiRand = 1.0f;
        juce::String sample;
        float pan = 0.0f;     // -100 .. 100
        float volumeDb = 0.0f;
        int group = 0, offBy = 0;
    };

    /** Parses SFZ text; regions inherit <global>, <master> and <group> opcodes. */
    [[nodiscard]] static std::vector<Region> parseSfz(const juce::String& text);

    /** One of the bundled kits. noteMap maps General MIDI notes of the drum
        machine lanes to the kit's own notes where they differ. */
    struct KitInfo
    {
        const char* id;
        const char* name;
        const char* sfzFile;
        std::vector<std::pair<int, int>> noteMap;
    };
    [[nodiscard]] static const std::vector<KitInfo>& bundledKits();
    [[nodiscard]] static const KitInfo* findKit(const juce::String& id);
    /** Instrument settings that select a bundled kit. */
    [[nodiscard]] static juce::ValueTree settingsFor(const juce::String& kitId);
    /** The kit new drum tracks start with. */
    [[nodiscard]] static juce::String defaultKitId() { return "sm-drums"; }

    /** Loads the kit; returns null (and sets error) if the SFZ or a sample is missing. */
    [[nodiscard]] static std::shared_ptr<SampleKit> load(const juce::File& kitsDirectory, const juce::String& kitId, juce::String& error);

    void prepare(double sampleRate, int maximumBlockSize) override;
    void render(const DrumEvent* events, int numEvents, float* left, float* right, int numSamples) noexcept override;
    [[nodiscard]] std::shared_ptr<TrackInstrument> createOfflineCopy() const override;
    [[nodiscard]] juce::String name() const override { return displayName; }

    struct Sample
    {
        std::vector<float> left, right; // right is empty for mono samples
        double sampleRate = 44100.0;
    };
    struct Layer
    {
        int loVel = 1, hiVel = 127;
        float loRand = 0.0f, hiRand = 1.0f;
        std::shared_ptr<const Sample> sample;
        float gainLeft = 1.0f, gainRight = 1.0f;
        int group = 0, offBy = 0;
    };
    /** Decoded kit at one sample rate: per note, the velocity layers. */
    struct Data
    {
        std::array<std::vector<Layer>, 128> notes;
        double sampleRate = 0.0;
    };

private:
    SampleKit(juce::File sfz, juce::String kitName, std::array<int, 128> map, std::shared_ptr<const Data> source);

    struct Voice
    {
        const Layer* layer = nullptr;
        size_t position = 0;
        float gain = 0.0f;
        float fade = 1.0f;
        bool choking = false;
        juce::uint32 age = 0;
    };
    void trigger(int note, int velocity) noexcept;

    juce::File sfzFile;
    juce::String displayName;
    std::array<int, 128> noteMap {};
    std::shared_ptr<const Data> original; // at the samples' own rate
    std::shared_ptr<const Data> data;     // resampled to the playback rate
    double sampleRate = 48000.0;
    std::array<Voice, 64> voices;
    juce::uint32 clock = 0;
    juce::uint32 random = 0x2545f491u;
};
} // namespace daw
