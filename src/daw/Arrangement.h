#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>

#include "AudioData.h"
#include "Drums.h"

namespace daw
{
/** Number of return tracks (send effects A-D). */
constexpr int maximumReturns = 4;

// Mixer values are shared between the UI and the audio thread and are changed
// without rebuilding the playback snapshot, so fader moves are glitch-free.
struct TrackMixer
{
    std::atomic<float> volumeDb { 0.0f };
    std::atomic<float> pan { 0.0f };      // -1 = left, +1 = right
    std::atomic<bool> muted { false };
    std::atomic<bool> soloed { false };
    std::atomic<float> meter { 0.0f };    // written by the audio thread
    // Post-fader send levels (linear gain, 0 = off) to the return tracks A-D.
    std::array<std::atomic<float>, maximumReturns> sendGain {};

    // Audio-thread state for click-free gain changes.
    float currentLeft = 0.0f, currentRight = 0.0f;
    bool gainInitialised = false;
    std::array<float, maximumReturns> currentSend {};
};

/** A track's insert chain (NAM pedal/amp, IR, EQs, effects, VST3). The app
    implements it with RigChain; the arrangement only needs this interface. */
struct TrackProcessor
{
    virtual ~TrackProcessor() = default;
    /** Message thread, never while the processor is in a published snapshot
        and the device is running. */
    virtual void prepare(double sampleRate, int maximumBlockSize) = 0;
    /** Audio thread: mono input (may be modified) to stereo output. */
    virtual void process(float* mono, float* left, float* right, int numSamples) noexcept = 0;
    [[nodiscard]] virtual int latencySamples() const = 0;
    /** Message thread: the settings with the current state of hosted plugins
        (edited in their own windows), or an invalid tree if nothing changed. */
    [[nodiscard]] virtual juce::ValueTree currentSettings(const juce::ValueTree&) { return {}; }
};

/** Per-track effect settings, shared by all undo states of a track.
    Insert: recorded dry, the chain runs on playback (editable afterwards).
    Print: the chain runs on the input and is recorded into the take. */
struct TrackFx
{
    std::shared_ptr<TrackProcessor> processor;
    std::atomic<bool> enabled { false };
    std::atomic<bool> printWhileRecording { false };
    juce::ValueTree settings; // SoundState tree, message thread only
};

/** The sound source of a drum track; shared by all undo states of a track. */
struct InstrumentSlot
{
    std::shared_ptr<TrackInstrument> instrument;
    juce::ValueTree settings = builtInInstrumentSettings(); // message thread only
};

struct ArrangementState;

/** A track rendered offline through its instrument and effect chain ("freeze").
    Playback uses this file instead of the live chain while the signature
    (clips, patterns, instrument, effects, tempo) still matches the track. */
struct FrozenRender
{
    juce::File file;
    AudioDataPtr data;               // null while loading
    juce::String signature;
};

/** One region of a source file placed on the timeline.

    Timeline values (start, length, fades) are in project samples. offset is
    the clip start inside the *original* source, so changing the stretch ratio
    keeps the same audio material at the clip start. */
struct Clip
{
    juce::Uuid id;
    juce::String name;
    juce::File file;
    juce::int64 sourceSamples = 0;   // length of the original source
    AudioDataPtr source;             // null while loading
    AudioDataPtr stretched;          // Rubber Band render for renderedRatio/renderedPitch
    double renderedRatio = 1.0;
    double renderedPitch = 0.0;

    juce::int64 start = 0;
    juce::int64 length = 0;
    double offset = 0.0;
    double stretchRatio = 1.0;       // > 1 = longer and slower
    double pitchSemitones = 0.0;
    float gainDb = 0.0f;
    juce::int64 fadeIn = 0;
    juce::int64 fadeOut = 0;
    int pattern = -1;                // >= 0: drum pattern clip (no audio source)
    // Loop recording: every pass is a take in the same file. takes[i] is the
    // source position of take i at the clip's start; offset belongs to activeTake.
    std::vector<double> takes;
    int activeTake = 0;

