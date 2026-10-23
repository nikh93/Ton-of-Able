#pragma once

#include <array>
#include <atomic>
#include <limits>
#include <memory>
#include <set>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "Arrangement.h"

namespace daw
{
/** Options for ArrangementEngine::renderOffline (mixdown, stems, freezing). */
struct OfflineRender
{
    int onlyTrack = -1;         // >= 0: just this track (a return track renders what the others send to it)
    bool preFader = false;      // with onlyTrack: unity gain, centred, no sends (freezing)
    bool useProcessors = false; // run the tracks' effect chains; the caller must own them exclusively
    int processorBlock = 512;   // the chains' prepared maximum block size
    const std::vector<TrackInstrument*>* instruments = nullptr; // offline instrument copies per track
};

/** Realtime side of the arrangement: transport, multitrack playback, input
    monitoring, metronome/count-in and multitrack recording.

    The message thread publishes immutable Snapshots; process() never
    allocates, locks or touches the disk. Recording uses one ThreadedWriter
    per armed track, whose disk thread performs all file I/O. */
class ArrangementEngine final
{
public:
    struct ClipRender
    {
        juce::int64 start = 0, length = 0, fadeIn = 0, fadeOut = 0;
        double offset = 0.0, ratio = 1.0;
        const AudioData* original = nullptr;
        const AudioData* stretched = nullptr; // preferred when present
        float gain = 1.0f;
        int pattern = -1;                     // drum pattern clip
    };

    struct TrackRender
    {
        juce::Uuid id;
        TrackMixer* mixer = nullptr;
        TrackFx* fx = nullptr;
        TrackProcessor* processor = nullptr; // null when the track has no chain
        std::vector<ClipRender> clips; // sorted, non-overlapping
        bool armed = false;
        bool monitorInput = false;
        int inputChannel = -1;
        bool drums = false;
        TrackInstrument* instrument = nullptr;
        const std::vector<DrumPattern>* patterns = nullptr;
        int returnSlot = -1;          // >= 0: return track fed by the sends
        bool frozen = false;          // clips hold the frozen render, chain and instrument are skipped
    };

    struct Snapshot
    {
        std::vector<TrackRender> tracks;
        std::vector<AudioDataPtr> retainedAudio;
        std::vector<std::shared_ptr<TrackMixer>> retainedMixers;
        std::vector<std::shared_ptr<TrackFx>> retainedFx;
        std::vector<std::shared_ptr<TrackProcessor>> retainedProcessors;
        std::vector<std::shared_ptr<TrackInstrument>> retainedInstruments;
        std::vector<std::shared_ptr<const std::vector<DrumPattern>>> retainedPatterns;
        double samplesPerStep = 6000.0;       // one sixteenth note
        std::array<int, maximumReturns> returnTracks { -1, -1, -1, -1 }; // index into tracks per slot
    };


    using OfflineRender = daw::OfflineRender;

    struct RecordTarget
    {
        juce::Uuid trackId;
        int inputChannel = -1; // -1 = processed rig (stereo)
        juce::File file;
        int channels = 2;      // 1 for a dry mono input without printed effects
    };

    struct Take
    {
        RecordTarget target;
        juce::int64 startPosition = 0; // transport position of the first recorded sample
        juce::int64 samples = 0;
        bool overflowed = false;
    };

    static constexpr juce::int64 punchAtNextBlock = std::numeric_limits<juce::int64>::min();
    static constexpr int maximumRecordTracks = 32;

    ArrangementEngine();
    ~ArrangementEngine();
    ArrangementEngine(const ArrangementEngine&) = delete;
    ArrangementEngine& operator=(const ArrangementEngine&) = delete;

    /** detachedFx: tracks whose effect chain is in offline use (export, freeze) and must not run live. */
    [[nodiscard]] static std::shared_ptr<const Snapshot> buildSnapshot(const ArrangementState&, const std::set<juce::Uuid>* detachedFx = nullptr);

    // Message thread.
    void publish(std::shared_ptr<const Snapshot>);
    [[nodiscard]] std::shared_ptr<const Snapshot> currentSnapshot() const { return owned; }
    void releaseRetiredSnapshots();
    /** Waits (briefly) until no audio callback can still use an older snapshot. */
    void waitForAudioCallbacks() const;
    juce::Result startRecording(const std::vector<RecordTarget>&, double sampleRate, juce::int64 punchIn);
    std::vector<Take> stopRecording();

