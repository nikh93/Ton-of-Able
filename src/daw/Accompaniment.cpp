#include "Accompaniment.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace daw::accompaniment
{
namespace
{
int letterClass(juce::juce_wchar letter)
{
    switch (juce::CharacterFunctions::toUpperCase(letter))
    {
        case 'C': return 0;
        case 'D': return 2;
        case 'E': return 4;
        case 'F': return 5;
        case 'G': return 7;
        case 'A': return 9;
        case 'B': case 'H': return 11;
        default: return -1;
    }
}

/** Pitch class of a note name at the start of text ("F#", "Bb", "H"); length read in used. */
int noteClass(const juce::String& text, int& used)
{
    used = 0;
    if (text.isEmpty())
        return -1;
    auto pitch = letterClass(text[0]);
    if (pitch < 0)
        return -1;
    used = 1;
    if (text.length() > 1 && (text[1] == '#' || text[1] == 'b'))
    {
        pitch += text[1] == '#' ? 1 : -1;
        used = 2;
    }
    return (pitch + 12) % 12;
}

constexpr double overlap = 0.25; // quarters a pad note rings into the next chord

/** Small, repeatable variation of the velocity (no randomness: the same chords give the same notes). */
int humanise(double start, int pitch, int spread)
{
    const auto hash = static_cast<int>(std::llround(start * 97.0)) * 31 + pitch * 17;
    return (((hash % (2 * spread + 1)) + (2 * spread + 1)) % (2 * spread + 1)) - spread;
}

struct Range { int low, high, centre; };
Range padRange(int range)
{
    switch (range)
    {
        case 0: return { 43, 67, 55 };
        case 2: return { 57, 81, 69 };
        default: return { 50, 74, 62 };
    }
}

/** Chooses octaves for the pitch classes: an open voicing, close to the previous one. */
std::vector<int> voice(const std::vector<int>& classes, int rootClass, const std::vector<int>& previous, const Range& range)
{
    std::vector<std::vector<int>> options;
    for (const auto pitchClass : classes)
    {
        std::vector<int> pitches;
        for (int pitch = range.low; pitch <= range.high; ++pitch)
            if (pitch % 12 == pitchClass)
                pitches.push_back(pitch);
        options.push_back(pitches);
    }
    std::vector<int> best;
    auto bestCost = 1.0e9;
    for (int strictness = 2; strictness >= 0 && best.empty(); --strictness)
    {
        std::vector<size_t> choice(options.size(), 0);
        for (;;)
        {
            std::vector<int> notes;
            for (size_t index = 0; index < options.size(); ++index)
                if (! options[index].empty())
                    notes.push_back(options[index][choice[index]]);
            std::sort(notes.begin(), notes.end());
            auto ok = ! notes.empty();
            for (size_t index = 1; ok && index < notes.size(); ++index)
            {
                const auto gap = notes[index] - notes[index - 1];
                if (gap == 0 || (strictness >= 1 && gap == 1))
                    ok = false;
                // Low notes close together sound muddy.
                if (strictness >= 2 && index == 1 && gap < (notes[0] < 55 ? 5 : 3))
                    ok = false;
            }
            const auto span = notes.empty() ? 0 : notes.back() - notes.front();
            if (ok && strictness >= 1 && span > 21)
                ok = false;
            if (ok)
            {
                double mean = 0.0;
                for (const auto note : notes)
                    mean += note;
                mean /= static_cast<double>(notes.size());
                auto cost = 0.3 * std::abs(span - 14) + (notes.front() % 12 == rootClass ? 0.0 : 1.5);
                if (previous.empty())
                    cost += std::abs(mean - range.centre);
                else
                {
                    // Voice leading: every voice moves as little as possible, both ways round.
                    for (const auto note : notes)
                    {
                        auto nearest = 127;
                        for (const auto old : previous)
                            nearest = std::min(nearest, std::abs(note - old));
                        cost += nearest;
                    }
                    for (const auto old : previous)
                    {
                        auto nearest = 127;
                        for (const auto note : notes)
                            nearest = std::min(nearest, std::abs(note - old));
                        cost += nearest;
                    }
                    cost += 0.15 * std::abs(mean - range.centre); // no slow creep up or down
                }
                if (cost < bestCost - 1.0e-9)
                {
                    bestCost = cost;
                    best = notes;
                }
            }
            size_t position = 0;
            while (position < choice.size() && (options[position].empty() || ++choice[position] >= options[position].size()))
                choice[position++] = 0;
            if (position == choice.size())
                break;
        }
    }
    return best;
}

/** At most five voices: drop the fifth, then the highest extensions. */
std::vector<int> padClasses(const ChordTones& chord)
{
    auto intervals = chord.intervals;
    if (intervals.size() > 5)
        intervals.erase(std::remove(intervals.begin(), intervals.end(), 7), intervals.end());
    while (intervals.size() > 5)
        intervals.pop_back();
    std::vector<int> classes;
    for (const auto interval : intervals)
    {
        const auto pitchClass = (chord.root + interval) % 12;
        if (std::find(classes.begin(), classes.end(), pitchClass) == classes.end())
            classes.push_back(pitchClass);
    }
    return classes;
}

/** No note of a pitch may sound into the next note of the same pitch (its note-off would cut it). */
void separate(MidiNotes& notes)
{
    std::sort(notes.begin(), notes.end(), [](const MidiNote& a, const MidiNote& b) { return a.start < b.start || (a.start == b.start && a.pitch < b.pitch); });
    std::map<int, size_t> last;
    for (size_t index = 0; index < notes.size(); ++index)
    {
        if (const auto found = last.find(notes[index].pitch); found != last.end())
        {
            auto& before = notes[found->second];
            before.length = std::max(0.05, std::min(before.length, notes[index].start - before.start));
        }
        last[notes[index].pitch] = index;
    }
}
} // namespace

std::optional<ChordTones> parse(const juce::String& typed)
{
    const auto name = typed.trim();
    // Names the lead sheet shows in red are rests, not a guess.
    if (! edit::isChordName(name) || name.equalsIgnoreCase("N.C."))
        return std::nullopt;
    int used = 0;
    ChordTones chord;
    chord.root = noteClass(name, used);
    if (chord.root < 0)
        return std::nullopt;
    auto quality = name.substring(used);
    chord.bass = chord.root;
    if (const auto slash = quality.lastIndexOfChar('/'); slash >= 0)
    {
        int bassUsed = 0;
        if (const auto bass = noteClass(quality.substring(slash + 1), bassUsed); bass >= 0)
            chord.bass = bass;
        quality = quality.substring(0, slash);
    }

    auto third = 4, fifth = 7, seventh = -1;
    auto majorSeventh = false, diminished = false, sixth = false;
    std::set<int> extensions;
    const auto addSeventh = [&] { if (seventh < 0) seventh = majorSeventh ? 11 : diminished ? 9 : 10; };
    int position = 0;
    const auto rest = [&] { return quality.substring(position); };
    const auto take = [&](const char* token)
    {
        if (! rest().startsWith(token))
            return false;
        position += juce::String(token).length();
        return true;
    };
    while (position < quality.length())
    {
        if (take("maj") || take("Maj") || take("^") || take(juce::CharPointer_UTF8("\xce\x94")))
        {
            majorSeventh = true;
            if (position >= quality.length() || ! juce::CharacterFunctions::isDigit(quality[position]))
                addSeventh();
        }
        else if (rest().startsWith("M") && ! rest().startsWith("Mi"))
        {
            ++position;
            majorSeventh = true;
            if (position >= quality.length() || ! juce::CharacterFunctions::isDigit(quality[position]))
                addSeventh();
        }
        else if (take("min") || take("m") || take("-"))
            third = 3;
        else if (take("dim") || take(juce::CharPointer_UTF8("\xc2\xb0")))
        {
            third = 3;
            fifth = 6;
            diminished = true;
        }
        else if (take(juce::CharPointer_UTF8("\xc3\xb8")))
        {
            third = 3;
            fifth = 6;
            seventh = 10;
        }
        else if (take("aug") || take("+"))
            fifth = 8;
        else if (take("sus2"))
            third = 2;
        else if (take("sus4") || take("sus"))
            third = 5;
        else if (take("add"))
        {
            const auto number = rest().initialSectionContainingOnly("0123456789");
            position += number.length();
            switch (number.getIntValue())
            {
                case 2: case 9: extensions.insert(14); break;
                case 4: case 11: extensions.insert(17); break;
                case 6: case 13: extensions.insert(21); break;
                default: break;
            }
        }
        else if (take("b5")) fifth = 6;
        else if (take("#5")) fifth = 8;
        else if (take("b9")) { addSeventh(); extensions.insert(13); }
        else if (take("#9")) { addSeventh(); extensions.insert(15); }
        else if (take("#11")) { addSeventh(); extensions.insert(18); }
        else if (take("b13")) { addSeventh(); extensions.insert(20); }
        else if (juce::CharacterFunctions::isDigit(quality[position]))
        {
            const auto number = rest().initialSectionContainingOnly("0123456789");
            position += number.length();
            switch (number.getIntValue())
            {
                case 5: third = -1; break;               // power chord
                case 6: sixth = true; extensions.insert(9); break;
                case 7: addSeventh(); break;
                case 9: if (sixth) extensions.insert(14); else { addSeventh(); extensions.insert(14); } break;
                case 11: addSeventh(); extensions.insert(14); extensions.insert(17); break;
                case 13: addSeventh(); extensions.insert(14); extensions.insert(21); break;
                default: break;
            }
        }
        else
            ++position; // brackets, commas and what is not understood
    }
    std::set<int> intervals { 0, fifth };
    if (third >= 0)
        intervals.insert(third);
    if (seventh >= 0)
        intervals.insert(seventh);
    intervals.insert(extensions.begin(), extensions.end());
    chord.intervals.assign(intervals.begin(), intervals.end());
    return chord;
}

ChordTones coloured(ChordTones chord, int colour)
{
    const auto has = [&](int interval) { return std::find(chord.intervals.begin(), chord.intervals.end(), interval) != chord.intervals.end(); };
    const auto add = [&](int interval) { if (! has(interval)) chord.intervals.push_back(interval); };
    const auto minor = has(3), major = has(4), diminished = has(6) && ! has(7);
    const auto seventh = has(10) || has(11) || (diminished && has(9));
    const auto extended = std::any_of(chord.intervals.begin(), chord.intervals.end(), [](int i) { return i > 12; });
    if (colour >= 1 && ! diminished && ! extended && ! has(13) && ! has(15))
        add(14); // the ninth: open and floating
    if (colour >= 2 && ! diminished)
    {
        if (major && ! seventh && ! has(9))
            add(11);                 // C -> Cmaj9
        else if (minor && ! seventh)
            add(10);                 // Am -> Am9
        if (minor)
            add(17);                 // ... and the eleventh
        else if (has(10) && major && ! has(20) && ! has(21))
            add(21);                 // G7 -> G13
    }
    std::sort(chord.intervals.begin(), chord.intervals.end());
    return chord;
}

MidiNotes generate(const std::vector<ChordSymbol>& chords, const AccompanimentStyle& style, double quartersPerBar, double fromBar)
{
    MidiNotes notes;
    const auto toQuarters = [&](double bar) { return (bar - fromBar) * quartersPerBar; };
    std::vector<int> previousPad;
    int previousBass = -1;
    double previousEnd = -1.0;
    std::map<int, size_t> sounding; // pitch -> note index of held pad/bass notes
    const auto release = [&](double at, bool ringOn)
    {
        for (const auto& [pitch, index] : sounding)
            notes[index].length = std::max(0.05, at + (ringOn ? overlap : 0.0) - notes[index].start);
        sounding.clear();
    };

    for (const auto& symbol : chords)
    {
        const auto parsed = parse(symbol.name);
        const auto start = toQuarters(symbol.bar), end = toQuarters(symbol.end());
        const auto contiguous = std::abs(start - previousEnd) < 1.0e-6;
        if (! contiguous || ! parsed)
        {
            release(previousEnd, false);
            previousPad.clear();
        }
        previousEnd = end;
        if (! parsed)
            continue;
        const auto chord = coloured(*parsed, style.colour);

        if (style.layer == AccompanimentStyle::Layer::pad)
        {
            const auto voicing = voice(padClasses(chord), chord.root, previousPad, padRange(style.range));
            // Common tones keep sounding, the others ring a little into the new chord.
            std::map<int, size_t> next;
            for (const auto& [pitch, index] : sounding)
                if (std::find(voicing.begin(), voicing.end(), pitch) != voicing.end())
                    next[pitch] = index;
                else
                    notes[index].length = std::max(0.05, start + overlap - notes[index].start);
            for (const auto pitch : voicing)
                if (next.count(pitch) == 0)
                {
                    next[pitch] = notes.size();
                    notes.push_back({ start, end - start, pitch, juce::jlimit(1, 127, 74 + humanise(start, pitch, 5)) });
                }
            for (const auto& [pitch, index] : next)
                notes[index].length = end - notes[index].start;
            sounding = std::move(next);
            previousPad = voicing;
        }
        else if (style.layer == AccompanimentStyle::Layer::bass)
        {
            const auto low = style.range == 0 ? 26 : style.range == 2 ? 36 : 31;
            auto pitch = low + ((chord.bass - low) % 12 + 12) % 12;
            // Stay close to the previous bass note (an octave lower if that is nearer).
            if (previousBass >= 0 && pitch - 12 >= low - 3 && std::abs(pitch - 12 - previousBass) < std::abs(pitch - previousBass))
                pitch -= 12;
            if (contiguous && sounding.count(pitch) > 0)
                notes[sounding[pitch]].length = end - notes[sounding[pitch]].start;
            else
            {
                release(start, false);
                sounding[pitch] = notes.size();
                notes.push_back({ start, end - start, pitch, juce::jlimit(1, 127, 92 + humanise(start, pitch, 4)) });
            }
            previousBass = pitch;
        }
        else
        {
            // Shimmer: the chord high up, up and down in even steps, each note ringing on.
            std::vector<int> tones;
            for (const auto pitchClass : padClasses(chord))
            {
                auto pitch = 67 + style.range * 5;
                while (pitch % 12 != pitchClass)
                    ++pitch;
                tones.push_back(pitch);
            }
            std::sort(tones.begin(), tones.end());
            if (tones.size() > 4)
                tones.resize(4);
            std::vector<int> order;
            for (size_t index = 0; index < tones.size(); ++index)
                order.push_back(static_cast<int>(index));
            for (auto index = static_cast<int>(tones.size()) - 2; index > 0; --index)
                order.push_back(index);
            const auto step = style.rhythm == 0 ? 1.0 : style.rhythm == 2 ? 0.25 : 0.5;
            int count = 0;
            for (auto time = start; time < end - 1.0e-6; time += step, ++count)
            {
                const auto pitch = tones[static_cast<size_t>(order[static_cast<size_t>(count) % order.size()])];
                const auto accent = count == 0 ? 66 : 50;
                notes.push_back({ time, std::min(step * 2.5, end + step - time), pitch, juce::jlimit(1, 127, accent + humanise(time, pitch, 6)) });
            }
        }
    }
    separate(notes);
    return notes;
}

juce::String layerName(AccompanimentStyle::Layer layer)
{
    switch (layer)
    {
        case AccompanimentStyle::Layer::pad: return "Pad";
        case AccompanimentStyle::Layer::bass: return "Bass";
        case AccompanimentStyle::Layer::shimmer: return "Shimmer";
    }
    return "Pad";
}

std::vector<Clip> makeClips(const ArrangementState& state, const AccompanimentStyle& style)
{
    const auto grid = TimeGrid::of(state);
    const auto quartersPerBar = grid.samplesPerBar() / grid.samplesPerQuarter();
    std::vector<Clip> clips;
    std::vector<ChordSymbol> run;
    const auto flush = [&]
    {
        if (run.empty())
            return;
        const auto from = run.front().bar, to = run.back().end();
        const auto start = static_cast<juce::int64>(std::llround(from * grid.samplesPerBar()));
        const auto end = static_cast<juce::int64>(std::llround(to * grid.samplesPerBar()));
        clips.push_back(edit::makeMidiClip(layerName(style.layer), start, end - start, generate(run, style, quartersPerBar, from)));
        run.clear();
    };
    for (const auto& chord : state.chords)
    {
        if (! parse(chord.name))
        {
            flush(); // N.C. and unknown names are rests
            continue;
        }
        if (! run.empty() && chord.bar > run.back().end() + 1.0e-6)
            flush();
        run.push_back(chord);
    }
    flush();
    return clips;
}

juce::String signature(const Track& track, double sampleRate)
{
    // Seconds instead of samples: survives a new sample rate (musical positions are kept).
    juce::String text;
    const auto seconds = [&](double samples) { return juce::String(samples / std::max(1.0, sampleRate), 3); };
    for (const auto& clip : track.clips)
    {
        text << seconds(static_cast<double>(clip.start)) << ':' << seconds(static_cast<double>(clip.length)) << ':'
             << seconds(clip.offset) << ':' << clip.gainDb << ':' << clip.name << '[';
        if (clip.notes != nullptr)
            for (const auto& note : *clip.notes)
                text << juce::String(note.start, 4) << ',' << juce::String(note.length, 4) << ',' << note.pitch << ',' << note.velocity << ';';
        text << ']';
    }
    return juce::String::toHexString(text.hashCode64()) + "/" + juce::String(track.clips.size());
}

void regenerate(ArrangementState& state, Track& track)
{
    if (! track.accompaniment)
        return;
    auto clips = makeClips(state, track.accompaniment->style);
    for (size_t index = 0; index < clips.size() && index < track.clips.size(); ++index)
        clips[index].id = track.clips[index].id; // open piano rolls keep their clip
    if (clips.size() == track.clips.size()
        && std::equal(clips.begin(), clips.end(), track.clips.begin(), [](const Clip& a, const Clip& b)
           { return a.id == b.id && a.start == b.start && a.length == b.length && a.offset == b.offset && a.name == b.name && *a.notes == *b.notes; }))
        clips = track.clips; // unchanged: keep the shared note lists
    track.clips = std::move(clips);
    track.accompaniment->linked = true;
    track.accompaniment->signature = signature(track, state.sampleRate);
}

void update(ArrangementState& state)
{
    for (auto& track : state.tracks)
    {
        if (! track.accompaniment || ! track.accompaniment->linked)
            continue;
        if (signature(track, state.sampleRate) != track.accompaniment->signature)
            track.accompaniment->linked = false; // edited by hand: the notes are the user's now
        else
            regenerate(state, track);
    }
}
} // namespace daw::accompaniment
