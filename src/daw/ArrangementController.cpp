#include "ArrangementController.h"

#include <algorithm>
#include <cmath>

#include "AudioCodecs.h"
#include "DawText.h"

namespace daw
{
namespace
{
constexpr size_t undoLimit = 100;

juce::File uniqueFile(const juce::File& directory, const juce::String& stem, const juce::String& extension)
{
    const auto safe = juce::File::createLegalFileName(stem).trim();
    return directory.getNonexistentChildFile(safe.isEmpty() ? "Audio" : safe, extension, false);
}

constexpr int offlineBlock = 8192;

// Renders [from, to) block by block and hands each block to write.
bool renderRange(const ArrangementEngine::Snapshot& snapshot, const ArrangementEngine::OfflineRender& options, juce::int64 from, juce::int64 to,
                 float gain, const std::function<bool(const float* const*, int)>& write)
{
    juce::AudioBuffer<float> block(2, offlineBlock), scratch;
    for (auto position = from; position < to; position += offlineBlock)
    {
        const auto count = static_cast<int>(std::min<juce::int64>(offlineBlock, to - position));
        ArrangementEngine::renderOffline(snapshot, position, count, block.getWritePointer(0), block.getWritePointer(1), scratch, options);
        block.applyGain(0, count, gain);
        if (! write(block.getArrayOfReadPointers(), count))
            return false;
    }
    return true;
}

// Runs a function on the message thread and waits for it (from a worker thread).
void onMessageThread(std::function<void()> function)
{
    auto done = std::make_shared<juce::WaitableEvent>();
    juce::MessageManager::callAsync([function = std::move(function), done] { function(); done->signal(); });
    done->wait(10000);
}
} // namespace

ArrangementController::ArrangementController(ArrangementEngine& engineToUse, juce::AudioDeviceManager& deviceManager, std::function<int()> rigLatencySamples)
    : audio(engineToUse),
      devices(deviceManager),
      rigLatency(std::move(rigLatencySamples)),
      stretcher([this](AudioDataPtr source, double ratio, double pitch, AudioDataPtr rendered) { assignRender(source, ratio, pitch, rendered); })
{
    newProject();
    startTimerHz(15);
}

ArrangementController::~ArrangementController()
{
    stopTimer();
    alive.reset();
    if (audio.isRecording())
        audio.stopRecording();
    loader.removeAllJobs(true, 30000);
}

//==============================================================================
void ArrangementController::forEachState(const std::function<void(ArrangementState&)>& visit)
{
    visit(current);
    for (auto& state : undoStack) visit(state);
    for (auto& state : redoStack) visit(state);
    if (gestureOrigin != nullptr) visit(*gestureOrigin);
}

void ArrangementController::perform(const juce::String& description, const std::function<void(ArrangementState&)>& change)
{
    if (gestureOrigin != nullptr)
        cancelGesture();
    undoStack.push_back(current);
    undoNames.push_back(description);
    if (undoStack.size() > undoLimit)
    {
        undoStack.erase(undoStack.begin());
        undoNames.erase(undoNames.begin());
    }
    redoStack.clear();
    redoNames.clear();
    change(current);
    afterChange(true);
}

void ArrangementController::modify(const std::function<void(ArrangementState&)>& change)
{
    change(current);
    afterChange(true);
}

void ArrangementController::beginGesture()
{
    gestureOrigin = std::make_unique<ArrangementState>(current);
}

void ArrangementController::updateGesture(const std::function<void(ArrangementState&)>& change)
{
    if (gestureOrigin == nullptr)
        return;
    current = *gestureOrigin;
    change(current);
    afterChange(false);
}

void ArrangementController::endGesture(const juce::String& description)
{
    if (gestureOrigin == nullptr)
        return;
    undoStack.push_back(std::move(*gestureOrigin));
    undoNames.push_back(description);
    if (undoStack.size() > undoLimit)
    {
        undoStack.erase(undoStack.begin());
        undoNames.erase(undoNames.begin());
    }
    gestureOrigin.reset();
    redoStack.clear();
    redoNames.clear();
    afterChange(true);
}

void ArrangementController::cancelGesture()
{
    if (gestureOrigin == nullptr)
        return;
    current = std::move(*gestureOrigin);
    gestureOrigin.reset();
    afterChange(true);
}

bool ArrangementController::undo()
{
    if (undoStack.empty() || gestureOrigin != nullptr)
        return false;
    redoStack.push_back(std::move(current));
    redoNames.push_back(undoNames.back());
    current = std::move(undoStack.back());
    undoStack.pop_back();
    message(text("Rückgängig: ") + undoNames.back());
    undoNames.pop_back();
    afterChange(true);
    return true;
}

bool ArrangementController::redo()
{
    if (redoStack.empty() || gestureOrigin != nullptr)
        return false;
    undoStack.push_back(std::move(current));
    undoNames.push_back(redoNames.back());
    current = std::move(redoStack.back());
    redoStack.pop_back();
    message("Wiederholen: " + redoNames.back());
    redoNames.pop_back();
    afterChange(true);
    return true;
}

void ArrangementController::afterChange(bool structural)
{
    dirty = true;
    syncEngine();
    ensureInstruments();
    publish();
    if (structural)
    {
        ensureSources();
        ensureRenders();
    }
    reportThawedTracks();
    if (onChanged)
        onChanged();
}

void ArrangementController::publish()
{
    audio.publish(ArrangementEngine::buildSnapshot(current, &detachedFx));
}

void ArrangementController::syncEngine()
{
    audio.setTempo(current.bpm, current.beatsPerBar, current.beatUnit);
    audio.setLoop(current.loopEnabled, current.loopStart, current.loopEnd);
}

//==============================================================================
void ArrangementController::loadAsync(const juce::File& file, std::function<void(AudioDataPtr, juce::String)> done)
{
    const auto rate = current.sampleRate;
    ++loadsInFlight;
    std::weak_ptr<bool> weakAlive = alive;
    loader.addJob([this, file, rate, done, weakAlive]
    {
        juce::String error;
        std::shared_ptr<const AudioData> data = loadAudioFile(file, rate, error);
        juce::MessageManager::callAsync([this, data, error, done, weakAlive]
        {
            if (weakAlive.expired())
                return;
            --loadsInFlight;
            done(data, error);
        });
    });
}

void ArrangementController::ensureSources()
{
    std::vector<juce::File> missing;
    for (const auto& track : current.tracks)
        for (const auto& clip : track.clips)
            if (clip.source == nullptr && clip.file != juce::File())
            {
                const auto key = clip.file.getFullPathName();
                if (const auto cached = loadedFiles[key].lock())
                {
                    juce::MessageManager::callAsync([this, file = clip.file, cached, weakAlive = std::weak_ptr<bool>(alive)]
                    {
                        if (! weakAlive.expired())
                            assignSource(file, cached);
                    });
                    continue;
                }
                if (! loading[key] && std::find(missing.begin(), missing.end(), clip.file) == missing.end())
                    missing.push_back(clip.file);
            }

    for (const auto& track : current.tracks)
        if (track.frozen != nullptr && track.frozen->data == nullptr && track.frozen->file.existsAsFile())
        {
            const auto& file = track.frozen->file;
            if (const auto cached = loadedFiles[file.getFullPathName()].lock())
                juce::MessageManager::callAsync([this, file, cached, weakAlive = std::weak_ptr<bool>(alive)]
                {
                    if (! weakAlive.expired())
                        assignSource(file, cached);
                });
            else if (! loading[file.getFullPathName()] && std::find(missing.begin(), missing.end(), file) == missing.end())
                missing.push_back(file);
        }

    for (const auto& file : missing)
    {
        loading[file.getFullPathName()] = true;
        loadAsync(file, [this, file](AudioDataPtr data, juce::String error)
        {
            loading.erase(file.getFullPathName());
            if (data == nullptr)
            {
                message(error);
                return;
            }
            assignSource(file, data);
        });
    }
}

void ArrangementController::assignSource(const juce::File& file, const AudioDataPtr& data)
{
    loadedFiles[file.getFullPathName()] = data;
    bool changed = false;
    forEachState([&](ArrangementState& state)
    {
        for (auto& track : state.tracks)
            if (track.frozen != nullptr && track.frozen->file == file && track.frozen->data == nullptr
                && std::abs(data->sampleRate - state.sampleRate) < 1.0e-6)
            {
                auto loaded = std::make_shared<FrozenRender>(*track.frozen);
                loaded->data = data;
                track.frozen = std::move(loaded);
                changed = true;
            }
    });
    forEachState([&](ArrangementState& state)
    {
        for (auto& track : state.tracks)
            for (auto& clip : track.clips)
                if (clip.file == file && clip.source == nullptr && std::abs(data->sampleRate - state.sampleRate) < 1.0e-6)
                {
                    clip.source = data;
                    clip.sourceSamples = data->length();
                    clip.length = std::max<juce::int64>(1, std::min(clip.length, clip.maximumLength()));
                    changed = true;
                }
    });
    if (! changed)
        return;
    publish();
    ensureRenders();
    if (onChanged)
        onChanged();
}

void ArrangementController::ensureRenders()
{
    for (auto& track : current.tracks)
        for (auto& clip : track.clips)
        {
            if (clip.source == nullptr || ! clip.needsStretch() || clip.hasCurrentRender())
                continue;
            // Reuse a render made for another clip (e.g. after a split or undo).
            AudioDataPtr existing;
            forEachState([&](ArrangementState& state)
            {
                for (const auto& t : state.tracks)
                    for (const auto& other : t.clips)
                        if (existing == nullptr && other.source == clip.source && other.stretched != nullptr
                            && std::abs(other.renderedRatio - clip.stretchRatio) < 1.0e-9 && std::abs(other.renderedPitch - clip.pitchSemitones) < 1.0e-9)
                            existing = other.stretched;
            });
            if (existing != nullptr)
            {
                clip.stretched = existing;
                clip.renderedRatio = clip.stretchRatio;
                clip.renderedPitch = clip.pitchSemitones;
                continue;
            }
            stretcher.request(clip.source, clip.stretchRatio, clip.pitchSemitones);
        }
    publish();
    stretcher.cancelUnwanted([this](const AudioData* source, double ratio, double pitch)
    {
        for (const auto& track : current.tracks)
            for (const auto& clip : track.clips)
                if (clip.source.get() == source && std::abs(clip.stretchRatio - ratio) < 1.0e-9 && std::abs(clip.pitchSemitones - pitch) < 1.0e-9)
                    return true;
        return false;
    });
}

void ArrangementController::assignRender(const AudioDataPtr& source, double ratio, double pitch, const AudioDataPtr& rendered)
{
    forEachState([&](ArrangementState& state)
    {
        for (auto& track : state.tracks)
            for (auto& clip : track.clips)
                if (clip.source == source && std::abs(clip.stretchRatio - ratio) < 1.0e-9 && std::abs(clip.pitchSemitones - pitch) < 1.0e-9)
                {
                    clip.stretched = rendered;
                    clip.renderedRatio = ratio;
                    clip.renderedPitch = pitch;
                }
    });
    publish();
    if (onChanged)
        onChanged();
}

//==============================================================================
juce::Uuid ArrangementController::addTrack(const juce::String& name, int inputChannel)
{
    juce::Uuid id;
    perform("Spur erstellen", [&](ArrangementState& state)
    {
        auto track = edit::makeTrack(state, name.isNotEmpty() ? name : "Audio " + juce::String(state.firstReturnIndex() + 1));
        track.inputChannel = inputChannel;
        id = track.id;
        edit::addTrack(state, std::move(track));
    });
    return id;
}

void ArrangementController::removeTrack(const juce::Uuid& id)
{
    perform(text("Spur löschen"), [&](ArrangementState& state)
    {
        state.tracks.erase(std::remove_if(state.tracks.begin(), state.tracks.end(), [&](const Track& t) { return t.id == id; }), state.tracks.end());
    });
}

void ArrangementController::setTrackFxHandlers(FxFactory factory, FxConfigurator configurator)
{
    fxFactory = std::move(factory);
    fxConfigurator = std::move(configurator);
    instantiateFx();
}

std::shared_ptr<TrackProcessor> ArrangementController::createProcessor(const juce::ValueTree& settings)
{
    if (! fxFactory)
        return {};
    auto processor = fxFactory();
    if (processor == nullptr)
        return {};
    const auto rate = audio.preparedSampleRate() > 0.0 ? audio.preparedSampleRate() : current.sampleRate;
    processor->prepare(rate, audio.maximumBlockSize()); // not yet visible to the audio thread
    if (fxConfigurator)
        fxConfigurator(*processor, settings);
    return processor;
}

void ArrangementController::instantiateFx()
{
    for (auto& track : current.tracks)
        if (track.fx->processor == nullptr && track.fx->settings.isValid())
            track.fx->processor = createProcessor(track.fx->settings);
    publish();
}

void ArrangementController::setTrackFx(const juce::Uuid& trackId, const juce::ValueTree& settings, bool enabled, bool print)
{
    auto* track = current.findTrack(trackId);
    if (track == nullptr)
        return;
    auto& fx = *track->fx;
    fx.settings = settings.createCopy();
    if (fx.processor == nullptr)
        fx.processor = createProcessor(fx.settings);
    else if (fxConfigurator)
        fxConfigurator(*fx.processor, fx.settings);
    fx.enabled = enabled && fx.processor != nullptr;
    fx.printWhileRecording = print;
    dirty = true;
    publish();
    reportThawedTracks();
    if (onChanged)
        onChanged();
}

//==============================================================================
juce::Uuid ArrangementController::addReturnTrack()
{
    auto track = edit::makeReturnTrack(current);
    if (! track)
    {
        message(text("Alle vier Return-Spuren (A–D) sind schon angelegt."));
        return juce::Uuid::null();
    }
    const auto id = track->id;
    const auto slot = track->returnSlot;
    perform("Return-Spur erstellen", [&](ArrangementState& state) { edit::addTrack(state, *track); });
    if (returnDefaults)
        setTrackFx(id, returnDefaults(slot), true, false);
    message(text("Return ") + returnName(slot) + text(" angelegt – mit den SEND-Reglern der Spuren beschicken."));
    return id;
}

void ArrangementController::setSend(const juce::Uuid& trackId, int slot, float gain)
{
    auto* track = current.findTrack(trackId);
    if (track == nullptr || track->isReturn() || slot < 0 || slot >= maximumReturns)
        return;
    track->mixer->sendGain[static_cast<size_t>(slot)] = juce::jlimit(0.0f, 2.0f, gain);
    dirty = true;
    if (onChanged)
        onChanged();
}

bool ArrangementController::isTrackFrozen(const juce::Uuid& trackId) const
{
    const auto* track = current.findTrack(trackId);
    return track != nullptr && track->isFrozen(current);
}

void ArrangementController::reportThawedTracks()
{
    std::set<juce::Uuid> now;
    for (const auto& track : current.tracks)
        if (track.isFrozen(current))
            now.insert(track.id);
    for (const auto& id : frozenBefore)
        if (now.count(id) == 0)
            if (const auto* track = current.findTrack(id); track != nullptr && track->frozen != nullptr)
                message(text("Spur \"") + track->name + text("\" ist aufgetaut, weil sie bearbeitet wurde (erneut einfrieren spart wieder Rechenleistung)."));
    frozenBefore = std::move(now);
}

void ArrangementController::detachFx(const std::set<juce::Uuid>& tracks)
{
    for (const auto& id : tracks)
        detachedFx.insert(id);
    publish();
    audio.waitForAudioCallbacks();
    // Exclusive now: start the offline render from a clean state.
    const auto rate = audio.preparedSampleRate() > 0.0 ? audio.preparedSampleRate() : current.sampleRate;
    for (const auto& id : tracks)
        if (const auto* track = current.findTrack(id); track != nullptr && track->fx->processor != nullptr)
            track->fx->processor->prepare(rate, audio.maximumBlockSize());
}

void ArrangementController::reattachFx(const std::set<juce::Uuid>& tracks)
{
    const auto rate = audio.preparedSampleRate() > 0.0 ? audio.preparedSampleRate() : current.sampleRate;
    for (const auto& id : tracks)
    {
        detachedFx.erase(id);
        // Clear tails left by the offline render before the chain plays live again.
        if (const auto* track = current.findTrack(id); track != nullptr && track->fx->processor != nullptr)
            track->fx->processor->prepare(rate, audio.maximumBlockSize());
    }
    publish();
}

void ArrangementController::startFreeze(const juce::Uuid& trackId, std::function<void(std::shared_ptr<const FrozenRender>)> done)
{
    const auto index = current.trackIndex(trackId);
    if (index < 0)
        return;
    const auto& track = current.tracks[static_cast<size_t>(index)];
    if (track.isReturn())
    {
        message(text("Return-Spuren lassen sich nicht einfrieren."));
        return;
    }
    if (freezing.count(trackId) > 0 || exporting || audio.isRecording())
    {
        message(text("Einfrieren ist gerade nicht möglich (Aufnahme, Export oder Einfrieren läuft)."));
        return;
    }
    if (track.clips.empty())
    {
        message(text("Die Spur ist leer – nichts einzufrieren."));
        return;
    }
    for (const auto& clip : track.clips)
        if (! clip.isPattern() && (clip.source == nullptr || isRendering(clip)))
        {
            message(text("Einfrieren noch nicht möglich: Audio wird noch geladen oder per Rubber Band berechnet."));
            return;
        }

    const auto signature = freezeSignature(track, current);
    const auto chain = track.fx->processor != nullptr && track.fx->enabled.load() && ! track.fx->printWhileRecording.load();
    // Room for reverb/delay tails and ringing drum hits after the last clip.
    const auto tail = static_cast<juce::int64>(current.sampleRate * (chain ? 4.0 : track.drums ? 2.0 : 0.0));
    const auto end = track.contentEnd() + tail;
    const auto rate = current.sampleRate;
    const auto file = uniqueFile(audioDirectory().getChildFile("Freeze"), track.name + " (eingefroren)", ".wav");
    std::vector<std::shared_ptr<TrackInstrument>> copies(current.tracks.size());
    if (track.drums && track.instrument->instrument != nullptr)
        if ((copies[static_cast<size_t>(index)] = track.instrument->instrument->createOfflineCopy()) != nullptr)
            copies[static_cast<size_t>(index)]->prepare(rate, offlineBlock);

    freezing.insert(trackId);
    std::set<juce::Uuid> detached;
    if (chain)
        detached.insert(trackId);
    detachFx(detached);
    auto snapshot = ArrangementEngine::buildSnapshot(current);
    ArrangementEngine::OfflineRender options;
    options.onlyTrack = index;
    options.preFader = true;
    options.useProcessors = true;
    options.processorBlock = audio.maximumBlockSize();
    message(text("Friere \"") + track.name + text("\" ein …"));
    if (onChanged) onChanged();

    ++loadsInFlight;
    std::weak_ptr<bool> weakAlive = alive;
    juce::Thread::launch([this, snapshot, options, copies, file, rate, end, signature, trackId, detached, done, weakAlive]
    {
        std::vector<TrackInstrument*> instruments;
        for (const auto& copy : copies)
            instruments.push_back(copy.get());
        auto withInstruments = options;
        withInstruments.instruments = &instruments;
        auto result = juce::Result::ok();
        file.getParentDirectory().createDirectory();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
            const auto format = juce::AudioFormatWriterOptions {}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(32)
                                    .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
            auto writer = stream != nullptr ? wav.createWriterFor(stream, format) : nullptr;
            if (writer == nullptr)
                result = juce::Result::fail("Datei kann nicht angelegt werden: " + file.getFullPathName());
            else if (! renderRange(*snapshot, withInstruments, 0, end, 1.0f,
                                   [&writer](const float* const* channels, int count) { return writer->writeFromFloatArrays(channels, 2, count); }))
                result = juce::Result::fail("Schreibfehler beim Einfrieren: " + file.getFullPathName());
        }
        juce::MessageManager::callAsync([this, result, file, signature, trackId, detached, done, weakAlive]
        {
            if (weakAlive.expired())
                return;
            --loadsInFlight;
            reattachFx(detached);
            if (result.failed())
            {
                freezing.erase(trackId);
                file.deleteFile();
                message(result.getErrorMessage());
                if (onChanged) onChanged();
                return;
            }
            loadAsync(file, [this, file, signature, trackId, done](AudioDataPtr data, juce::String error)
            {
                freezing.erase(trackId);
                if (data == nullptr)
                {
                    message(error);
                    if (onChanged) onChanged();
                    return;
                }
                loadedFiles[file.getFullPathName()] = data;
                auto render = std::make_shared<FrozenRender>();
                render->file = file;
                render->data = data;
                render->signature = signature;
                done(render);
            });
        });
    });
}

