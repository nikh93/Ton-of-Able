#include "ArrangementEngine.h"

#include <algorithm>
#include <cmath>

namespace daw
{
namespace
{
constexpr float pi = 3.14159265358979f;

float fadeGain(const ArrangementEngine::ClipRender& clip, juce::int64 local) noexcept
{
    // Every edge gets at least a short declick ramp, so cuts never pop.
    const auto declick = std::min<juce::int64>(64, clip.length / 4);
    const auto in = std::max(clip.fadeIn, declick);
    const auto out = std::max(clip.fadeOut, declick);
    float gain = 1.0f;
    if (local < in && in > 0)
        gain *= std::sin(0.5f * pi * static_cast<float>(local + 1) / static_cast<float>(in + 1));
    if (const auto remaining = clip.length - local; remaining <= out && out > 0)
        gain *= std::sin(0.5f * pi * static_cast<float>(remaining) / static_cast<float>(out + 1));
    return gain;
}

// Adds [position, position + length) of a clip into left/right (indices relative to position).
void renderClip(const ArrangementEngine::ClipRender& clip, juce::int64 position, int length, float* left, float* right) noexcept
{
    const auto from = std::max(position, clip.start);
    const auto to = std::min(position + length, clip.start + clip.length);
    if (from >= to || clip.original == nullptr)
        return;

    const auto* source = clip.stretched != nullptr ? clip.stretched : clip.original;
    const auto* sourceLeft = source->samples.getReadPointer(0);
    const auto* sourceRight = source->samples.getReadPointer(source->numChannels() > 1 ? 1 : 0);
    const auto sourceLength = source->length();

    for (auto t = from; t < to; ++t)
    {
        const auto local = t - clip.start;
        const auto gain = clip.gain * fadeGain(clip, local);
        float l = 0.0f, r = 0.0f;
        if (clip.stretched != nullptr || std::abs(clip.ratio - 1.0) < 1.0e-9)
        {
            const auto base = clip.stretched != nullptr ? std::llround(clip.offset * clip.ratio) : std::llround(clip.offset);
            const auto index = base + local;
            if (index < 0 || index >= sourceLength)
                continue;
            l = sourceLeft[index];
            r = sourceRight[index];
        }
        else
        {
            // Rubber Band render still pending: varispeed preview.
            const auto read = clip.offset + static_cast<double>(local) / clip.ratio;
            const auto index = static_cast<juce::int64>(read);
            if (index < 0 || index + 1 >= sourceLength)
                continue;
            const auto fraction = static_cast<float>(read - static_cast<double>(index));
            l = sourceLeft[index] + fraction * (sourceLeft[index + 1] - sourceLeft[index]);
            r = sourceRight[index] + fraction * (sourceRight[index + 1] - sourceRight[index]);
        }
        left[t - position] += l * gain;
        right[t - position] += r * gain;
    }
}

void renderTrackClips(const ArrangementEngine::TrackRender& track, juce::int64 position, int length, float* left, float* right) noexcept
{
    const auto first = std::lower_bound(track.clips.begin(), track.clips.end(), position,
        [](const ArrangementEngine::ClipRender& clip, juce::int64 value) { return clip.start + clip.length <= value; });
    for (auto clip = first; clip != track.clips.end() && clip->start < position + length; ++clip)
        renderClip(*clip, position, length, left, right);
}

std::pair<float, float> balanceGains(const TrackMixer& mixer) noexcept
{
    if (mixer.muted.load(std::memory_order_relaxed))
        return { 0.0f, 0.0f };
    const auto gain = juce::Decibels::decibelsToGain(mixer.volumeDb.load(std::memory_order_relaxed), -60.0f);
    const auto pan = juce::jlimit(-1.0f, 1.0f, mixer.pan.load(std::memory_order_relaxed));
    return { gain * std::min(1.0f, 1.0f - pan), gain * std::min(1.0f, 1.0f + pan) };
}

int collectTrackEvents(const ArrangementEngine::TrackRender& track, double samplesPerStep, juce::int64 position, int length,
                       DrumEvent* out, int maxEvents) noexcept
{
    if (track.patterns == nullptr)
        return 0;
    int count = 0;
    for (const auto& clip : track.clips)
        if (clip.pattern >= 0 && clip.pattern < static_cast<int>(track.patterns->size())
            && clip.start < position + length && clip.start + clip.length > position)
            count += collectPatternEvents((*track.patterns)[static_cast<size_t>(clip.pattern)], samplesPerStep, clip.start, clip.start + clip.length,
                                          clip.offset, position, length, out + count, maxEvents - count);
    return count;
}

bool anySolo(const ArrangementEngine::Snapshot& snapshot) noexcept
{
    for (const auto& track : snapshot.tracks)
        if (track.mixer->soloed.load(std::memory_order_relaxed))
            return true;
    return false;
}
} // namespace

ArrangementEngine::ArrangementEngine()
{
    diskThread.startThread();
}

ArrangementEngine::~ArrangementEngine()
{
    stopRecording();
    diskThread.stopThread(2000);
}

std::shared_ptr<const ArrangementEngine::Snapshot> ArrangementEngine::buildSnapshot(const ArrangementState& state)
{
    auto snapshot = std::make_shared<Snapshot>();
    snapshot->tracks.reserve(state.tracks.size());
    for (const auto& track : state.tracks)
    {
        TrackRender render;
        render.id = track.id;
        render.mixer = track.mixer.get();
        render.fx = track.fx.get();
        render.processor = track.fx->processor.get();
        snapshot->retainedFx.push_back(track.fx);
        if (track.fx->processor != nullptr)
            snapshot->retainedProcessors.push_back(track.fx->processor);
        if (track.drums)
        {
            render.drums = true;
            auto patterns = std::make_shared<const std::vector<DrumPattern>>(track.patterns);
            render.patterns = patterns.get();
            snapshot->retainedPatterns.push_back(std::move(patterns));
            if (track.instrument->instrument != nullptr)
            {
                render.instrument = track.instrument->instrument.get();
                snapshot->retainedInstruments.push_back(track.instrument->instrument);
            }
        }
        render.armed = track.armed;
        render.monitorInput = track.monitorInput;
        render.inputChannel = track.inputChannel;
        snapshot->retainedMixers.push_back(track.mixer);
        for (const auto& clip : track.clips)
        {
            if (clip.isPattern() && clip.length > 0)
            {
                ClipRender c;
                c.start = clip.start;
                c.length = clip.length;
                c.offset = clip.offset;
                c.pattern = clip.pattern;
                render.clips.push_back(c);
                continue;
            }
            if (clip.source == nullptr || clip.length <= 0)
                continue;
            ClipRender c;
            c.start = clip.start;
            c.length = clip.length;
            c.fadeIn = clip.fadeIn;
            c.fadeOut = clip.fadeOut;
            c.offset = clip.offset;
            c.ratio = clip.stretchRatio;
            c.original = clip.source.get();
            c.stretched = clip.needsStretch() && clip.hasCurrentRender() ? clip.stretched.get() : nullptr;
            c.gain = juce::Decibels::decibelsToGain(clip.gainDb, -96.0f);
            snapshot->retainedAudio.push_back(clip.source);
            if (c.stretched != nullptr)
                snapshot->retainedAudio.push_back(clip.stretched);
            render.clips.push_back(c);
        }
        snapshot->tracks.push_back(std::move(render));
    }
    snapshot->samplesPerStep = state.sampleRate * 60.0 / state.bpm / 4.0;
    return snapshot;
}

void ArrangementEngine::publish(std::shared_ptr<const Snapshot> snapshot)
{
    if (owned != nullptr)
        retired.push_back(owned);
    owned = std::move(snapshot);
    live.store(owned.get());
    releaseRetiredSnapshots();
}

void ArrangementEngine::releaseRetiredSnapshots()
{
    // A callback that started after the store above sees the new snapshot;
    // with none in flight, no callback can still hold a retired pointer.
    if (! retired.empty() && callbacksInFlight.load() == 0)
        retired.clear();
}

void ArrangementEngine::setPosition(juce::int64 value) noexcept
{
    playhead.store(value);
    pendingLocate.store(value);
}

void ArrangementEngine::setLoop(bool enabled, juce::int64 start, juce::int64 end) noexcept
{
    loopStart.store(start);
    loopEnd.store(end);
    loopEnabled.store(enabled && end > start);
}

void ArrangementEngine::setPatternAudition(int trackIndex, int patternIndex) noexcept
{
    auditionPatternIndex.store(std::max(0, patternIndex));
    auditionTrackIndex.store(trackIndex);
    auditionGeneration.fetch_add(1);
    if (trackIndex < 0)
        auditionStepNow.store(-1);
}

void ArrangementEngine::setTempo(double newBpm, int newBeatsPerBar, int newBeatUnit) noexcept
{
    bpm.store(juce::jlimit(20.0, 300.0, newBpm));
    beatsPerBar.store(std::max(1, newBeatsPerBar));
    beatUnit.store(std::max(1, newBeatUnit));
}

juce::Result ArrangementEngine::startRecording(const std::vector<RecordTarget>& targets, double rate, juce::int64 punch)
{
    if (recording.load() || slotCount.load() != 0)
        return juce::Result::fail(juce::String(juce::CharPointer_UTF8("Aufnahme l\xc3\xa4uft bereits.")));
    if (targets.empty() || targets.size() > slots.size() || rate <= 0.0)
        return juce::Result::fail(juce::String(juce::CharPointer_UTF8("Keine g\xc3\xbcltige Aufnahmespur.")));

    juce::WavAudioFormat wav;
    for (size_t index = 0; index < targets.size(); ++index)
    {
        const auto& target = targets[index];
        target.file.getParentDirectory().createDirectory();
        std::unique_ptr<juce::OutputStream> stream(target.file.createOutputStream());
        const auto options = juce::AudioFormatWriterOptions {}
            .withSampleRate(rate)
            .withNumChannels(juce::jlimit(1, 2, target.channels))
            .withBitsPerSample(24);
        auto writer = stream != nullptr ? wav.createWriterFor(stream, options) : nullptr;
        if (writer == nullptr)
        {
            for (size_t created = 0; created < index; ++created)
                slots[created].owner.reset();
            return juce::Result::fail("Aufnahmedatei kann nicht angelegt werden: " + target.file.getFullPathName());
        }
        auto& slot = slots[index];
        slot.owner = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer.release(), diskThread, 1 << 17);
        slot.target = target;
        slot.written.store(0);
        slot.overflowed.store(false);
        slot.writer.store(slot.owner.get());
    }
    slotCount.store(static_cast<int>(targets.size()));
    punchIn.store(punch);
    recording.store(true);
    return juce::Result::ok();
}

