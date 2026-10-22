#pragma once

#include <functional>
#include <map>
#include <set>
#include <memory>

#include <juce_audio_devices/juce_audio_devices.h>

#include "Arrangement.h"
#include "ArrangementEngine.h"
#include "AudioCodecs.h"
#include "TimeStretcher.h"

namespace daw
{
/** Message-thread owner of the arrangement: editing with undo/redo, project
    files, audio loading, Rubber Band renders, recording and mixdown. Every
    change is pushed to the ArrangementEngine as an immutable snapshot. */
class ArrangementController final : private juce::Timer
{
public:
    struct InputChoice
    {
        int channel = -1;      // index into the callback's active inputs, -1 = rig
        juce::String name;
    };

    ArrangementController(ArrangementEngine&, juce::AudioDeviceManager&, std::function<int()> rigLatencySamples);
    ~ArrangementController() override;

    [[nodiscard]] const ArrangementState& state() const noexcept { return current; }
    [[nodiscard]] TimeGrid grid() const noexcept { return TimeGrid::of(current); }
    [[nodiscard]] ArrangementEngine& engine() noexcept { return audio; }

    // Editing. perform() records one undo step.
    void perform(const juce::String& description, const std::function<void(ArrangementState&)>& change);
    /** Applies a change without an undo step (arm, monitor, selection-like state). */
    void modify(const std::function<void(ArrangementState&)>& change);
    // Mouse gestures: every update restarts from the state at beginGesture().
    void beginGesture();
    void updateGesture(const std::function<void(ArrangementState&)>& change);
    void endGesture(const juce::String& description);
    void cancelGesture();
    [[nodiscard]] bool isInGesture() const noexcept { return gestureOrigin != nullptr; }
    bool undo();
    bool redo();
    [[nodiscard]] bool canUndo() const noexcept { return ! undoStack.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return ! redoStack.empty(); }
    [[nodiscard]] juce::String undoDescription() const { return undoNames.empty() ? juce::String() : undoNames.back(); }

    // Tracks.
    juce::Uuid addTrack(const juce::String& name, int inputChannel);
    void removeTrack(const juce::Uuid& id);
    [[nodiscard]] std::vector<InputChoice> availableInputs() const;

    // Per-track effect chains. The app supplies how chains are created and
    // configured from a SoundState tree; the controller owns their lifetime.
    using FxFactory = std::function<std::shared_ptr<TrackProcessor>()>;
    using FxConfigurator = std::function<void(TrackProcessor&, const juce::ValueTree& settings)>;
    void setTrackFxHandlers(FxFactory factory, FxConfigurator configurator);
    void setTrackFx(const juce::Uuid& trackId, const juce::ValueTree& settings, bool enabled, bool print);

    // Send effects: up to four return tracks (A-D) at the bottom, fed by post-fader sends.
    /** Supplies the effect settings a new return track starts with (e.g. a 100 % wet reverb). */
    using ReturnDefaults = std::function<juce::ValueTree(int slot)>;
    void setReturnDefaults(ReturnDefaults defaults) { returnDefaults = std::move(defaults); }
    /** Adds the next free return track; null when all four exist. */
    juce::Uuid addReturnTrack();
    /** Send level of a track to return slot 0-3 (linear gain, 0 = off). */
    void setSend(const juce::Uuid& trackId, int slot, float gain);

    // Freeze: renders a track through its instrument and effect chain into a
    // file, which then plays instead of the live chain until the track changes.
    void freezeTrack(const juce::Uuid& trackId);
    void unfreezeTrack(const juce::Uuid& trackId);
    /** Freezes if needed, then replaces the track's clips by the render and switches its chain off. */
    void flattenTrack(const juce::Uuid& trackId);
    [[nodiscard]] bool isFreezing(const juce::Uuid& trackId) const noexcept { return freezing.count(trackId) > 0; }
    [[nodiscard]] bool isTrackFrozen(const juce::Uuid& trackId) const;

    // Drum machine. Without a factory every drum track uses the built-in DrumSynth.
    using InstrumentFactory = std::function<std::shared_ptr<TrackInstrument>(const juce::ValueTree& settings)>;
    void setInstrumentFactory(InstrumentFactory factory);
    juce::Uuid addDrumTrack(const juce::String& name = {});
    /** Replaces the sound source of a drum track (built-in kit or VST3 settings). */
    juce::Result setDrumInstrument(const juce::Uuid& trackId, const juce::ValueTree& settings);
    /** Inserts a clip playing patternIndex for the given number of bars. */
    juce::Uuid insertPatternClip(const juce::Uuid& trackId, int patternIndex, juce::int64 start, int bars);
    void previewDrum(const juce::Uuid& trackId, int note, int velocity);
    /** Loops a pattern while the transport is stopped; null id stops it. */
    void setPatternAudition(const juce::Uuid& trackId, int patternIndex);