void ArrangementController::freezeTrack(const juce::Uuid& trackId)
{
    if (isTrackFrozen(trackId))
        return;
    startFreeze(trackId, [this, trackId](std::shared_ptr<const FrozenRender> render)
    {
        perform("Spur einfrieren", [&](ArrangementState& state)
        {
            if (auto* track = state.findTrack(trackId))
                track->frozen = render;
        });
        if (const auto* track = current.findTrack(trackId))
            message(text(track->isFrozen(current) ? "Eingefroren: \"" : "Eingefroren, aber inzwischen bearbeitet: \"") + track->name + "\"");
    });
}

void ArrangementController::unfreezeTrack(const juce::Uuid& trackId)
{
    const auto* track = current.findTrack(trackId);
    if (track == nullptr || track->frozen == nullptr)
        return;
    perform("Spur auftauen", [&](ArrangementState& state)
    {
        if (auto* t = state.findTrack(trackId))
            t->frozen.reset();
    });
    frozenBefore.erase(trackId);
}

void ArrangementController::flattenTrack(const juce::Uuid& trackId)
{
    const auto flatten = [this, trackId](std::shared_ptr<const FrozenRender> render)
    {
        if (render == nullptr || render->data == nullptr)
            return;
        perform("Spur in Audio umwandeln", [&](ArrangementState& state)
        {
            auto* track = state.findTrack(trackId);
            if (track == nullptr)
                return;
            auto clip = edit::makeClip(render->file, render->data, 0);
            clip.name = track->name;
            track->clips.clear();
            track->clips.push_back(std::move(clip));
            track->drums = false;
            track->patterns.clear();
            track->frozen.reset();
            // The chain is now part of the audio: keep its settings, switched off.
            auto fx = std::make_shared<TrackFx>();
            fx->settings = track->fx->settings.createCopy();
            track->fx = std::move(fx);
        });
        instantiateFx();
        if (const auto* track = current.findTrack(trackId))
            message(text("In Audio umgewandelt: \"") + track->name + text("\" (Effekte sind eingerechnet und ausgeschaltet)."));
    };
    if (const auto* track = current.findTrack(trackId); track != nullptr && track->isFrozen(current))
        flatten(track->frozen);
    else
        startFreeze(trackId, flatten);
}