std::vector<ArrangementEngine::Take> ArrangementEngine::stopRecording()
{
    const auto count = slotCount.load();
    recording.store(false);
    for (int index = 0; index < count; ++index)
        slots[static_cast<size_t>(index)].writer.store(nullptr);
    // Wait until no callback can still write into a writer we are about to close.
    for (int spin = 0; callbacksInFlight.load() != 0 && spin < 2000; ++spin)
        juce::Thread::sleep(1);

    std::vector<Take> takes;
    const auto start = punchIn.load();
    for (int index = 0; index < count; ++index)
    {
        auto& slot = slots[static_cast<size_t>(index)];
        slot.owner.reset(); // drains the FIFO and finalises the WAV header
        if (start != punchAtNextBlock && slot.written.load() > 0)
            takes.push_back({ slot.target, start, slot.written.load(), slot.overflowed.load() });
    }
    slotCount.store(0);
    return takes;
}

void ArrangementEngine::prepare(double rate, int maximumBlockSize)
{
    maximumBlock = std::max(1, maximumBlockSize);
    trackBuffer.setSize(2, maximumBlock, false, true, false);
    mixBuffer.setSize(2, maximumBlock, false, true, false);
    monoBuffer.setSize(1, maximumBlock, false, true, false);
    instrumentBuffer.setSize(2, maximumBlock, false, true, false);
    silence.assign(static_cast<size_t>(maximumBlock), 0.0f);
    // The device is stopped here, so the published chains can be re-prepared.
    if (owned != nullptr)
    {
        for (const auto& processor : owned->retainedProcessors)
            processor->prepare(rate, maximumBlock);
        for (const auto& instrument : owned->retainedInstruments)
            instrument->prepare(rate, maximumBlock);
    }
    currentMaster = juce::Decibels::decibelsToGain(masterDb.load(), -60.0f);
    sampleRate.store(rate);
}

