#include "Arrangement.h"
#include "SampleKit.h"

#include <algorithm>
#include <cmath>

namespace daw
{
namespace
{
constexpr double epsilon = 1.0e-9;

void clampFades(Clip& clip)
{
    clip.fadeIn = juce::jlimit<juce::int64>(0, clip.length, clip.fadeIn);
    clip.fadeOut = juce::jlimit<juce::int64>(0, clip.length - clip.fadeIn, clip.fadeOut);
}

// Moves the left edge to position without moving the audio on the timeline.
void cutFront(Clip& clip, juce::int64 position)
{
    const auto delta = position - clip.start;
    clip.offset = clip.sourcePositionAt(position);
    clip.start = position;
    clip.length -= delta;
    clip.fadeIn = std::max<juce::int64>(0, clip.fadeIn - delta);
    clampFades(clip);
}

Track* trackOf(ArrangementState& state, const juce::Uuid& clipId, Clip*& clip)
{
    int index = -1;
    clip = state.findClip(clipId, &index);
    return clip != nullptr ? &state.tracks[static_cast<size_t>(index)] : nullptr;
}

double clampRatio(double ratio) { return juce::jlimit(edit::minimumRatio, edit::maximumRatio, ratio); }
} // namespace

bool Clip::needsStretch() const noexcept
{
    return std::abs(stretchRatio - 1.0) > epsilon || std::abs(pitchSemitones) > epsilon;
}

bool Clip::hasCurrentRender() const noexcept
{
    return stretched != nullptr && std::abs(renderedRatio - stretchRatio) < epsilon && std::abs(renderedPitch - pitchSemitones) < epsilon;
}

juce::int64 Clip::maximumLength() const noexcept
{
    return static_cast<juce::int64>(std::floor((static_cast<double>(sourceSamples) - offset) * stretchRatio));
}

double Clip::sourcePositionAt(juce::int64 timelinePosition) const noexcept
{
    return offset + static_cast<double>(timelinePosition - start) / stretchRatio;
}

juce::int64 ArrangementState::endSample() const noexcept
{
    juce::int64 end = 0;
    for (const auto& track : tracks)
        if (! track.clips.empty())
            end = std::max(end, track.clips.back().end());
    return end;
}

const Track* ArrangementState::reampTrack() const noexcept
{
    const auto* track = reampSource.isNull() ? nullptr : findTrack(reampSource);
    return track != nullptr && ! track->drums && ! track->isReturn() ? track : nullptr;
}

Track* ArrangementState::findTrack(const juce::Uuid& id) noexcept
{
    return const_cast<Track*>(std::as_const(*this).findTrack(id));
}

const Track* ArrangementState::findTrack(const juce::Uuid& id) const noexcept
{
    for (const auto& track : tracks)
        if (track.id == id)
            return &track;
    return nullptr;
}

int ArrangementState::trackIndex(const juce::Uuid& id) const noexcept
{
    for (size_t index = 0; index < tracks.size(); ++index)
        if (tracks[index].id == id)
            return static_cast<int>(index);
    return -1;
}

Clip* ArrangementState::findClip(const juce::Uuid& id, int* index) noexcept
{
    return const_cast<Clip*>(std::as_const(*this).findClip(id, index));
}

const Clip* ArrangementState::findClip(const juce::Uuid& id, int* index) const noexcept
{
    for (size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
        for (const auto& clip : tracks[trackIndex].clips)
            if (clip.id == id)
            {
                if (index != nullptr)
                    *index = static_cast<int>(trackIndex);
                return &clip;
            }
    return nullptr;
}

int ArrangementState::returnTrackIndex(int slot) const noexcept
{
    for (size_t index = 0; index < tracks.size(); ++index)
        if (tracks[index].returnSlot == slot)
            return static_cast<int>(index);
    return -1;
}

int ArrangementState::firstReturnIndex() const noexcept
{
    for (size_t index = 0; index < tracks.size(); ++index)
        if (tracks[index].isReturn())
            return static_cast<int>(index);
    return static_cast<int>(tracks.size());
}

juce::int64 Track::contentEnd() const noexcept
{
    return clips.empty() ? 0 : clips.back().end();
}

bool Track::isFrozen(const ArrangementState& state) const
{
    return frozen != nullptr && frozen->signature == freezeSignature(*this, state);
}

juce::String freezeSignature(const Track& track, const ArrangementState& state)
{
    juce::String text;
    text << state.sampleRate << '|' << state.bpm << '|' << (track.drums ? 1 : 0);
    for (const auto& clip : track.clips)
        text << "|c" << clip.file.getFullPathName() << ',' << clip.start << ',' << clip.length << ',' << clip.offset << ','
             << clip.stretchRatio << ',' << clip.pitchSemitones << ',' << clip.gainDb << ',' << clip.fadeIn << ','
             << clip.fadeOut << ',' << clip.pattern;
    if (track.drums)
    {
        for (const auto& pattern : track.patterns)
            text << "|p" << pattern.toValueTree().toXmlString(juce::XmlElement::TextFormat().singleLine());
        text << "|i" << track.instrument->settings.toXmlString(juce::XmlElement::TextFormat().singleLine());
    }
    // Only an active insert chain changes the sound; printed effects are in the recording.
    if (track.fx->enabled.load() && ! track.fx->printWhileRecording.load() && track.fx->settings.isValid())
        text << "|f" << track.fx->settings.toXmlString(juce::XmlElement::TextFormat().singleLine());
    return juce::String::toHexString(text.hashCode64()) + ":" + juce::String(text.length());
}

juce::String returnName(int slot)
{
    return juce::String::charToString(static_cast<juce::juce_wchar>('A' + juce::jlimit(0, maximumReturns - 1, slot)));
}

juce::int64 TimeGrid::snap(juce::int64 position, double divisionInBars) const noexcept
{
    if (divisionInBars <= 0.0)
        return position;
    const auto step = samplesPerBar() * divisionInBars;
    return static_cast<juce::int64>(std::llround(std::round(static_cast<double>(position) / step) * step));
}

juce::String TimeGrid::formatPosition(juce::int64 position) const
{
    const auto bar = std::floor(static_cast<double>(position) / samplesPerBar());
    const auto inBar = static_cast<double>(position) - bar * samplesPerBar();
    const auto beat = std::floor(inBar / samplesPerBeat());
    const auto sixteenth = std::floor((inBar - beat * samplesPerBeat()) / (samplesPerBeat() / 4.0));
    return juce::String(static_cast<int>(bar) + 1) + "." + juce::String(static_cast<int>(beat) + 1) + "." + juce::String(static_cast<int>(sixteenth) + 1);
}

namespace edit
{
Track makeTrack(const ArrangementState& state, const juce::String& name)
{
    static const juce::uint32 palette[] { 0xffe3a447, 0xffc8734a, 0xff8fae5b, 0xff5f9ea8, 0xffb07ac2, 0xffd4c35a, 0xff6b86c9, 0xffcf6a7f };
    Track track;
    track.name = name;
    track.colour = juce::Colour(palette[state.tracks.size() % std::size(palette)]);
    return track;
}

Track makeDrumTrack(const ArrangementState& state, const juce::String& name)
{
    auto track = makeTrack(state, name);
    track.drums = true;
    track.patterns = defaultDrumPatterns();
    track.instrument->settings = SampleKit::settingsFor(SampleKit::defaultKitId()); // real drum samples
    return track;
}

std::optional<Track> makeReturnTrack(const ArrangementState& state)
{
    for (int slot = 0; slot < maximumReturns; ++slot)
        if (state.returnTrackIndex(slot) < 0)
        {
            auto track = makeTrack(state, "Return " + returnName(slot));
            static const juce::uint32 palette[] { 0xff7fb2c9, 0xff9a8fd0, 0xff6fc0a4, 0xffc98fb6 };
            track.colour = juce::Colour(palette[slot]);
            track.returnSlot = slot;
            track.monitorInput = false;
            return track;
        }
    return std::nullopt;
}

int addTrack(ArrangementState& state, Track track)
{
    // Return tracks stay at the bottom, sorted by slot; everything else goes above them.
    auto index = state.firstReturnIndex();
    if (track.isReturn())
        while (index < static_cast<int>(state.tracks.size()) && state.tracks[static_cast<size_t>(index)].returnSlot < track.returnSlot)
            ++index;
    state.tracks.insert(state.tracks.begin() + index, std::move(track));
    return index;
}

Clip makePatternClip(int patternIndex, const juce::String& name, juce::int64 start, juce::int64 length)
{
    Clip clip;
    clip.name = name;
    clip.pattern = std::max(0, patternIndex);
    clip.sourceSamples = Clip::patternSourceSamples;
    clip.start = std::max<juce::int64>(0, start);
    clip.length = std::max<juce::int64>(1, length);
    return clip;
}

Clip makeClip(const juce::File& file, AudioDataPtr source, juce::int64 start)
{
    Clip clip;
    clip.name = file.getFileNameWithoutExtension();
    clip.file = file;
    clip.sourceSamples = source != nullptr ? source->length() : 0;
    clip.source = std::move(source);
    clip.start = std::max<juce::int64>(0, start);
    clip.length = clip.sourceSamples;
    return clip;
}

void sortClips(Track& track)
{
    std::stable_sort(track.clips.begin(), track.clips.end(), [](const Clip& a, const Clip& b) { return a.start < b.start; });
}

void resolveOverlaps(Track& track, const juce::Uuid& winningClipId)
{
    const auto winner = std::find_if(track.clips.begin(), track.clips.end(), [&](const Clip& c) { return c.id == winningClipId; });
    if (winner == track.clips.end())
        return;
    const auto from = winner->start;
    const auto to = winner->end();

    std::vector<Clip> result;
    result.reserve(track.clips.size() + 1);
    for (auto clip : track.clips)
    {
        if (clip.id == winningClipId || clip.end() <= from || clip.start >= to)
        {
            result.push_back(std::move(clip));
            continue;
        }
        if (clip.start < from && clip.end() > to)
        {
            auto right = clip;
            right.id = juce::Uuid();
            cutFront(right, to);
            clip.length = from - clip.start;
            clampFades(clip);
            result.push_back(std::move(clip));
            result.push_back(std::move(right));
        }
        else if (clip.start < from)
        {
            clip.length = from - clip.start;
            clampFades(clip);
            result.push_back(std::move(clip));
        }
        else if (clip.end() > to)
        {
            cutFront(clip, to);
            result.push_back(std::move(clip));
        }
        // else: completely covered, dropped.
    }
    track.clips = std::move(result);
    sortClips(track);
}

std::optional<juce::Uuid> split(ArrangementState& state, const juce::Uuid& clipId, juce::int64 at)
{
    Clip* clip = nullptr;
    auto* track = trackOf(state, clipId, clip);
    if (track == nullptr || at <= clip->start || at >= clip->end())
        return std::nullopt;

    auto right = *clip;
    right.id = juce::Uuid();
    cutFront(right, at);
    right.fadeIn = 0;
    clip->length = at - clip->start;
    clip->fadeOut = 0;
    clampFades(*clip);
    const auto rightId = right.id;
    track->clips.push_back(std::move(right));
    sortClips(*track);
    return rightId;
}

bool move(ArrangementState& state, const juce::Uuid& clipId, juce::int64 newStart, int targetTrack)
{
    int sourceTrack = -1;
    auto* clip = state.findClip(clipId, &sourceTrack);
    if (clip == nullptr || targetTrack < 0 || targetTrack >= static_cast<int>(state.tracks.size())
        || state.tracks[static_cast<size_t>(targetTrack)].isReturn())
        return false;

    auto moved = *clip;
    auto& from = state.tracks[static_cast<size_t>(sourceTrack)].clips;
    from.erase(std::find_if(from.begin(), from.end(), [&](const Clip& c) { return c.id == clipId; }));
    moved.start = std::max<juce::int64>(0, newStart);
    auto& target = state.tracks[static_cast<size_t>(targetTrack)];
    target.clips.push_back(std::move(moved));
    resolveOverlaps(target, clipId);
    return true;
}

bool trimStart(ArrangementState& state, const juce::Uuid& clipId, juce::int64 newStart)
{
    Clip* clip = nullptr;
    auto* track = trackOf(state, clipId, clip);
    if (track == nullptr)
        return false;
    const auto earliest = std::max<juce::int64>(0, clip->start - static_cast<juce::int64>(std::floor(clip->offset * clip->stretchRatio)));
    newStart = juce::jlimit(earliest, clip->end() - minimumLength, newStart);
    cutFront(*clip, newStart);
    clip->offset = std::max(0.0, clip->offset);
    resolveOverlaps(*track, clipId);
    return true;
}

bool trimEnd(ArrangementState& state, const juce::Uuid& clipId, juce::int64 newEnd)
{
    Clip* clip = nullptr;
    auto* track = trackOf(state, clipId, clip);
    if (track == nullptr)
        return false;
    const auto maximum = clip->sourceSamples > 0 ? clip->maximumLength() : clip->length;
    clip->length = juce::jlimit(std::min(minimumLength, maximum), maximum, newEnd - clip->start);
    clampFades(*clip);
    resolveOverlaps(*track, clipId);
    return true;
}

namespace
{
bool applyStretch(ArrangementState& state, const juce::Uuid& clipId, juce::int64 requestedLength, bool keepEnd)
{
    Clip* clip = nullptr;
    auto* track = trackOf(state, clipId, clip);
    if (track == nullptr || clip->length <= 0 || clip->isPattern())
        return false; // patterns follow the tempo instead of being stretched
    const auto oldLength = clip->length;
    const auto oldEnd = clip->end();
    const auto ratio = clampRatio(clip->stretchRatio * static_cast<double>(std::max(minimumLength, requestedLength)) / static_cast<double>(oldLength));
    const auto scale = ratio / clip->stretchRatio;
    clip->length = std::max<juce::int64>(1, std::llround(static_cast<double>(oldLength) * scale));
    clip->fadeIn = std::llround(static_cast<double>(clip->fadeIn) * scale);
    clip->fadeOut = std::llround(static_cast<double>(clip->fadeOut) * scale);
    clip->stretchRatio = ratio;
    if (keepEnd)
        clip->start = std::max<juce::int64>(0, oldEnd - clip->length);
    clampFades(*clip);
    resolveOverlaps(*track, clipId);
    return true;
}
} // namespace

bool stretchEnd(ArrangementState& state, const juce::Uuid& clipId, juce::int64 newEnd)
{
    const auto* clip = state.findClip(clipId);
    return clip != nullptr && applyStretch(state, clipId, newEnd - clip->start, false);
}

bool stretchStart(ArrangementState& state, const juce::Uuid& clipId, juce::int64 newStart)
{
    const auto* clip = state.findClip(clipId);
    return clip != nullptr && applyStretch(state, clipId, clip->end() - newStart, true);
}

bool setStretchRatio(ArrangementState& state, const juce::Uuid& clipId, double ratio)
{
    const auto* clip = state.findClip(clipId);
    if (clip == nullptr)
        return false;
    const auto requested = static_cast<double>(clip->length) * clampRatio(ratio) / clip->stretchRatio;
    return applyStretch(state, clipId, std::llround(requested), false);
}

bool setPitch(ArrangementState& state, const juce::Uuid& clipId, double semitones)
{
    auto* clip = state.findClip(clipId);
    if (clip == nullptr || clip->isPattern())
        return false;
    clip->pitchSemitones = juce::jlimit(-24.0, 24.0, semitones);
    return true;
}

bool setFades(ArrangementState& state, const juce::Uuid& clipId, juce::int64 fadeIn, juce::int64 fadeOut)
{
    auto* clip = state.findClip(clipId);
    if (clip == nullptr)
        return false;
    clip->fadeIn = fadeIn;
    clip->fadeOut = fadeOut;
    clip->fadeIn = juce::jlimit<juce::int64>(0, clip->length, clip->fadeIn);
    clip->fadeOut = juce::jlimit<juce::int64>(0, clip->length - clip->fadeIn, clip->fadeOut);
    return true;
}

bool remove(ArrangementState& state, const juce::Uuid& clipId)
{
    for (auto& track : state.tracks)
    {
        const auto found = std::find_if(track.clips.begin(), track.clips.end(), [&](const Clip& c) { return c.id == clipId; });
        if (found != track.clips.end())
        {
            track.clips.erase(found);
            return true;
        }
    }
    return false;
}

bool selectTake(ArrangementState& state, const juce::Uuid& clipId, int index)
{
    auto* clip = state.findClip(clipId);
    if (clip == nullptr || ! clip->hasTakes() || ! juce::isPositiveAndBelow(index, static_cast<int>(clip->takes.size())) || index == clip->activeTake)
        return false;
    // The takes share the source; only the position in it changes.
    clip->offset += clip->takes[static_cast<size_t>(index)] - clip->takes[static_cast<size_t>(clip->activeTake)];
    clip->activeTake = index;
    return true;
}

bool dropOtherTakes(ArrangementState& state, const juce::Uuid& clipId)
{
    auto* clip = state.findClip(clipId);
    if (clip == nullptr || ! clip->hasTakes())
        return false;
    clip->takes.clear();
    clip->activeTake = 0;
    return true;
}

std::vector<Clip> clipsFromLoopRecording(const juce::File& file, AudioDataPtr source, juce::int64 recorded,
                                         juce::int64 punch, juce::int64 loopStart, juce::int64 loopEnd,
                                         juce::int64 latency, const juce::String& name)
{
    std::vector<Clip> clips;
    const auto plain = [&]
    {
        auto clip = makeClip(file, source, punch);
        const auto shift = juce::jlimit<juce::int64>(0, std::max<juce::int64>(0, clip.sourceSamples - 1), latency);
        clip.offset = static_cast<double>(shift);
        clip.length = clip.sourceSamples - shift;
        clip.name = name;
        if (clip.length > 0)
            clips.push_back(std::move(clip));
        return clips;
    };
    const auto loopLength = loopEnd - loopStart;
    const auto firstPass = loopEnd - punch; // file samples until the first wrap
    if (loopLength <= 0 || punch >= loopEnd || recorded <= firstPass)
        return plain(); // the loop never repeated

    // Lead-in before the loop (recording started earlier than the loop).
    if (punch < loopStart)
    {
        auto lead = makeClip(file, source, punch);
        lead.offset = static_cast<double>(latency);
        lead.length = loopStart - punch;
        lead.name = name;
        clips.push_back(std::move(lead));
    }
    std::vector<double> takes;
    if (punch <= loopStart)
        takes.push_back(static_cast<double>(loopStart - punch + latency)); // the first pass is complete
    int lastComplete = static_cast<int>(takes.size()) - 1;
    for (juce::int64 pass = 0;; ++pass)
    {
        const auto begin = firstPass + pass * loopLength;
        const auto covered = recorded - begin;
        if (covered <= 0)
            break;
        if (covered >= loopLength)
            lastComplete = static_cast<int>(takes.size());
        else if (covered * 2 < loopLength)
            break; // a short last pass is no take
        takes.push_back(static_cast<double>(begin + latency));
    }
    if (takes.empty())
        return plain();
    auto clip = makeClip(file, source, loopStart);
    clip.length = loopLength;
    clip.name = name;
    clip.activeTake = std::max(0, lastComplete);
    clip.offset = takes[static_cast<size_t>(clip.activeTake)];
    if (takes.size() > 1)
        clip.takes = std::move(takes);
    else
        clip.activeTake = 0;
    clips.push_back(std::move(clip));
    return clips;
}

juce::Uuid insert(ArrangementState& state, Clip clip, int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= static_cast<int>(state.tracks.size()) || state.tracks[static_cast<size_t>(trackIndex)].isReturn())
        return juce::Uuid::null(); // a default-constructed Uuid would be random
    clip.id = juce::Uuid();
    const auto id = clip.id;
    place(state, std::move(clip), trackIndex);
    return id;
}

void place(ArrangementState& state, Clip clip, int trackIndex)
{
    // Return tracks carry no clips; they are fed by the sends of other tracks.
    if (trackIndex < 0 || trackIndex >= static_cast<int>(state.tracks.size()) || state.tracks[static_cast<size_t>(trackIndex)].isReturn())
        return;
    clip.start = std::max<juce::int64>(0, clip.start);
    clampFades(clip);
    const auto id = clip.id;
    auto& track = state.tracks[static_cast<size_t>(trackIndex)];
    track.clips.push_back(std::move(clip));
    resolveOverlaps(track, id);
}

bool moveMany(ArrangementState& state, const std::vector<MoveRequest>& requests)
{
    std::vector<std::pair<Clip, int>> lifted;
    for (const auto& request : requests)
    {
        const auto* clip = state.findClip(request.clipId);
        if (clip == nullptr || request.track < 0 || request.track >= static_cast<int>(state.tracks.size())
            || state.tracks[static_cast<size_t>(request.track)].isReturn())
            continue;
        auto copy = *clip;
        copy.start = request.start;
        remove(state, request.clipId);
        lifted.emplace_back(std::move(copy), request.track);
    }
    for (auto& [clip, track] : lifted)
        place(state, std::move(clip), track);
    return ! lifted.empty();
}
} // namespace edit

juce::ValueTree toValueTree(const ArrangementState& state, const juce::File& projectDirectory)
{
    juce::ValueTree root("TonOfAbleProject");
    root.setProperty("version", 1, nullptr);
    root.setProperty("sampleRate", state.sampleRate, nullptr);
    root.setProperty("bpm", state.bpm, nullptr);
    root.setProperty("beatsPerBar", state.beatsPerBar, nullptr);
    root.setProperty("beatUnit", state.beatUnit, nullptr);
    root.setProperty("loopEnabled", state.loopEnabled, nullptr);
    root.setProperty("loopStart", state.loopStart, nullptr);
    root.setProperty("loopEnd", state.loopEnd, nullptr);
    for (const auto& track : state.tracks)
    {
        juce::ValueTree node("Track");
        node.setProperty("id", track.id.toString(), nullptr);
        node.setProperty("name", track.name, nullptr);
        node.setProperty("colour", track.colour.toString(), nullptr);
        node.setProperty("volumeDb", track.mixer->volumeDb.load(), nullptr);
        node.setProperty("pan", track.mixer->pan.load(), nullptr);
        node.setProperty("muted", track.mixer->muted.load(), nullptr);
        node.setProperty("soloed", track.mixer->soloed.load(), nullptr);
        node.setProperty("armed", track.armed, nullptr);
        node.setProperty("monitorInput", track.monitorInput, nullptr);
        node.setProperty("inputChannel", track.inputChannel, nullptr);
        if (track.isReturn())
            node.setProperty("returnSlot", track.returnSlot, nullptr);
        for (int slot = 0; slot < maximumReturns; ++slot)
            if (const auto send = track.mixer->sendGain[static_cast<size_t>(slot)].load(); send > 0.0f)
                node.setProperty("send" + returnName(slot), send, nullptr);
        if (track.frozen != nullptr)
        {
            juce::ValueTree frozen("Frozen");
            const auto relative = projectDirectory != juce::File() && track.frozen->file.isAChildOf(projectDirectory);
            frozen.setProperty("file", relative ? track.frozen->file.getRelativePathFrom(projectDirectory).replaceCharacter('\\', '/') : track.frozen->file.getFullPathName(), nullptr);
            frozen.setProperty("relative", relative, nullptr);
            frozen.setProperty("signature", track.frozen->signature, nullptr);
            node.addChild(frozen, -1, nullptr);
        }
        if (track.drums)
        {
            node.setProperty("drums", true, nullptr);
            juce::ValueTree patterns("Patterns");
            for (const auto& pattern : track.patterns)
                patterns.addChild(pattern.toValueTree(), -1, nullptr);
            node.addChild(patterns, -1, nullptr);
            node.addChild(track.instrument->settings.createCopy(), -1, nullptr);
        }
        if (track.fx->settings.isValid())
        {
            juce::ValueTree fx("Fx");
            fx.setProperty("enabled", track.fx->enabled.load(), nullptr);
            fx.setProperty("print", track.fx->printWhileRecording.load(), nullptr);
            fx.addChild(track.fx->settings.createCopy(), -1, nullptr);
            node.addChild(fx, -1, nullptr);
        }
        for (const auto& clip : track.clips)
        {
            juce::ValueTree c("Clip");
            c.setProperty("id", clip.id.toString(), nullptr);
            c.setProperty("name", clip.name, nullptr);
            const auto relative = projectDirectory != juce::File() && clip.file.isAChildOf(projectDirectory);
            c.setProperty("file", relative ? clip.file.getRelativePathFrom(projectDirectory).replaceCharacter('\\', '/') : clip.file.getFullPathName(), nullptr);
            c.setProperty("relative", relative, nullptr);
            c.setProperty("sourceSamples", clip.sourceSamples, nullptr);
            c.setProperty("start", clip.start, nullptr);
            c.setProperty("length", clip.length, nullptr);
            c.setProperty("offset", clip.offset, nullptr);
            c.setProperty("stretchRatio", clip.stretchRatio, nullptr);
            c.setProperty("pitch", clip.pitchSemitones, nullptr);
            c.setProperty("gainDb", clip.gainDb, nullptr);
            c.setProperty("fadeIn", clip.fadeIn, nullptr);
            c.setProperty("fadeOut", clip.fadeOut, nullptr);
            if (clip.isPattern())
                c.setProperty("pattern", clip.pattern, nullptr);
            if (clip.hasTakes())
            {
                juce::StringArray takes;
                for (const auto take : clip.takes)
                    takes.add(juce::String(take, 3));
                c.setProperty("takes", takes.joinIntoString(","), nullptr);
                c.setProperty("activeTake", clip.activeTake, nullptr);
            }
            node.addChild(c, -1, nullptr);
        }
        root.addChild(node, -1, nullptr);
    }
    return root;
}

ArrangementState fromValueTree(const juce::ValueTree& root, const juce::File& projectDirectory)
{
    ArrangementState state;
    if (! root.hasType("TonOfAbleProject"))
        return state;
    state.sampleRate = std::max(8000.0, static_cast<double>(root.getProperty("sampleRate", 48000.0)));
    state.bpm = juce::jlimit(20.0, 300.0, static_cast<double>(root.getProperty("bpm", 120.0)));
    state.beatsPerBar = juce::jlimit(1, 16, static_cast<int>(root.getProperty("beatsPerBar", 4)));
    state.beatUnit = juce::jlimit(1, 16, static_cast<int>(root.getProperty("beatUnit", 4)));
    state.loopEnabled = root.getProperty("loopEnabled", false);
    state.loopStart = static_cast<juce::int64>(root.getProperty("loopStart", 0));
    state.loopEnd = static_cast<juce::int64>(root.getProperty("loopEnd", 0));
    for (const auto& node : root)
    {
        if (! node.hasType("Track"))
            continue;
        auto track = edit::makeTrack(state, node.getProperty("name", "Audio").toString());
        if (const juce::Uuid id(node.getProperty("id").toString()); ! id.isNull())
            track.id = id;
        if (node.hasProperty("colour"))
            track.colour = juce::Colour::fromString(node.getProperty("colour").toString());
        track.mixer->volumeDb = static_cast<float>(node.getProperty("volumeDb", 0.0f));
        track.mixer->pan = static_cast<float>(node.getProperty("pan", 0.0f));
        track.mixer->muted = static_cast<bool>(node.getProperty("muted", false));
        track.mixer->soloed = static_cast<bool>(node.getProperty("soloed", false));
        track.armed = node.getProperty("armed", false);
        track.monitorInput = node.getProperty("monitorInput", true);
        track.inputChannel = node.getProperty("inputChannel", -1);
        track.returnSlot = juce::jlimit(-1, maximumReturns - 1, static_cast<int>(node.getProperty("returnSlot", -1)));
        for (int slot = 0; slot < maximumReturns; ++slot)
            track.mixer->sendGain[static_cast<size_t>(slot)] = juce::jlimit(0.0f, 2.0f, static_cast<float>(node.getProperty("send" + returnName(slot), 0.0f)));
        if (const auto frozen = node.getChildWithName("Frozen"); frozen.isValid())
        {
            auto render = std::make_shared<FrozenRender>();
            const auto path = frozen.getProperty("file").toString();
            render->file = static_cast<bool>(frozen.getProperty("relative", false)) ? projectDirectory.getChildFile(path) : juce::File(path);
            render->signature = frozen.getProperty("signature").toString();
            if (render->file.existsAsFile())
                track.frozen = std::move(render);
        }
        track.drums = static_cast<bool>(node.getProperty("drums", false));
        if (track.drums)
        {
            for (const auto& pattern : node.getChildWithName("Patterns"))
                track.patterns.push_back(DrumPattern::fromValueTree(pattern));
            if (track.patterns.empty())
                track.patterns = defaultDrumPatterns();
            if (const auto instrument = node.getChildWithName("Instrument"); instrument.isValid())
                track.instrument->settings = instrument.createCopy();
        }
        if (const auto fx = node.getChildWithName("Fx"); fx.isValid() && fx.getNumChildren() > 0)
        {
            track.fx->enabled = static_cast<bool>(fx.getProperty("enabled", false));
            track.fx->printWhileRecording = static_cast<bool>(fx.getProperty("print", false));
            track.fx->settings = fx.getChild(0).createCopy();
        }
        for (const auto& c : node)
        {
            if (! c.hasType("Clip"))
                continue;
            Clip clip;
            if (const juce::Uuid id(c.getProperty("id").toString()); ! id.isNull())
                clip.id = id;
            clip.name = c.getProperty("name").toString();
            const auto path = c.getProperty("file").toString();
            clip.file = static_cast<bool>(c.getProperty("relative", false)) ? projectDirectory.getChildFile(path) : juce::File(path);
            clip.sourceSamples = static_cast<juce::int64>(c.getProperty("sourceSamples", 0));
            clip.start = static_cast<juce::int64>(c.getProperty("start", 0));
            clip.length = static_cast<juce::int64>(c.getProperty("length", 0));
            clip.offset = c.getProperty("offset", 0.0);
            clip.stretchRatio = clampRatio(c.getProperty("stretchRatio", 1.0));
            clip.pitchSemitones = c.getProperty("pitch", 0.0);
            clip.gainDb = static_cast<float>(c.getProperty("gainDb", 0.0f));
            clip.fadeIn = static_cast<juce::int64>(c.getProperty("fadeIn", 0));
            clip.fadeOut = static_cast<juce::int64>(c.getProperty("fadeOut", 0));
            clip.pattern = c.getProperty("pattern", -1);
            for (const auto& take : juce::StringArray::fromTokens(c.getProperty("takes").toString(), ",", {}))
                clip.takes.push_back(take.getDoubleValue());
            clip.activeTake = juce::jlimit(0, std::max(0, static_cast<int>(clip.takes.size()) - 1), static_cast<int>(c.getProperty("activeTake", 0)));
            if (clip.takes.size() < 2)
                clip.takes.clear();
            if (clip.isPattern())
            {
                clip.sourceSamples = Clip::patternSourceSamples;
                clip.stretchRatio = 1.0;
                clip.file = juce::File();
            }
            if (clip.length > 0)
                track.clips.push_back(std::move(clip));
        }
        edit::sortClips(track);
        if (track.isReturn() && (state.returnTrackIndex(track.returnSlot) >= 0 || track.drums))
            track.returnSlot = -1; // a damaged file must not create two returns for one slot
        if (track.isReturn())
            track.clips.clear();
        edit::addTrack(state, std::move(track));
    }
    return state;
}

void changeSampleRate(ArrangementState& state, double newRate)
{
    if (newRate <= 0.0 || std::abs(newRate - state.sampleRate) < epsilon)
        return;
    const auto factor = newRate / state.sampleRate;
    const auto scale = [factor](juce::int64 value) { return static_cast<juce::int64>(std::llround(static_cast<double>(value) * factor)); };
    for (auto& track : state.tracks)
        for (auto& clip : track.clips)
        {
            clip.start = scale(clip.start);
            clip.length = scale(clip.length);
            clip.fadeIn = scale(clip.fadeIn);
            clip.fadeOut = scale(clip.fadeOut);
            if (! clip.isPattern())
                clip.sourceSamples = scale(clip.sourceSamples);
            clip.offset *= factor;
            clip.source.reset();
            clip.stretched.reset();
        }
    state.loopStart = scale(state.loopStart);
    state.loopEnd = scale(state.loopEnd);
    state.sampleRate = newRate;
}
} // namespace daw