void ArrangementController::setInstrumentFactory(InstrumentFactory factory)
{
    instrumentFactory = std::move(factory);
    ensureInstruments();
    publish();
}

std::shared_ptr<TrackInstrument> ArrangementController::createInstrument(const juce::ValueTree& settings, int blockSize)
{
    std::shared_ptr<TrackInstrument> instrument;
    if (instrumentFactory)
        instrument = instrumentFactory(settings);
    if (instrument == nullptr)
        instrument = std::make_shared<DrumSynth>();
    const auto rate = audio.preparedSampleRate() > 0.0 ? audio.preparedSampleRate() : current.sampleRate;
    instrument->prepare(rate, blockSize); // not yet visible to the audio thread
    return instrument;
}

void ArrangementController::ensureInstruments()
{
    for (auto& track : current.tracks)
        if (track.drums && track.instrument->instrument == nullptr)
            track.instrument->instrument = createInstrument(track.instrument->settings, audio.maximumBlockSize());
}

juce::Uuid ArrangementController::addDrumTrack(const juce::String& name)
{
    juce::Uuid id;
    perform("Drum-Spur erstellen", [&](ArrangementState& state)
    {
        auto track = edit::makeDrumTrack(state, name.isNotEmpty() ? name : "Drums " + juce::String(state.firstReturnIndex() + 1));
        id = track.id;
        edit::addTrack(state, std::move(track));
    });
    return id;
}