void ArrangementEngine::process(const float* const* inputs, int numInputs, const float* rigLeft, const float* rigRight,
                                float* outLeft, float* outRight, int numSamples) noexcept
{
    callbacksInFlight.fetch_add(1);
    for (int offset = 0; offset < numSamples && maximumBlock > 0; offset += maximumBlock)
        processChunk(inputs, numInputs, rigLeft, rigRight, outLeft, outRight, offset, std::min(maximumBlock, numSamples - offset));
    callbacksInFlight.fetch_sub(1);
}

void ArrangementEngine::processChunk(const float* const* inputs, int numInputs, const float* rigLeft, const float* rigRight,
                                     float* outLeft, float* outRight, int offset, int numSamples) noexcept
{
    const auto* snapshot = live.load();
    auto position = playhead.load(std::memory_order_relaxed);
    if (const auto locate = pendingLocate.exchange(std::numeric_limits<juce::int64>::min()); locate != std::numeric_limits<juce::int64>::min())
        position = locate;

    auto* mixLeft = mixBuffer.getWritePointer(0);
    auto* mixRight = mixBuffer.getWritePointer(1);
    juce::FloatVectorOperations::clear(mixLeft, numSamples);
    juce::FloatVectorOperations::clear(mixRight, numSamples);

    const auto isPlaying = playing.load();
    const auto isRecording = recording.load() && isPlaying;
    if (isRecording && punchIn.load() == punchAtNextBlock)
        punchIn.store(position);

    if (snapshot != nullptr && ! isPlaying)
        renderTracks(*snapshot, inputs, numInputs, rigLeft, rigRight, position, offset, 0, numSamples, false, false, mixLeft, mixRight);

    if (isPlaying)
    {
        const auto looping = loopEnabled.load() && ! isRecording;
        const auto loopFrom = loopStart.load();
        const auto loopTo = loopEnd.load();
        for (int done = 0; done < numSamples;)
        {
            auto length = numSamples - done;
            const auto wraps = looping && position < loopTo && loopTo > loopFrom;
            if (wraps)
                length = static_cast<int>(std::min<juce::int64>(length, loopTo - position));
            if (snapshot != nullptr)
                renderTracks(*snapshot, inputs, numInputs, rigLeft, rigRight, position, offset + done, done, length, true, isRecording, mixLeft, mixRight);
            const auto countIn = isRecording && position < punchIn.load();
            if (metronome.load() || countIn)
                renderClick(position, mixLeft + done, mixRight + done, length);
            position += length;
            done += length;
            if (wraps && position >= loopTo)
                position = loopFrom;
        }
    }

    const auto target = juce::Decibels::decibelsToGain(masterDb.load(), -60.0f);
    float peak = 0.0f;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto gain = currentMaster + (target - currentMaster) * static_cast<float>(sample + 1) / static_cast<float>(numSamples);
        const auto l = mixLeft[sample] * gain;
        const auto r = mixRight[sample] * gain;
        outLeft[offset + sample] += l;
        outRight[offset + sample] += r;
        peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
    }
    currentMaster = target;
    if (peak > masterMeter.load(std::memory_order_relaxed))
        masterMeter.store(peak, std::memory_order_relaxed);
    playhead.store(position, std::memory_order_relaxed);
}

