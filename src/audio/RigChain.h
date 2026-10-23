#pragma once

#include <atomic>
#include <map>

#include <juce_audio_basics/juce_audio_basics.h>

#include "NamProcessor.h"
#include "NamPedalProcessor.h"
#include "IRProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

#include "ChainNodes.h"
#include "presets/ChainLayout.h"
#include "audio/effects/Effects.h"
#include "audio/effects/GraphicEQProcessor.h"
#include "audio/effects/LivePitchShifter.h"
#include "daw/Arrangement.h"

struct SoundState;

/** The complete guitar chain: input gain, a signal graph, output gain.

    The graph follows a chain::Layout: built-in effects (pitch shifter,
    compressor, wah, drive, NAM pedal, NAM amp, cabinet IR, amp EQ, graphic
    EQ, Auto-Wah, Chorus, Delay, Reverb), VST3 plugins (effects, instruments,
    MIDI generators) and MIDI tools in any order, plus extra audio and MIDI
    connections between them. MIDI keyboards feed the "keys" source.

    Used once for the live rig and once per arrangement track that has its
    own effects. Models and IRs load off the audio thread; process() does not
    allocate (third-party VST3 code excepted). setLayout / plugin access:
    message thread. */
class RigChain : public daw::TrackProcessor
{
public:
    RigChain();
    ~RigChain() override;
    RigChain(const RigChain&) = delete;
    RigChain& operator=(const RigChain&) = delete;

    // daw::TrackProcessor
    void prepare(double sampleRate, int maximumBlockSize) override;
    void process(float* mono, float* left, float* right, int numSamples) noexcept override { processWithInputPeak(mono, left, right, numSamples); }
    [[nodiscard]] int latencySamples() const override;
    [[nodiscard]] juce::ValueTree currentSettings(const juce::ValueTree& settings) override;

    /** Same as process(); returns the input peak after the input gain.
        keyboard: MIDI from keyboards for the "keys" source (may be null). */
    float processWithInputPeak(float* mono, float* left, float* right, int numSamples, juce::MidiBuffer* keyboard = nullptr) noexcept;

    /** Builds the graph for a layout. Plugins load only when their state
        differs from what is loaded; unchanged blocks keep running. */
    void setLayout(const chain::Layout&);
    /** The layout with the plugins' current state (e.g. after editing them in their editors). */
    [[nodiscard]] chain::Layout captureLayout(const chain::Layout&);
    /** The hosted plugin of a block, or null. */
    [[nodiscard]] Vst3Slot* plugin(const juce::String& blockId);
    void setBlockBypassed(const juce::String& blockId, bool bypassed);
    /** Plugins the last setLayout could not load ("name: reason"). */
    [[nodiscard]] const juce::StringArray& loadErrors() const noexcept { return errors; }

    // Message thread.
    void loadNamAsync(const juce::File& modelFile) { namProcessor.loadAsync(modelFile); }
    [[nodiscard]] juce::String activeNamName() const { return namProcessor.activeModelName(); }
    void setNamPedal(const juce::String& filePath, bool bypass, float inputDb, float outputDb) { namPedalProcessor.set(filePath, bypass, inputDb, outputDb); }
    [[nodiscard]] juce::String activeNamPedalName() const { return namPedalProcessor.name(); }
    [[nodiscard]] juce::String namPedalError() const { return namPedalProcessor.lastError(); }
    juce::Result loadIr(const juce::File& file) { return irProcessor.loadAsync(file); }
    [[nodiscard]] juce::String activeIrName() const { return irProcessor.loadedName(); }
    void applyEffectState(const SoundState& state);