juce::Result ArrangementController::setDrumInstrument(const juce::Uuid& trackId, const juce::ValueTree& settings)
{
    auto* track = current.findTrack(trackId);
    if (track == nullptr || ! track->drums)
        return juce::Result::fail("Keine Drum-Spur.");
    auto instrument = instrumentFactory ? instrumentFactory(settings) : nullptr;
    if (instrument == nullptr && settings.getProperty("type").toString() != "builtin")
        return juce::Result::fail("Instrument konnte nicht geladen werden.");
    if (instrument == nullptr)
        instrument = std::make_shared<DrumSynth>();
    const auto rate = audio.preparedSampleRate() > 0.0 ? audio.preparedSampleRate() : current.sampleRate;
    instrument->prepare(rate, audio.maximumBlockSize());
    // The old instrument stays alive in retired snapshots until the audio thread let go.
    track->instrument->instrument = std::move(instrument);
    track->instrument->settings = settings.createCopy();
    dirty = true;
    publish();
    if (onChanged)
        onChanged();
    return juce::Result::ok();
}

juce::Uuid ArrangementController::insertPatternClip(const juce::Uuid& trackId, int patternIndex, juce::int64 start, int bars)
{
    juce::Uuid id;
    perform(text("Pattern einfügen"), [&](ArrangementState& state)
    {
        const auto index = state.trackIndex(trackId);
        if (index < 0 || ! state.tracks[static_cast<size_t>(index)].drums)
            return;
        const auto& patterns = state.tracks[static_cast<size_t>(index)].patterns;
        const auto name = juce::isPositiveAndBelow(patternIndex, static_cast<int>(patterns.size())) ? patterns[static_cast<size_t>(patternIndex)].name : juce::String("Pattern");
        const auto length = static_cast<juce::int64>(std::llround(TimeGrid::of(state).samplesPerBar() * std::max(1, bars)));
        id = edit::insert(state, edit::makePatternClip(patternIndex, name, start, length), index);
    });
    return id;
}