void ArrangementEngine::renderTracks(const Snapshot& snapshot, const float* const* inputs, int numInputs, const float* rigLeft, const float* rigRight,
                                     juce::int64 position, int inputOffset, int mixOffset, int length, bool playingNow, bool recordingNow,
                                     float* mixLeft, float* mixRight) noexcept
{
    const auto solo = anySolo(snapshot);
    auto* left = trackBuffer.getWritePointer(0);
    auto* right = trackBuffer.getWritePointer(1);
    auto* mono = monoBuffer.getWritePointer(0);
    const auto audition = auditionTrackIndex.load();
    if (const auto generation = auditionGeneration.load(); generation != auditionSeenGeneration)
    {
        auditionSeenGeneration = generation;
        auditionPosition = 0;
    }
    for (size_t trackIndex = 0; trackIndex < snapshot.tracks.size(); ++trackIndex)
    {
        const auto& track = snapshot.tracks[trackIndex];
        auto& mixer = *track.mixer;
        const auto fxOn = track.processor != nullptr && track.fx->enabled.load(std::memory_order_relaxed);
        const auto print = fxOn && track.fx->printWhileRecording.load(std::memory_order_relaxed);
        const auto* input = track.inputChannel >= 0 && track.inputChannel < numInputs && inputs[track.inputChannel] != nullptr
                          ? inputs[track.inputChannel] + inputOffset : nullptr;
        // Armed dry-input tracks listen to their input while recording, and
        // while stopped when monitoring is on; otherwise they play their clips.
        const auto takesInput = track.armed && input != nullptr && (recordingNow || (! playingNow && track.monitorInput));

        juce::FloatVectorOperations::clear(left, length);
        juce::FloatVectorOperations::clear(right, length);
        auto audible = true;
        if (takesInput)
        {
            if (fxOn)
            {
                juce::FloatVectorOperations::copy(mono, input, length);
                track.processor->process(mono, left, right, length);
            }
            else
            {
                juce::FloatVectorOperations::copy(left, input, length);
                juce::FloatVectorOperations::copy(right, input, length);
            }
            if (recordingNow)
                recordTrack(track.id, print ? left : input, print ? right : input, position, length);
            audible = track.monitorInput;
        }
        else
        {
            if (playingNow && ! (recordingNow && track.armed))
            {
                renderTrackClips(track, position, length, left, right);
                if (fxOn && ! print)
                {
                    // Insert mode: the chain is applied to the recorded audio on playback.
                    for (int sample = 0; sample < length; ++sample)
                        mono[sample] = 0.5f * (left[sample] + right[sample]);
                    track.processor->process(mono, left, right, length);
                }
            }
            else
                audible = false;
            if (track.drums && track.instrument != nullptr)
            {
                // The instrument always runs, so hits ring out and previews sound.
                int count = 0;
                if (playingNow && ! (recordingNow && track.armed))
                    count = collectTrackEvents(track, snapshot.samplesPerStep, position, length, events.data(), maximumEvents);
                else if (! playingNow && audition == static_cast<int>(trackIndex) && track.patterns != nullptr && ! track.patterns->empty())
                {
                    const auto& pattern = (*track.patterns)[static_cast<size_t>(std::min(auditionPatternIndex.load(), static_cast<int>(track.patterns->size()) - 1))];
                    count = collectPatternEvents(pattern, snapshot.samplesPerStep, 0, std::numeric_limits<juce::int64>::max() / 2, 0.0,
                                                 auditionPosition, length, events.data(), maximumEvents);
                    auditionPosition += length;
                    const auto step = static_cast<int>(static_cast<double>(auditionPosition) / snapshot.samplesPerStep) % std::max(1, pattern.steps);
                    auditionStepNow.store(step);
                }
                sortEvents(events.data(), count);
                auto* instrumentLeft = instrumentBuffer.getWritePointer(0);
                auto* instrumentRight = instrumentBuffer.getWritePointer(1);
                track.instrument->render(events.data(), count, instrumentLeft, instrumentRight, length);
                juce::FloatVectorOperations::add(left, instrumentLeft, length);
                juce::FloatVectorOperations::add(right, instrumentRight, length);
                audible = true;
            }
            if (recordingNow && track.armed && track.inputChannel < 0)
                recordTrack(track.id, rigLeft != nullptr ? rigLeft + inputOffset : silence.data(),
                            rigRight != nullptr ? rigRight + inputOffset : silence.data(), position, length);
        }

        auto [targetLeft, targetRight] = balanceGains(mixer);
        if ((solo && ! mixer.soloed.load(std::memory_order_relaxed)) || ! audible)
            targetLeft = targetRight = 0.0f;
        if (! mixer.gainInitialised)
        {
            mixer.currentLeft = targetLeft;
            mixer.currentRight = targetRight;
            mixer.gainInitialised = true;
        }
        float peak = 0.0f;
        for (int sample = 0; sample < length; ++sample)
        {
            const auto amount = static_cast<float>(sample + 1) / static_cast<float>(length);
            const auto l = left[sample] * (mixer.currentLeft + (targetLeft - mixer.currentLeft) * amount);
            const auto r = right[sample] * (mixer.currentRight + (targetRight - mixer.currentRight) * amount);
            mixLeft[mixOffset + sample] += l;
            mixRight[mixOffset + sample] += r;
            peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
        }
        mixer.currentLeft = targetLeft;
        mixer.currentRight = targetRight;
        if (peak > mixer.meter.load(std::memory_order_relaxed))
            mixer.meter.store(peak, std::memory_order_relaxed);
    }
}