    // Transport: lock-free, any thread.
    void play() noexcept { playing.store(true); }
    void stop() noexcept { playing.store(false); }
    [[nodiscard]] bool isPlaying() const noexcept { return playing.load(); }
    [[nodiscard]] bool isRecording() const noexcept { return recording.load(); }
    void setPosition(juce::int64) noexcept;
    [[nodiscard]] juce::int64 position() const noexcept { return playhead.load(std::memory_order_relaxed); }
    [[nodiscard]] juce::int64 punchInPosition() const noexcept { return punchIn.load(); }
    void setLoop(bool enabled, juce::int64 start, juce::int64 end) noexcept;
    void setTempo(double bpm, int beatsPerBar, int beatUnit) noexcept;
    void setMetronome(bool enabled) noexcept { metronome.store(enabled); }
    [[nodiscard]] bool isMetronomeOn() const noexcept { return metronome.load(); }
    void setMasterGainDb(float value) noexcept { masterDb.store(juce::jlimit(-60.0f, 12.0f, value)); }
    [[nodiscard]] float masterGainDb() const noexcept { return masterDb.load(); }
    [[nodiscard]] float takeMasterMeter() noexcept { return masterMeter.exchange(0.0f); }
    /** Loops one pattern of a drum track while the transport is stopped
        (trackIndex < 0 stops it). */
    void setPatternAudition(int trackIndex, int patternIndex) noexcept;
    [[nodiscard]] int auditionTrack() const noexcept { return auditionTrackIndex.load(); }
    /** Current step of the audition loop, -1 if none is running. */
    [[nodiscard]] int auditionStep() const noexcept { return auditionStepNow.load(); }

    // Device lifecycle and audio thread.
    void prepare(double sampleRate, int maximumBlockSize);
    [[nodiscard]] double preparedSampleRate() const noexcept { return sampleRate.load(); }
    [[nodiscard]] int maximumBlockSize() const noexcept { return maximumBlock > 0 ? maximumBlock : 512; }
    /** inputs: raw device inputs. rig: processed stereo rig output, recorded
        by tracks whose input is the rig. Playback, monitoring and the click
        are added to out. */
    void process(const float* const* inputs, int numInputs,
                 const float* rigLeft, const float* rigRight,
                 float* outLeft, float* outRight, int numSamples) noexcept;

    /** Offline mixdown of [start, start + numSamples) into left/right (overwritten).
        With onlyTrack the track is rendered with its volume and pan, ignoring mute/solo. */
    static void renderOffline(const Snapshot&, juce::int64 start, int numSamples, float* left, float* right,
                              juce::AudioBuffer<float>& scratch, const OfflineRender& options = {});

private:
    struct RecordSlot
    {
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> owner;
        std::atomic<juce::AudioFormatWriter::ThreadedWriter*> writer { nullptr };
        std::atomic<juce::int64> written { 0 };
        std::atomic<bool> overflowed { false };
        RecordTarget target;
    };

    void processChunk(const float* const* inputs, int numInputs, const float* rigLeft, const float* rigRight,
                      float* outLeft, float* outRight, int offset, int numSamples) noexcept;
    void renderTracks(const Snapshot&, const float* const* inputs, int numInputs, const float* rigLeft, const float* rigRight,
                      juce::int64 position, int inputOffset, int mixOffset, int length, bool playingNow, bool recordingNow,
                      float* mixLeft, float* mixRight) noexcept;
    void recordTrack(const juce::Uuid& trackId, const float* left, const float* right, juce::int64 position, int length) noexcept;
    void renderClick(juce::int64 position, float* outLeft, float* outRight, int length) const noexcept;

    std::atomic<const Snapshot*> live { nullptr };
    std::shared_ptr<const Snapshot> owned;
    std::vector<std::shared_ptr<const Snapshot>> retired;
    std::atomic<int> callbacksInFlight { 0 };

    std::atomic<bool> playing { false }, recording { false }, metronome { false };
    std::atomic<juce::int64> playhead { 0 };
    std::atomic<juce::int64> pendingLocate { std::numeric_limits<juce::int64>::min() };
    std::atomic<juce::int64> punchIn { punchAtNextBlock };
    std::atomic<bool> punchedIn { false }; // recording has passed the punch-in (loop passes record from their start)
    std::atomic<bool> loopEnabled { false };
    std::atomic<juce::int64> loopStart { 0 }, loopEnd { 0 };
    std::atomic<double> bpm { 120.0 };
    std::atomic<int> beatsPerBar { 4 }, beatUnit { 4 };
    std::atomic<float> masterDb { 0.0f };
    std::atomic<float> masterMeter { 0.0f };
    std::atomic<double> sampleRate { 0.0 };

    float currentMaster = 1.0f;
    int maximumBlock = 0;
    juce::AudioBuffer<float> trackBuffer, mixBuffer, monoBuffer, instrumentBuffer, returnBuffer;
    static constexpr int maximumEvents = 512;
    std::array<DrumEvent, maximumEvents> events;
    std::atomic<int> auditionTrackIndex { -1 }, auditionPatternIndex { 0 }, auditionStepNow { -1 };
    std::atomic<int> auditionGeneration { 0 };
    int auditionSeenGeneration = -1;
    juce::int64 auditionPosition = 0;
    std::vector<float> silence;

    juce::TimeSliceThread diskThread { "Arrangement recorder" };
    std::array<RecordSlot, maximumRecordTracks> slots;
    std::atomic<int> slotCount { 0 };
};
} // namespace daw