void ArrangementController::previewDrum(const juce::Uuid& trackId, int note, int velocity)
{
    if (const auto* track = current.findTrack(trackId); track != nullptr && track->instrument->instrument != nullptr)
        track->instrument->instrument->preview(note, velocity);
}

void ArrangementController::setPatternAudition(const juce::Uuid& trackId, int patternIndex)
{
    audio.setPatternAudition(trackId.isNull() ? -1 : current.trackIndex(trackId), patternIndex);
}

std::vector<ArrangementController::InputChoice> ArrangementController::availableInputs() const
{
    std::vector<InputChoice> inputs { { -1, "NAM-Rig (bearbeitet, Stereo)" } };
    if (auto* device = devices.getCurrentAudioDevice())
    {
        const auto names = device->getInputChannelNames();
        const auto active = device->getActiveInputChannels();
        int callbackIndex = 0;
        for (int channel = 0; channel < names.size(); ++channel)
            if (active[channel])
                inputs.push_back({ callbackIndex++, "In " + juce::String(channel + 1) + ": " + names[channel] + " (dry)" });
    }
    return inputs;
}

void ArrangementController::importFiles(const juce::Array<juce::File>& files, int trackIndex, juce::int64 start)
{
    const auto separateTracks = files.size() > 1 || trackIndex < 0 || trackIndex >= static_cast<int>(current.tracks.size())
                             || current.tracks[static_cast<size_t>(trackIndex)].isReturn();
    const auto targetTrack = separateTracks ? juce::Uuid::null() : current.tracks[static_cast<size_t>(trackIndex)].id;
    const auto rate = current.sampleRate;
    const auto extractionDirectory = audioDirectory();
    for (const auto& file : files)
    {
        message(text("Importiere ") + file.getFileName() + " ...");
        ++loadsInFlight;
        std::weak_ptr<bool> weakAlive = alive;
        loader.addJob([this, file, rate, extractionDirectory, targetTrack, separateTracks, start, weakAlive]
        {
            juce::String error;
            auto actual = file;
            if (! codecs::canDecodeNatively(file))
            {
                // Compressed codecs and video containers: extract the audio
                // stream into the project so the project stays self-contained.
                actual = uniqueFile(extractionDirectory, file.getFileNameWithoutExtension() + (codecs::isVideoFile(file) ? " (Video-Audio)" : ""), ".wav");
                if (const auto result = codecs::extractWithFfmpeg(file, actual); result.failed())
                    error = result.getErrorMessage();
            }
            std::shared_ptr<const AudioData> data;
            if (error.isEmpty())
                data = loadAudioFile(actual, rate, error);
            juce::MessageManager::callAsync([this, file, actual, data, error, targetTrack, separateTracks, start, weakAlive]
            {
                if (weakAlive.expired())
                    return;
                --loadsInFlight;
                if (data == nullptr)
                {
                    message(error);
                    if (onChanged) onChanged();
                    return;
                }
                loadedFiles[actual.getFullPathName()] = data;
                perform("Audio importieren", [&](ArrangementState& state)
                {
                    auto index = state.trackIndex(targetTrack);
                    if (separateTracks || index < 0 || state.tracks[static_cast<size_t>(index)].isReturn())
                        index = edit::addTrack(state, edit::makeTrack(state, file.getFileNameWithoutExtension()));
                    auto clip = edit::makeClip(actual, data, start);
                    clip.name = file.getFileNameWithoutExtension();
                    edit::insert(state, std::move(clip), index);
                });
                message(text("Importiert: ") + file.getFileName());
            });
        });
    }
}

//==============================================================================
void ArrangementController::togglePlay()
{
    if (audio.isPlaying())
        stop();
    else
        audio.play();
    if (onChanged) onChanged();
}

void ArrangementController::stop()
{
    if (audio.isRecording())
        finishRecording();
    if (! audio.isPlaying())
        audio.setPosition(0); // second stop returns to the start, as in Ableton
    audio.stop();
    if (onChanged) onChanged();
}

void ArrangementController::locate(juce::int64 position)
{
    audio.setPosition(std::max<juce::int64>(0, position));
    if (onChanged) onChanged();
}

void ArrangementController::setLoop(bool enabled, juce::int64 start, juce::int64 end)
{
    modify([&](ArrangementState& state)
    {
        state.loopEnabled = enabled;
        state.loopStart = std::max<juce::int64>(0, std::min(start, end));
        state.loopEnd = std::max(start, end);
    });
}