void ArrangementEngine::recordTrack(const juce::Uuid& trackId, const float* left, const float* right, juce::int64 position, int length) noexcept
{
    const auto punch = punchIn.load();
    if (position + length <= punch)
        return; // still counting in
    const auto skip = static_cast<int>(std::max<juce::int64>(0, punch - position));
    for (int index = 0; index < slotCount.load(); ++index)
    {
        auto& slot = slots[static_cast<size_t>(index)];
        auto* writer = slot.writer.load();
        if (writer == nullptr || slot.target.trackId != trackId)
            continue;
        const float* channels[2] { left + skip, right + skip };
        if (! writer->write(channels, length - skip))
            slot.overflowed.store(true);
        slot.written.fetch_add(length - skip);
    }
}

void ArrangementEngine::renderClick(juce::int64 position, float* outLeft, float* outRight, int length) const noexcept
{
    const auto rate = sampleRate.load();
    if (rate <= 0.0)
        return;
    const auto beat = rate * 60.0 / bpm.load() * 4.0 / beatUnit.load();
    const auto clickLength = 0.035 * rate;
    for (int sample = 0; sample < length; ++sample)
    {
        const auto at = static_cast<double>(position + sample);
        const auto index = std::floor(at / beat);
        const auto phase = at - index * beat;
        if (phase >= clickLength)
            continue;
        const auto bar = static_cast<juce::int64>(index) % beatsPerBar.load();
        const auto downbeat = bar == 0;
        const auto frequency = downbeat ? 1760.0 : 1100.0;
        const auto value = static_cast<float>((downbeat ? 0.45 : 0.3) * std::exp(-phase / (0.007 * rate))
            * std::sin(2.0 * 3.141592653589793 * frequency * phase / rate));
        outLeft[sample] += value;
        outRight[sample] += value;
    }
}