    static constexpr juce::int64 patternSourceSamples = juce::int64(1) << 40;
    [[nodiscard]] bool isPattern() const noexcept { return pattern >= 0; }
    [[nodiscard]] bool hasTakes() const noexcept { return takes.size() > 1; }
    [[nodiscard]] juce::int64 end() const noexcept { return start + length; }
    [[nodiscard]] bool needsStretch() const noexcept;
    [[nodiscard]] bool hasCurrentRender() const noexcept;
    /** Longest possible timeline length given offset and stretch. */
    [[nodiscard]] juce::int64 maximumLength() const noexcept;
    /** Original-source position that plays at timeline position t. */
    [[nodiscard]] double sourcePositionAt(juce::int64 timelinePosition) const noexcept;
};

struct Track
{
    juce::Uuid id;
    juce::String name;
    juce::Colour colour;
    std::shared_ptr<TrackMixer> mixer = std::make_shared<TrackMixer>();
    std::shared_ptr<TrackFx> fx = std::make_shared<TrackFx>();
    bool armed = false;
    bool monitorInput = true;
    int inputChannel = -1;           // -1 = processed NAM rig (stereo), >= 0 = dry device input
    std::vector<Clip> clips;         // sorted by start, never overlapping
    bool drums = false;              // drum track: pattern clips played by its instrument
    std::vector<DrumPattern> patterns;
    std::shared_ptr<InstrumentSlot> instrument = std::make_shared<InstrumentSlot>();
    int returnSlot = -1;             // >= 0: return track (send effect A-D), fed only by sends
    std::shared_ptr<const FrozenRender> frozen;

    [[nodiscard]] bool isReturn() const noexcept { return returnSlot >= 0; }
    /** True when the track has a frozen render that matches its current content. */
    [[nodiscard]] bool isFrozen(const ArrangementState& state) const;
    [[nodiscard]] juce::int64 contentEnd() const noexcept;
};

struct ArrangementState
{
    std::vector<Track> tracks;
    double sampleRate = 48000.0;
    double bpm = 120.0;
    int beatsPerBar = 4;
    int beatUnit = 4;
    bool loopEnabled = false;
    juce::int64 loopStart = 0;
    juce::int64 loopEnd = 0;

    [[nodiscard]] juce::int64 endSample() const noexcept;
    [[nodiscard]] Track* findTrack(const juce::Uuid& id) noexcept;
    [[nodiscard]] const Track* findTrack(const juce::Uuid& id) const noexcept;
    [[nodiscard]] int trackIndex(const juce::Uuid& id) const noexcept;
    [[nodiscard]] Clip* findClip(const juce::Uuid& id, int* trackIndex = nullptr) noexcept;
    [[nodiscard]] const Clip* findClip(const juce::Uuid& id, int* trackIndex = nullptr) const noexcept;
    /** Index of the return track for slot (0-3), or -1. */
    [[nodiscard]] int returnTrackIndex(int slot) const noexcept;
    /** Where new tracks go: before the return tracks, which stay at the bottom. */
    [[nodiscard]] int firstReturnIndex() const noexcept;
};

/** Everything that changes how a track sounds before its fader; a frozen
    render is valid while this matches the signature it was made with. */
juce::String freezeSignature(const Track&, const ArrangementState&);
/** "A" ... "D". */
juce::String returnName(int slot);

/** Tempo grid helper: musical positions <-> samples. */
struct TimeGrid
{
    double sampleRate = 48000.0;
    double bpm = 120.0;
    int beatsPerBar = 4;
    int beatUnit = 4;