void ArrangementController::setReampSource(const juce::Uuid& trackId)
{
    modify([&trackId](ArrangementState& state) { state.reampSource = trackId; });
}

void ArrangementController::setTempo(double bpm, int beatsPerBar, int beatUnit)
{
    perform("Tempo", [&](ArrangementState& state)
    {
        state.bpm = juce::jlimit(20.0, 300.0, bpm);
        state.beatsPerBar = juce::jlimit(1, 16, beatsPerBar);
        state.beatUnit = juce::jlimit(1, 16, beatUnit);
    });
}

void ArrangementController::toggleRecord(int fallbackTrack)
{
    if (audio.isRecording())
    {
        finishRecording();
        if (onChanged) onChanged();
        return;
    }
    if (current.sampleRate <= 0.0 || devices.getCurrentAudioDevice() == nullptr)
    {
        message(text("Kein Audiogerät geöffnet – Aufnahme nicht möglich."));
        return;
    }

    // Drum and return tracks are not recorded, frozen tracks must be thawed first.
    const auto recordable = [this](const Track& t) { return ! t.drums && ! t.isReturn() && ! t.isFrozen(current); };
    if (std::none_of(current.tracks.begin(), current.tracks.end(), [&](const Track& t) { return t.armed && recordable(t); }))
    {
        // Fall back to the selected or first audio track.
        auto index = juce::jlimit(0, std::max(0, static_cast<int>(current.tracks.size()) - 1), fallbackTrack);
        if (current.tracks.empty() || ! recordable(current.tracks[static_cast<size_t>(index)]))
        {
            if (std::none_of(current.tracks.begin(), current.tracks.end(), recordable))
                addTrack("Gitarre", -1);
            index = static_cast<int>(std::find_if(current.tracks.begin(), current.tracks.end(), recordable) - current.tracks.begin());
        }
        modify([index](ArrangementState& state) { state.tracks[static_cast<size_t>(index)].armed = true; });
    }

    std::vector<ArrangementEngine::RecordTarget> targets;
    takeLatency.clear();
    auto* device = devices.getCurrentAudioDevice();
    const auto deviceLatency = device->getInputLatencyInSamples() + device->getOutputLatencyInSamples();
    const auto manual = static_cast<int>(std::lround(manualLatency() * current.sampleRate / 1000.0));
    const auto stamp = juce::Time::getCurrentTime().formatted("%H-%M-%S");
    // While re-amping, the rig hears a track from inside the engine: its
    // output only lags by the rig's own latency, not the interface's.
    const auto reamping = current.reampTrack() != nullptr;
    for (const auto& track : current.tracks)
        if (track.armed && recordable(track) && targets.size() < static_cast<size_t>(ArrangementEngine::maximumRecordTracks))
        {
            const auto print = track.inputChannel >= 0 && track.fx->enabled && track.fx->printWhileRecording && track.fx->processor != nullptr;
            targets.push_back({ track.id, track.inputChannel, uniqueFile(audioDirectory(), track.name + " " + stamp, ".wav"),
                                track.inputChannel < 0 || print ? 2 : 1 });
            takeLatency[track.id] = (track.inputChannel < 0 && reamping ? 0 : deviceLatency + manual)
                + (track.inputChannel < 0 && rigLatency ? rigLatency() : 0)
                + (print ? track.fx->processor->latencySamples() : 0);
        }

    // With the loop on, every pass becomes a take.
    const auto loopValid = current.loopEnabled && current.loopEnd > current.loopStart;
    recordLoopStart = loopValid ? current.loopStart : 0;
    recordLoopEnd = loopValid ? current.loopEnd : 0;
    auto punch = ArrangementEngine::punchAtNextBlock;
    if (! audio.isPlaying())
    {
        punch = audio.position();
        if (countIn)
            audio.setPosition(punch - static_cast<juce::int64>(std::llround(grid().samplesPerBar())));
    }
    if (const auto result = audio.startRecording(targets, current.sampleRate, punch); result.failed())
    {
        message(result.getErrorMessage());
        return;
    }
    audio.play();
    message(juce::String(static_cast<int>(targets.size())) + text(targets.size() == 1 ? " Spur nimmt auf" : " Spuren nehmen auf"));
    if (onChanged) onChanged();
}

void ArrangementController::selectTake(const juce::Uuid& clipId, int index)
{
    const auto* clip = current.findClip(clipId);
    if (clip == nullptr || ! clip->hasTakes() || index == clip->activeTake)
        return;
    perform(text("Take wählen"), [&](ArrangementState& state) { edit::selectTake(state, clipId, index); });
}

void ArrangementController::stepTakes(const std::vector<juce::Uuid>& clipIds, int direction)
{
    std::vector<std::pair<juce::Uuid, int>> changes;
    for (const auto& id : clipIds)
        if (const auto* clip = current.findClip(id); clip != nullptr && clip->hasTakes())
        {
            const auto count = static_cast<int>(clip->takes.size());
            changes.emplace_back(id, (clip->activeTake + direction + count) % count);
        }
    if (changes.empty())
    {
        message(text("Kein Clip mit Takes ausgewählt (Takes entstehen bei Aufnahme mit Loop)."));
        return;
    }
    perform(text("Take wählen"), [&](ArrangementState& state)
    {
        for (const auto& [id, index] : changes)
            edit::selectTake(state, id, index);
    });
    if (const auto* clip = current.findClip(changes.front().first))
        message(text("Take ") + juce::String(clip->activeTake + 1) + text(" von ") + juce::String(static_cast<int>(clip->takes.size())));
}

void ArrangementController::dropOtherTakes(const juce::Uuid& clipId)
{
    perform(text("Andere Takes verwerfen"), [&](ArrangementState& state) { edit::dropOtherTakes(state, clipId); });
}