void ArrangementEngine::renderOffline(const Snapshot& snapshot, juce::int64 start, int numSamples, float* left, float* right,
                                      juce::AudioBuffer<float>& scratch, int onlyTrack,
                                      const std::vector<TrackInstrument*>* instruments)
{
    juce::FloatVectorOperations::clear(left, numSamples);
    juce::FloatVectorOperations::clear(right, numSamples);
    scratch.setSize(4, numSamples, false, false, true);
    std::vector<DrumEvent> offlineEvents(static_cast<size_t>(maximumEvents) * 4);
    const auto solo = anySolo(snapshot);
    for (size_t index = 0; index < snapshot.tracks.size(); ++index)
    {
        const auto& track = snapshot.tracks[index];
        auto [gainLeft, gainRight] = balanceGains(*track.mixer);
        if (onlyTrack >= 0)
        {
            if (static_cast<int>(index) != onlyTrack)
                continue;
            const auto gain = juce::Decibels::decibelsToGain(track.mixer->volumeDb.load(), -60.0f);
            const auto pan = juce::jlimit(-1.0f, 1.0f, track.mixer->pan.load());
            gainLeft = gain * std::min(1.0f, 1.0f - pan);
            gainRight = gain * std::min(1.0f, 1.0f + pan);
        }
        else if (solo && ! track.mixer->soloed.load())
            continue;
        if (gainLeft == 0.0f && gainRight == 0.0f)
            continue;
        scratch.clear();
        renderTrackClips(track, start, numSamples, scratch.getWritePointer(0), scratch.getWritePointer(1));
        if (auto* instrument = instruments != nullptr && index < instruments->size() ? (*instruments)[index] : nullptr; instrument != nullptr && track.drums)
        {
            // Offline copies are rendered strictly in order, block after block.
            const auto count = collectTrackEvents(track, snapshot.samplesPerStep, start, numSamples, offlineEvents.data(), static_cast<int>(offlineEvents.size()));
            sortEvents(offlineEvents.data(), count);
            instrument->render(offlineEvents.data(), count, scratch.getWritePointer(2), scratch.getWritePointer(3), numSamples);
            juce::FloatVectorOperations::add(scratch.getWritePointer(0), scratch.getReadPointer(2), numSamples);
            juce::FloatVectorOperations::add(scratch.getWritePointer(1), scratch.getReadPointer(3), numSamples);
        }
        juce::FloatVectorOperations::addWithMultiply(left, scratch.getReadPointer(0), gainLeft, numSamples);
        juce::FloatVectorOperations::addWithMultiply(right, scratch.getReadPointer(1), gainRight, numSamples);
    }
}
} // namespace daw