    /** Decodes files in the background (FFmpeg for other codecs and video)
        and inserts them as clips: one file onto trackIndex, several files or
        an invalid index onto new tracks. */
    void importFiles(const juce::Array<juce::File>& files, int trackIndex, juce::int64 start);

    // Transport.
    void togglePlay();
    void stop();
    /** Starts recording on all armed tracks (arming fallbackTrack if none is armed), or stops it. */
    void toggleRecord(int fallbackTrack);
    void locate(juce::int64 position);
    void setLoop(bool enabled, juce::int64 start, juce::int64 end);
    void setTempo(double bpm, int beatsPerBar, int beatUnit);
    void setCountIn(bool enabled) noexcept { countIn = enabled; }
    [[nodiscard]] bool isCountInEnabled() const noexcept { return countIn; }
    void setManualLatencyMs(double value) noexcept { latencyOffsetMs = value; }
    [[nodiscard]] double manualLatencyMs() const noexcept { return manualLatency(); }

    // Project.
    void newProject();
    juce::Result openProject(const juce::File& file);
    juce::Result save();
    juce::Result saveAs(const juce::File& file);
    [[nodiscard]] juce::File projectFile() const noexcept { return project; }
    [[nodiscard]] bool hasUnsavedChanges() const noexcept { return dirty; }
    /** For changes made directly on a TrackMixer (volume, pan, mute, solo). */
    void markDirty() noexcept { dirty = true; }
    /** Renders on a background thread: the master mix into target, or with
        stems = true one file per non-empty track into the folder target. */
    void exportAudio(const juce::File& target, codecs::ExportFormat format, bool loopOnly, bool stems,
                     std::function<void(juce::Result)> done, const juce::Uuid& singleTrack = juce::Uuid::null());

    [[nodiscard]] int pendingWork() const noexcept { return stretcher.pendingJobs() + loadsInFlight; }
    [[nodiscard]] bool isRendering(const Clip& clip) const noexcept { return clip.needsStretch() && ! clip.hasCurrentRender(); }

    std::function<void()> onChanged;
    std::function<void(const juce::String&)> onMessage;

private:
    void timerCallback() override;
    void afterChange(bool structural);
    void publish();
    void ensureSources();
    void ensureRenders();
    void assignSource(const juce::File& file, const AudioDataPtr& data);
    void assignRender(const AudioDataPtr& source, double ratio, double pitch, const AudioDataPtr& rendered);
    void forEachState(const std::function<void(ArrangementState&)>& visit);
    void loadAsync(const juce::File& file, std::function<void(AudioDataPtr, juce::String)> done);
    void finishRecording();
    void message(const juce::String& text) const { if (onMessage) onMessage(text); }
    [[nodiscard]] double manualLatency() const noexcept { return latencyOffsetMs; }
    [[nodiscard]] juce::File audioDirectory() const { return project.getParentDirectory().getChildFile("Audio"); }
    [[nodiscard]] static juce::File defaultProjectFile();
    void syncEngine();
    void instantiateFx();
    void ensureInstruments();
    std::shared_ptr<TrackInstrument> createInstrument(const juce::ValueTree& settings, int blockSize);
    std::shared_ptr<TrackProcessor> createProcessor(const juce::ValueTree& settings);
    /** Takes the chains of these tracks out of live playback for exclusive offline use. */
    void detachFx(const std::set<juce::Uuid>& tracks);
    /** Gives detached chains back to live playback with fresh (reset) state. */
    void reattachFx(const std::set<juce::Uuid>& tracks);
    void startFreeze(const juce::Uuid& trackId, std::function<void(std::shared_ptr<const FrozenRender>)> done);
    void reportThawedTracks();

    ArrangementEngine& audio;
    juce::AudioDeviceManager& devices;
    std::function<int()> rigLatency;
    ArrangementState current;
    std::vector<ArrangementState> undoStack, redoStack;
    std::vector<juce::String> undoNames, redoNames;
    std::unique_ptr<ArrangementState> gestureOrigin;
    juce::File project;
    bool dirty = false;
    bool countIn = true;
    double latencyOffsetMs = 0.0;
    std::map<juce::Uuid, juce::int64> takeLatency;
    FxFactory fxFactory;
    FxConfigurator fxConfigurator;
    InstrumentFactory instrumentFactory;
    ReturnDefaults returnDefaults;
    std::set<juce::Uuid> detachedFx;  // chains in offline use (export, freeze)
    std::set<juce::Uuid> freezing;
    std::set<juce::Uuid> frozenBefore; // to report tracks that were thawed by an edit
    bool exporting = false;

    StretchService stretcher;
    juce::ThreadPool loader { juce::ThreadPoolOptions {}.withThreadName("Audio loader").withNumberOfThreads(2) };
    std::map<juce::String, std::weak_ptr<const AudioData>> loadedFiles;
    std::map<juce::String, bool> loading;
    int loadsInFlight = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};
} // namespace daw