void ArrangementController::finishRecording()
{
    const auto takes = audio.stopRecording();
    if (takes.empty())
    {
        message("Aufnahme verworfen (nichts aufgenommen).");
        return;
    }
    auto remaining = std::make_shared<int>(static_cast<int>(takes.size()));
    auto loaded = std::make_shared<std::vector<std::pair<ArrangementEngine::Take, AudioDataPtr>>>();
    for (const auto& take : takes)
    {
        if (take.overflowed)
            message(text("Warnung: Festplatte zu langsam, Aufnahme enthält Lücken."));
        loadAsync(take.target.file, [this, take, remaining, loaded](AudioDataPtr data, juce::String error)
        {
            if (data != nullptr)
                loaded->emplace_back(take, data);
            else
                message(error);
            if (--*remaining > 0 || loaded->empty())
                return;
            int takeCount = 0;
            perform("Aufnahme", [&](ArrangementState& state)
            {
                for (const auto& [t, audioData] : *loaded)
                {
                    const auto index = state.trackIndex(t.target.trackId);
                    if (index < 0)
                        continue;
                    // Shift by the round-trip latency: what was played at
                    // transport position p arrives latency samples later.
                    for (auto& clip : edit::clipsFromLoopRecording(t.target.file, audioData, audioData->length(), t.startPosition,
                                                                    recordLoopStart, recordLoopEnd, takeLatency[t.target.trackId],
                                                                    state.tracks[static_cast<size_t>(index)].name + " Take"))
                    {
                        takeCount = std::max(takeCount, static_cast<int>(clip.takes.size()));
                        edit::insert(state, std::move(clip), index);
                    }
                }
            });
            if (takeCount > 1)
                message(juce::String(takeCount) + text(" Takes aufgenommen – der letzte spielt. Klick auf „Take“ im Clip wählt einen anderen."));
            else
                message("Aufnahme gespeichert: " + juce::String(static_cast<int>(loaded->size())) + " Spur(en)");
        });
    }
}

//==============================================================================
juce::File ArrangementController::defaultProjectFile()
{
    const auto name = "Projekt " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S");
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Ton of Able").getChildFile("Projekte").getChildFile(name).getChildFile(name + ".tonproj");
}

void ArrangementController::newProject()
{
    if (audio.isRecording())
        finishRecording();
    audio.stop();
    audio.setPosition(0);
    const auto rate = audio.preparedSampleRate();
    current = ArrangementState {};
    current.sampleRate = rate > 0.0 ? rate : 48000.0;
    edit::addTrack(current, edit::makeTrack(current, "Gitarre (Rig)"));
    auto dry = edit::makeTrack(current, "DI / Mikro");
    dry.inputChannel = 0;
    edit::addTrack(current, std::move(dry));
    undoStack.clear(); undoNames.clear(); redoStack.clear(); redoNames.clear();
    gestureOrigin.reset();
    project = defaultProjectFile();
    afterChange(true);
    dirty = false;
}

juce::Result ArrangementController::openProject(const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return juce::Result::fail("Projekt konnte nicht gelesen werden: " + file.getFullPathName());
    const auto tree = juce::ValueTree::fromXml(*xml);
    if (! tree.hasType("TonOfAbleProject"))
        return juce::Result::fail("Keine Ton-of-Able-Projektdatei: " + file.getFileName());
    if (audio.isRecording())
        finishRecording();
    audio.stop();
    audio.setPosition(0);
    current = fromValueTree(tree, file.getParentDirectory());
    audio.setMasterGainDb(static_cast<float>(tree.getProperty("masterDb", 0.0f)));
    audio.setMetronome(tree.getProperty("metronome", false));
    countIn = tree.getProperty("countIn", true);
    latencyOffsetMs = tree.getProperty("latencyOffsetMs", 0.0);
    undoStack.clear(); undoNames.clear(); redoStack.clear(); redoNames.clear();
    gestureOrigin.reset();
    project = file;
    if (const auto rate = audio.preparedSampleRate(); rate > 0.0)
        changeSampleRate(current, rate);
    for (auto& track : current.tracks)
        if (track.fx->settings.isValid())
            track.fx->processor = createProcessor(track.fx->settings);
    ensureInstruments();
    afterChange(true);
    dirty = false;
    return juce::Result::ok();
}

juce::Result ArrangementController::save()
{
    // Plugin instruments and chain plugins may have been edited in their own GUI.
    for (auto& track : current.tracks)
    {
        if (track.drums && track.instrument->instrument != nullptr)
            if (const auto settings = track.instrument->instrument->currentSettings(); settings.isValid())
                track.instrument->settings = settings;
        if (track.fx->processor != nullptr && track.fx->settings.isValid())
            if (const auto settings = track.fx->processor->currentSettings(track.fx->settings); settings.isValid())
                track.fx->settings = settings;
    }
    auto tree = toValueTree(current, project.getParentDirectory());
    tree.setProperty("masterDb", audio.masterGainDb(), nullptr);
    tree.setProperty("metronome", audio.isMetronomeOn(), nullptr);
    tree.setProperty("countIn", countIn, nullptr);
    tree.setProperty("latencyOffsetMs", latencyOffsetMs, nullptr);
    project.getParentDirectory().createDirectory();
    const auto xml = tree.createXml();
    if (xml == nullptr || ! xml->writeTo(project))
        return juce::Result::fail("Projekt konnte nicht gespeichert werden: " + project.getFullPathName());
    dirty = false;
    if (onChanged) onChanged();
    return juce::Result::ok();
}

juce::Result ArrangementController::saveAs(const juce::File& file)
{
    const auto target = file.withFileExtension(".tonproj");
    const auto oldAudio = audioDirectory();
    const auto newAudio = target.getParentDirectory().getChildFile("Audio");
    if (oldAudio != newAudio)
    {
        // Recordings and extracted audio belong to the project: copy them along.
        std::map<juce::String, juce::File> moved;
        for (const auto& track : current.tracks)
            for (const auto& clip : track.clips)
                if (clip.file.isAChildOf(oldAudio) && moved.count(clip.file.getFullPathName()) == 0)
                {
                    const auto copy = newAudio.getChildFile(clip.file.getRelativePathFrom(oldAudio));
                    copy.getParentDirectory().createDirectory();
                    if (! copy.existsAsFile() && ! clip.file.copyFileTo(copy))
                        return juce::Result::fail("Audiodatei konnte nicht kopiert werden: " + clip.file.getFileName());
                    moved[clip.file.getFullPathName()] = copy;
                }
        forEachState([&](ArrangementState& state)
        {
            for (auto& track : state.tracks)
                for (auto& clip : track.clips)
                    if (const auto found = moved.find(clip.file.getFullPathName()); found != moved.end())
                        clip.file = found->second;
        });
        for (const auto& [from, to] : moved)
            if (const auto data = loadedFiles[from].lock())
                loadedFiles[to.getFullPathName()] = data;
    }
    project = target;
    return save();
}