    // Lock-free parameter setters.
    void setInputGainDb(float value) noexcept { inputGainDb.store(juce::jlimit(-24.0f, 24.0f, value), std::memory_order_relaxed); }
    void setOutputGainDb(float value) noexcept { outputGainDb.store(juce::jlimit(-24.0f, 24.0f, value), std::memory_order_relaxed); }
    void setBassDb(float value) noexcept { eqProcessor.setLowGainDb(value); }
    void setMidDb(float value) noexcept { eqProcessor.setMidGainDb(value); }
    void setTrebleDb(float value) noexcept { eqProcessor.setHighGainDb(value); }
    void setNamBypassed(bool value) noexcept { namBypassRamp.setBypassed(value); }
    void setDriveBypassed(bool value) noexcept { driveProcessor.setBypassed(value); }
    void setIrBypassed(bool value) noexcept { irProcessor.setBypassed(value); }
    void setAutoWahBypassed(bool value) noexcept { autoWahProcessor.setBypassed(value); }
    void setChorusBypassed(bool value) noexcept { chorusProcessor.setBypassed(value); }
    void setDelayBypassed(bool value) noexcept { delayProcessor.setBypassed(value); }
    void setReverbBypassed(bool value) noexcept { reverbProcessor.setBypassed(value); }
    void setGraphicEqBypassed(bool value) noexcept { graphicEq.setBypassed(value); }
    void setCompressor(bool bypassed, float thresholdDb, float ratio, float attackMs, float releaseMs,
                       float kneeDb, float makeupDb, float mix) noexcept
    {
        compressor.setThresholdDb(thresholdDb); compressor.setRatio(ratio); compressor.setAttackMs(attackMs);
        compressor.setReleaseMs(releaseMs); compressor.setKneeDb(kneeDb); compressor.setMakeupDb(makeupDb);
        compressor.setMix(mix); compressor.setBypassed(bypassed);
    }
    [[nodiscard]] float compressorGainReductionDb() const noexcept { return compressor.gainReductionDb(); }
    void setWahPedal(bool bypassed, float position, float heelHz, float toeHz, float q, float mix) noexcept
    {
        wahPedal.setPosition(position); wahPedal.setHeelHz(heelHz); wahPedal.setToeHz(toeHz);
        wahPedal.setQ(q); wahPedal.setMix(mix); wahPedal.setBypassed(bypassed);
    }
    /** Lock-free, any rate (e.g. from the mouse): 0 heel ... 1 toe. */
    void setWahPosition(float position) noexcept { wahPedal.setPosition(position); }
    void setGraphicEqBandDb(int band, float db) noexcept { graphicEq.setBandGainDb(band, db); }
    void setPitch(bool bypassed, float semitones, float mix, bool formantPreserved, bool highQuality, bool analogOctave) noexcept
    {
        pitchShifter.setSemitones(semitones);
        pitchShifter.setMix(mix);
        pitchShifter.setFormantPreserved(formantPreserved);
        pitchShifter.setMode(highQuality ? nam::effects::LivePitchShifter::Mode::highQuality : nam::effects::LivePitchShifter::Mode::lowLatency);
        pitchShifter.setAnalogOctave(analogOctave);
        pitchShifter.setBypassed(bypassed);
    }
    [[nodiscard]] int pitchLatencySamples() const noexcept { return pitchShifter.latencySamples(); }

private:
    /** The processing of one built-in block (bound once, no lookups on the audio thread). */
    chainnodes::Builtin::Process builtinProcess(const juce::String& kind);
    struct Block
    {
        juce::AudioProcessorGraph::NodeID node;
        juce::String kind;
        juce::String loadedState; // plugin state the instance was created from
    };
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorGraph::NodeID audioIn, audioOut, midiIn;
    std::map<juce::String, Block> blocks;
    std::vector<juce::AudioProcessorGraph::NodeID> linkNodes;
    chain::Layout currentLayout;
    juce::StringArray errors;
    juce::MidiBuffer graphMidi;
    double preparedRate = 48000.0;

    NamProcessor namProcessor;
    NamPedalProcessor namPedalProcessor;
    IRProcessor irProcessor;
    juce::AudioBuffer<float> namDryBuffer;
    nam::effects::EQProcessor eqProcessor;
    nam::effects::GraphicEQProcessor graphicEq;
    nam::effects::LivePitchShifter pitchShifter;
    nam::effects::DriveProcessor driveProcessor;
    nam::effects::CompressorProcessor compressor;
    nam::effects::WahPedalProcessor wahPedal;
    nam::effects::AutoWahProcessor autoWahProcessor;
    nam::effects::ChorusProcessor chorusProcessor;
    nam::effects::DelayProcessor delayProcessor;
    nam::effects::ReverbProcessor reverbProcessor;
    std::atomic<float> inputGainDb { 0.0f };
    std::atomic<float> outputGainDb { 0.0f };
    nam::effects::BypassRamp namBypassRamp;
    nam::effects::SmoothedParameter namInputTrim { 0.f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
    int maximumBlock = 0;
};
