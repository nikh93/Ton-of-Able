#pragma once

#include "Drums.h"

namespace daw
{
/** The built-in instrument of MIDI tracks: a small polyphonic synthesizer
    (two detuned saws and a sub oscillator, filter and amp envelopes), so MIDI
    tracks sound without any plugin. Presets: pad, keys, bass, lead. */
class Synth final : public TrackInstrument
{
public:
    enum class Preset { pad, keys, bass, lead };
    static constexpr int numPresets = 4;
    [[nodiscard]] static juce::String presetName(Preset);
    [[nodiscard]] static Preset presetFromSettings(const juce::ValueTree&);

    explicit Synth(Preset = Preset::pad);
    void prepare(double sampleRate, int maximumBlockSize) override;
    void render(const DrumEvent* events, int numEvents, float* left, float* right, int numSamples) noexcept override;
    [[nodiscard]] std::shared_ptr<TrackInstrument> createOfflineCopy() const override { return std::make_shared<Synth>(preset); }
    [[nodiscard]] juce::String name() const override { return "Synth: " + presetName(preset); }

private:
    struct Parameters
    {
        float attack, decay, sustain, release; // seconds, level
        float cutoff, envelopeAmount, resonance; // Hz, octaves
        float detune, sub, gain;
    };
    struct Voice
    {
        int note = -1;
        bool held = false;
        float velocity = 0.0f;
        float level = 0.0f;        // amp envelope
        int stage = 0;             // 0 idle, 1 attack, 2 decay/sustain, 3 release
        double phase1 = 0.0, phase2 = 0.0, phaseSub = 0.0;
        float low = 0.0f, band = 0.0f;
        juce::uint32 age = 0;
    };

    void noteOn(int note, int velocity) noexcept;
    void noteOff(int note) noexcept;
    void allNotesOff() noexcept;
    [[nodiscard]] float renderVoice(Voice&) noexcept;

    Preset preset;
    Parameters parameters;
    double sampleRate = 48000.0;
    std::array<Voice, 16> voices;
    juce::uint32 clock = 0;
};

/** Instrument settings of the built-in synth (default for new MIDI tracks). */
[[nodiscard]] juce::ValueTree synthInstrumentSettings(Synth::Preset = Synth::Preset::pad);
} // namespace daw