//==============================================================================
void ArrangementController::exportAudio(const juce::File& target, codecs::ExportFormat format, bool loopOnly, bool stems,
                                        std::function<void(juce::Result)> done, const juce::Uuid& singleTrack)
{
    for (const auto& track : current.tracks)
        for (const auto& clip : track.clips)
            if (! clip.isPattern() && (clip.source == nullptr || isRendering(clip)))
            {
                done(juce::Result::fail(text("Export noch nicht möglich: Audio wird noch geladen oder per Rubber Band berechnet.")));
                return;
            }

    const auto from = loopOnly && current.loopEnabled ? current.loopStart : 0;
    const auto to = loopOnly && current.loopEnabled ? current.loopEnd : current.endSample();
    if (to <= from)
    {
        done(juce::Result::fail("Nichts zu exportieren: das Arrangement ist leer."));
        return;
    }

    struct Job { juce::File file; int track; };
    std::vector<Job> jobs;
    const auto extension = juce::String(".") + codecs::infoFor(format).extension;
    if (! singleTrack.isNull())
    {
        const auto index = current.trackIndex(singleTrack);
        if (index < 0)
        {
            done(juce::Result::fail("Spur nicht gefunden."));
            return;
        }
        jobs.push_back({ target.withFileExtension(extension), index });
    }
    else if (stems)
    {
        target.createDirectory();
        for (size_t index = 0; index < current.tracks.size(); ++index)
            if (! current.tracks[index].clips.empty() || current.tracks[index].isReturn())
                jobs.push_back({ uniqueFile(target, juce::String(static_cast<int>(index) + 1).paddedLeft('0', 2) + " " + current.tracks[index].name, extension), static_cast<int>(index) });
    }
    else
        jobs.push_back({ target.withFileExtension(extension), -1 });

    if (exporting || ! freezing.empty())
    {
        done(juce::Result::fail(text("Export ist gerade nicht möglich: ein Export oder Einfrieren läuft noch.")));
        return;
    }

    // The effect chains render offline, so they leave live playback meanwhile.
    std::set<juce::Uuid> chains;
    for (const auto& track : current.tracks)
        if (track.fx->processor != nullptr && track.fx->enabled.load() && ! track.isFrozen(current))
            chains.insert(track.id);
    exporting = true;
    detachFx(chains);
    const auto snapshot = ArrangementEngine::buildSnapshot(current);
    const auto rate = current.sampleRate;
    // Drum tracks render through independent instrument copies (created here,
    // on the message thread), so the live instruments keep playing.
    auto offline = std::make_shared<std::vector<std::shared_ptr<TrackInstrument>>>();
    for (const auto& track : current.tracks)
    {
        std::shared_ptr<TrackInstrument> copy;
        if (track.drums && track.instrument->instrument != nullptr && ! track.isFrozen(current))
            if ((copy = track.instrument->instrument->createOfflineCopy()) != nullptr)
                copy->prepare(rate, offlineBlock);
        offline->push_back(std::move(copy));
    }
    const auto master = juce::Decibels::decibelsToGain(audio.masterGainDb(), -60.0f);
    const auto processorBlock = audio.maximumBlockSize();
    ++loadsInFlight;
    std::weak_ptr<bool> weakAlive = alive;
    juce::Thread::launch([this, snapshot, jobs, rate, master, from, to, format, done, weakAlive, offline, chains, processorBlock]
    {
        auto result = juce::Result::ok();
        for (size_t jobIndex = 0; jobIndex < jobs.size(); ++jobIndex)
        {
            const auto& job = jobs[jobIndex];
            // Fresh state for every file: re-prepare the instrument copies and the chains.
            std::vector<TrackInstrument*> instruments;
            for (const auto& copy : *offline)
            {
                if (copy != nullptr)
                    copy->prepare(rate, offlineBlock);
                instruments.push_back(copy.get());
            }
            if (jobIndex > 0 && ! chains.empty())
                onMessageThread([this, chains, weakAlive]
                {
                    if (weakAlive.expired())
                        return;
                    const auto sampleRate = audio.preparedSampleRate() > 0.0 ? audio.preparedSampleRate() : current.sampleRate;
                    for (const auto& id : chains)
                        if (const auto* track = current.findTrack(id); track != nullptr && track->fx->processor != nullptr)
                            track->fx->processor->prepare(sampleRate, audio.maximumBlockSize());
                });
            ArrangementEngine::OfflineRender options;
            options.onlyTrack = job.track;
            options.useProcessors = true;
            options.processorBlock = processorBlock;
            options.instruments = &instruments;
            codecs::ExportWriter writer(job.file, format, rate, 2);
            if (result = writer.openResult(); result.failed())
                break;
            if (! renderRange(*snapshot, options, from, to, job.track < 0 ? master : 1.0f,
                              [&writer](const float* const* channels, int count) { return writer.write(channels, count); }))
                result = juce::Result::fail("Schreibfehler beim Export: " + job.file.getFullPathName());
            if (result.wasOk())
                result = writer.finish();
            if (result.failed())
                break;
        }
        juce::MessageManager::callAsync([this, result, done, weakAlive, chains]
        {
            if (weakAlive.expired())
                return;
            --loadsInFlight;
            exporting = false;
            reattachFx(chains);
            done(result);
        });
    });
}

//==============================================================================
void ArrangementController::timerCallback()
{
    audio.releaseRetiredSnapshots();
    const auto rate = audio.preparedSampleRate();
    if (rate > 0.0 && std::abs(rate - current.sampleRate) > 1.0e-6 && ! audio.isRecording() && gestureOrigin == nullptr)
    {
        // The device changed its sample rate: keep musical positions, reload audio.
        const auto hadClips = current.endSample() > 0;
        changeSampleRate(current, rate);
        undoStack.clear(); undoNames.clear(); redoStack.clear(); redoNames.clear();
        loadedFiles.clear();
        afterChange(true);
        if (hadClips)
            message("Samplerate " + juce::String(rate / 1000.0, 1) + " kHz: Audio wird neu geladen.");
    }
}
} // namespace daw