    static TimeGrid of(const ArrangementState& state) noexcept { return { state.sampleRate, state.bpm, state.beatsPerBar, state.beatUnit }; }
    [[nodiscard]] double samplesPerBeat() const noexcept { return sampleRate * 60.0 / bpm * 4.0 / beatUnit; }
    [[nodiscard]] double samplesPerBar() const noexcept { return samplesPerBeat() * beatsPerBar; }
    /** division in bars, e.g. 1.0 = bar, 0.25 = quarter of a 4/4 bar; <= 0 disables snapping. */
    [[nodiscard]] juce::int64 snap(juce::int64 position, double divisionInBars) const noexcept;
    /** "Bar.Beat.Sixteenth", 1-based. */
    [[nodiscard]] juce::String formatPosition(juce::int64 position) const;
};

namespace edit
{
/** Creates a new, empty track with a distinct colour. */
Track makeTrack(const ArrangementState& state, const juce::String& name);
/** Creates a clip covering the whole source, placed at start. */
Clip makeClip(const juce::File& file, AudioDataPtr source, juce::int64 start);
/** Creates a drum track with the default patterns and the built-in kit. */
Track makeDrumTrack(const ArrangementState& state, const juce::String& name);
/** Creates a return track for a free slot (A-D); returns nullopt when all four exist. */
std::optional<Track> makeReturnTrack(const ArrangementState& state);
/** Adds a track above the return tracks; returns its index. */
int addTrack(ArrangementState& state, Track track);
/** A clip repeating pattern patternIndex for length samples. */
Clip makePatternClip(int patternIndex, const juce::String& name, juce::int64 start, juce::int64 length);

/** Splits at timeline position; returns the id of the right part. */
std::optional<juce::Uuid> split(ArrangementState&, const juce::Uuid& clipId, juce::int64 at);
/** Moves a clip to another start / track; the moved clip wins overlaps. */
bool move(ArrangementState&, const juce::Uuid& clipId, juce::int64 newStart, int targetTrack);
/** Trims the left edge; the audio stays where it is on the timeline. */
bool trimStart(ArrangementState&, const juce::Uuid& clipId, juce::int64 newStart);
bool trimEnd(ArrangementState&, const juce::Uuid& clipId, juce::int64 newEnd);
/** Time-stretches by dragging an edge: the opposite edge stays fixed. */
bool stretchEnd(ArrangementState&, const juce::Uuid& clipId, juce::int64 newEnd);
bool stretchStart(ArrangementState&, const juce::Uuid& clipId, juce::int64 newStart);
/** Sets an absolute stretch ratio, keeping the start fixed. */
bool setStretchRatio(ArrangementState&, const juce::Uuid& clipId, double ratio);
bool setPitch(ArrangementState&, const juce::Uuid& clipId, double semitones);
bool setFades(ArrangementState&, const juce::Uuid& clipId, juce::int64 fadeIn, juce::int64 fadeOut);
bool remove(ArrangementState&, const juce::Uuid& clipId);
/** Plays take index of a loop recording instead of the current one. */
bool selectTake(ArrangementState&, const juce::Uuid& clipId, int index);
/** Keeps only the active take ("flatten"). */
bool dropOtherTakes(ArrangementState&, const juce::Uuid& clipId);

/** A continuous recording made while the loop [loopStart, loopEnd) repeated:
    file sample i was played at punch + i until loopEnd, then every pass
    restarts at loopStart. latency: what was played at transport position p
    sits at file position + latency. Returns the clips for the timeline: an
    optional lead-in before the loop and one clip for the loop with a take per
    full pass (and a last pass if it covers at least half the loop); the last
    complete take is active. Without a full pass: one plain clip, as before. */
std::vector<Clip> clipsFromLoopRecording(const juce::File&, AudioDataPtr source, juce::int64 recordedSamples,
                                         juce::int64 punch, juce::int64 loopStart, juce::int64 loopEnd,
                                         juce::int64 latency, const juce::String& name);
/** Inserts a copy (with a fresh id) and lets it overwrite what is below. */
juce::Uuid insert(ArrangementState&, Clip clip, int trackIndex);
/** Like insert(), but keeps the clip's id. */
void place(ArrangementState&, Clip clip, int trackIndex);

struct MoveRequest
{
    juce::Uuid clipId;
    juce::int64 start = 0;
    int track = 0;
};
/** Moves several clips at once. All are lifted first, so clips moved together
    never cut each other; then each overwrites what lies below its target. */
bool moveMany(ArrangementState&, const std::vector<MoveRequest>&);
/** Removes or shortens other clips on the track so that clipId is unobstructed. */
void resolveOverlaps(Track&, const juce::Uuid& winningClipId);
void sortClips(Track&);

constexpr double minimumRatio = 0.25;
constexpr double maximumRatio = 4.0;
constexpr juce::int64 minimumLength = 32;
} // namespace edit

// Persistence. Clip files inside projectDirectory are stored relative to it.
juce::ValueTree toValueTree(const ArrangementState&, const juce::File& projectDirectory);
ArrangementState fromValueTree(const juce::ValueTree&, const juce::File& projectDirectory);
/** Rescales every timeline value after a sample-rate change; sources must be reloaded. */
void changeSampleRate(ArrangementState&, double newRate);
} // namespace daw
