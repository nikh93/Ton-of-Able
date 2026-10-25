#include "DrumStyles.h"

#include <map>

#include "DawText.h"

namespace daw::grooves
{
namespace
{
enum Lane { kick, snare, rim, clap, closedHat, pedalHat, openHat, lowTom, midTom, highTom, crash, ride };

/** One lane as text, a character per sixteenth: X accent, x normal, o ghost, - rest. */
using Lanes = std::initializer_list<std::pair<Lane, const char*>>;

Groove groove(const char* genre, const char* name, Lanes lanes, float swing = 0.0f, bool fill = false)
{
    Groove result;
    result.genre = text(genre);
    result.fill = fill;
    result.pattern.name = text(genre) + ": " + text(name);
    result.pattern.swing = swing;
    int steps = 0;
    for (const auto& [lane, row] : lanes)
    {
        const juce::String cells(row);
        steps = std::max(steps, cells.length());
        for (int step = 0; step < cells.length() && step < DrumPattern::maxSteps; ++step)
        {
            const auto c = cells[step];
            const auto velocity = c == 'X' ? 118 : c == 'x' ? 92 : c == 'o' ? 48 : 0;
            result.pattern.velocity[static_cast<size_t>(lane)][static_cast<size_t>(step)] = static_cast<juce::uint8>(velocity);
        }
    }
    result.pattern.steps = std::max(1, steps);
    return result;
}

Groove fill(const char* genre, const char* name, Lanes lanes, float swing = 0.0f) { return groove(genre, name, lanes, swing, true); }
} // namespace

const juce::StringArray& genres()
{
    static const juce::StringArray list { "Rock", "Pop", "Funk", "Disco/House", "Hip-Hop", "Metal", "Blues/Shuffle", "Jazz",
                                          "Reggae", "Latin", "Punk", "Country", "Ballade", "3/4 & 6/8" };
    return list;
}

std::pair<int, int> tempoRange(const juce::String& genre)
{
    static const std::map<juce::String, std::pair<int, int>> ranges {
        { "Rock", { 100, 140 } }, { "Pop", { 90, 125 } }, { "Funk", { 92, 112 } }, { "Disco/House", { 115, 128 } },
        { "Hip-Hop", { 80, 96 } }, { "Metal", { 120, 170 } }, { "Blues/Shuffle", { 75, 120 } }, { "Jazz", { 110, 180 } },
        { "Reggae", { 68, 84 } }, { "Latin", { 120, 140 } }, { "Punk", { 160, 200 } }, { "Country", { 100, 140 } },
        { "Ballade", { 60, 80 } }, { "3/4 & 6/8", { 66, 96 } } };
    const auto found = ranges.find(genre);
    return found != ranges.end() ? found->second : std::pair { 100, 130 };
}

const std::vector<Groove>& library()
{
    static const std::vector<Groove> all {
        groove("Rock", "Standard", { { kick, "X-------X-x-----" }, { snare, "----X-------X---" }, { closedHat, "X-x-X-x-X-x-X-x-" } }),
        groove("Rock", "Treibend", { { kick, "X-----x-X-x-----" }, { snare, "----X-------X---" }, { closedHat, "XxXxXxXxXxXxXxXx" } }),
        groove("Rock", "Ride (Refrain)", { { kick, "X-----x-X-x-----" }, { snare, "----X-------X---" }, { ride, "X-x-X-x-X-x-X-x-" } }),
        groove("Rock", "Offene HiHat", { { kick, "X-------X-x-----" }, { snare, "----X-------X---" }, { openHat, "x-x-x-x-x-x-x-x-" } }),
        fill("Rock", "Fill Toms", { { kick, "X-------X-------" }, { snare, "----X---xx------" }, { closedHat, "X-x-x-x---------" },
                                    { highTom, "----------xx----" }, { midTom, "------------xx--" }, { lowTom, "--------------xX" } }),
        fill("Rock", "Fill Snare", { { kick, "X-------X-------" }, { snare, "----X-------xxxX" }, { closedHat, "X-x-x-x-X-x-----" } }),

        groove("Pop", "Straight", { { kick, "X-------X--x----" }, { snare, "----X-------X---" }, { closedHat, "XoxoXoxoXoxoXoxo" } }),
        groove("Pop", "Four-Floor Clap", { { kick, "X---X---X---X---" }, { clap, "----X-------X---" }, { closedHat, "--x---x---x---x-" } }),
        groove("Pop", "Rimclick", { { kick, "X-------x-x-----" }, { rim, "----x-------x---" }, { closedHat, "x-x-x-x-x-x-x-x-" } }),
        groove("Pop", "Refrain", { { kick, "X-x-----X-x-----" }, { snare, "----X-------X---" }, { openHat, "--x---x---x---x-" }, { closedHat, "X---X---X---X---" } }),
        fill("Pop", "Fill", { { kick, "X-------X-------" }, { snare, "----X-------xoxX" }, { closedHat, "X-x-x-x-X-x-----" } }),

        groove("Funk", "16tel", { { kick, "X--x------X--x--" }, { snare, "----X--o-o--X--o" }, { closedHat, "xoxoxoxoxoxoxoxo" } }, 0.1f),
        groove("Funk", "Ghost Notes", { { kick, "X-x-------x-----" }, { snare, "----X-oo-o--X-o-" }, { closedHat, "x-x-x-x-x-x-x-x-" }, { openHat, "--------------x-" } }, 0.12f),
        groove("Funk", "Synkopiert", { { kick, "X--x--x---x-----" }, { snare, "----X-------X-o-" }, { closedHat, "xoxoxoxoxoxoxoxo" } }, 0.08f),
        fill("Funk", "Fill", { { kick, "X-------X-----x-" }, { snare, "----X---X-xoX-xX" }, { closedHat, "x-x-x-x---------" } }, 0.1f),

        groove("Disco/House", "Four on the Floor", { { kick, "X---X---X---X---" }, { clap, "----X-------X---" }, { openHat, "--x---x---x---x-" } }),
        groove("Disco/House", "Disco Open HiHat", { { kick, "X---X---X---X---" }, { snare, "----X-------X---" }, { openHat, "--X---X---X---X-" },
                                                    { closedHat, "x-o-x-o-x-o-x-o-" } }),
        groove("Disco/House", "House Shuffle", { { kick, "X---X---X---X---" }, { clap, "----X-------X---" }, { closedHat, "-x-x-x-x-x-x-x-x" } }, 0.3f),
        fill("Disco/House", "Snare-Wirbel", { { kick, "X---X---X---X---" }, { snare, "--------xxxxXXXX" } }),

        groove("Hip-Hop", "Boom Bap", { { kick, "X------x-xX-----" }, { snare, "----X-------X---" }, { closedHat, "x-x-x-x-x-x-x-x-" } }, 0.18f),
        groove("Hip-Hop", "Lazy", { { kick, "X--x---x--X-----" }, { snare, "----X--o----X---" }, { closedHat, "x-x-x-x-x-x-x-x-" } }, 0.25f),
        groove("Hip-Hop", "Trap", { { kick, "X------X--X-----" }, { clap, "--------X-------" }, { closedHat, "XxxxXxxxXxxxXxxx" } }),
        fill("Hip-Hop", "Fill", { { kick, "X------x-x------" }, { snare, "----X-------X-xx" }, { closedHat, "x-x-x-x-x-x-----" } }, 0.18f),

        groove("Metal", "Double Bass", { { kick, "xxxxxxxxxxxxxxxx" }, { snare, "----X-------X---" }, { ride, "X---X---X---X---" } }),
        groove("Metal", "Thrash", { { kick, "X---x---X---x---" }, { snare, "--X---X---X---X-" }, { ride, "X-x-X-x-X-x-X-x-" } }),
        groove("Metal", "Breakdown", { { kick, "X--X--X---X-----" }, { snare, "--------X-------" }, { crash, "X-------X-------" } }),
        groove("Metal", "Galopp", { { kick, "X-xxX-xxX-xxX-xx" }, { snare, "----X-------X---" }, { closedHat, "X-x-X-x-X-x-X-x-" } }),
        fill("Metal", "Fill", { { kick, "xxxxxxxxxxxxxxxx" }, { snare, "----X---xxxx----" }, { highTom, "------------xx--" }, { lowTom, "--------------xX" } }),

        groove("Blues/Shuffle", "Shuffle", { { kick, "X-----x-X-------" }, { snare, "----X-------X---" }, { ride, "X-x-X-x-X-x-X-x-" },
                                             { pedalHat, "----x-------x---" } }, 0.5f),
        groove("Blues/Shuffle", "Slow Blues", { { kick, "X-------X-------" }, { snare, "----X-------X---" }, { closedHat, "x-x-x-x-x-x-x-x-" } }, 0.5f),
        groove("Blues/Shuffle", "Texas Shuffle", { { kick, "X---X---X---X---" }, { snare, "----X-------X---" }, { closedHat, "X-x-X-x-X-x-X-x-" } }, 0.5f),
        fill("Blues/Shuffle", "Fill", { { kick, "X-------X-------" }, { snare, "----X---x-x-x-xX" }, { closedHat, "X-x-x-x-X-------" } }, 0.5f),

        groove("Jazz", "Swing Ride", { { ride, "X---x-x-X---x-x-" }, { pedalHat, "----x-------x---" }, { kick, "o---o---o---o---" } }, 0.55f),
        groove("Jazz", "Two-Feel", { { ride, "X---x-x-X---x-x-" }, { pedalHat, "----x-------x---" }, { kick, "x-------x-------" } }, 0.55f),
        groove("Jazz", "Comping", { { ride, "X---x-x-X---x-x-" }, { pedalHat, "----x-------x---" }, { snare, "------o-----o--o" }, { kick, "o-------o-----x-" } }, 0.55f),
        fill("Jazz", "Fill", { { ride, "X---x-x---------" }, { snare, "--------x-x-x-X-" }, { kick, "x-------------x-" } }, 0.55f),

        groove("Reggae", "One Drop", { { kick, "--------X-------" }, { rim, "--------X-------" }, { closedHat, "x-x-x-x-x-x-x-x-" } }, 0.3f),
        groove("Reggae", "Steppers", { { kick, "X---X---X---X---" }, { rim, "--------X-------" }, { closedHat, "--x---x---x---x-" } }, 0.2f),
        groove("Reggae", "Rockers", { { kick, "X-------X-------" }, { rim, "----x-------x---" }, { closedHat, "x-x-x-x-x-x-x-x-" } }, 0.25f),
        fill("Reggae", "Fill", { { kick, "--------X-------" }, { rim, "--------X-------" }, { snare, "------------xxxX" } }, 0.3f),

        groove("Latin", "Bossa Nova", { { kick, "X--xX--xX--xX--x" }, { rim, "X--x--x---x--x--" }, { closedHat, "x-x-x-x-x-x-x-x-" } }),
        groove("Latin", "Samba", { { kick, "X--xX--xX--xX--x" }, { rim, "x-x--x-x-x--x-x-" }, { closedHat, "xoxoxoxoxoxoxoxo" } }),
        groove("Latin", "Songo", { { kick, "---x------x-----" }, { snare, "--o-X--o--o-X---" }, { ride, "X-x-X-x-X-x-X-x-" } }),
        fill("Latin", "Fill", { { kick, "X--xX--x--------" }, { highTom, "--------xx------" }, { midTom, "----------xx----" }, { lowTom, "------------xxXX" } }),

        groove("Punk", "Skank", { { kick, "X---X---X---X---" }, { snare, "--X---X---X---X-" }, { closedHat, "X-x-X-x-X-x-X-x-" } }),
        groove("Punk", "D-Beat", { { kick, "X-----x-X-x-----" }, { snare, "----X-------X---" }, { crash, "X-x-x-x-X-x-x-x-" } }),
        groove("Punk", "Straight", { { kick, "X-x-----X-x-----" }, { snare, "----X-------X---" }, { closedHat, "XxXxXxXxXxXxXxXx" } }),
        fill("Punk", "Fill", { { kick, "X-------X-------" }, { snare, "xxxxxxxxXXXXXXXX" } }),

        groove("Country", "Train Beat", { { kick, "X-------X-------" }, { snare, "o-o-X-o-o-o-X-o-" } }),
        groove("Country", "Two-Step", { { kick, "X-------X-------" }, { snare, "----X-------X---" }, { closedHat, "--x---x---x---x-" } }),
        groove("Country", "Shuffle", { { kick, "X-------X-------" }, { snare, "----X-------X---" }, { closedHat, "x-x-x-x-x-x-x-x-" } }, 0.4f),
        fill("Country", "Fill", { { kick, "X-------X-------" }, { snare, "----X---xoxoxoxX" } }),

        groove("Ballade", "Half-Time", { { kick, "X-----x---x-----" }, { snare, "--------X-------" }, { closedHat, "x-x-x-x-x-x-x-x-" } }),
        groove("Ballade", "Rimclick", { { kick, "X-------x-------" }, { rim, "----x-------x---" }, { closedHat, "x-x-x-x-x-x-x-x-" } }),
        groove("Ballade", "Ride", { { kick, "X-------x-x-----" }, { snare, "----x-------x---" }, { ride, "x-x-x-x-x-x-x-x-" } }),
        fill("Ballade", "Fill", { { kick, "X-------X-------" }, { snare, "--------x-x-X---" }, { highTom, "------------x---" },
                                  { midTom, "-------------x--" }, { lowTom, "--------------xX" } }),

        groove("3/4 & 6/8", "Walzer", { { kick, "X-----------" }, { snare, "----x---x---" }, { closedHat, "x-x-x-x-x-x-" } }),
        groove("3/4 & 6/8", "6/8 Ballade", { { kick, "X-----------" }, { snare, "------X-----" }, { closedHat, "x-x-x-x-x-x-" } }),
        groove("3/4 & 6/8", "6/8 Treibend", { { kick, "X-----x---x-" }, { snare, "------X-----" }, { closedHat, "X-x-x-X-x-x-" } }),
        fill("3/4 & 6/8", "3/4 Fill", { { kick, "X-----------" }, { snare, "----x-x-xxxX" } }),
        fill("3/4 & 6/8", "6/8 Fill", { { kick, "X-----------" }, { snare, "------xx----" }, { highTom, "--------x---" }, { midTom, "---------x--" },
                                        { lowTom, "----------xX" } }),
    };
    return all;
}

std::vector<const Groove*> find(const juce::String& genre, int stepsPerBar, bool fills)
{
    std::vector<const Groove*> found;
    for (const auto& entry : library())
        if ((genre.isEmpty() || entry.genre == genre) && entry.fill == fills && (stepsPerBar <= 0 || entry.pattern.steps == stepsPerBar))
            found.push_back(&entry);
    return found;
}

int stepsPerBar(const ArrangementState& state)
{
    return juce::jlimit(1, DrumPattern::maxSteps, state.beatsPerBar * 16 / std::max(1, state.beatUnit));
}

DrumPattern ending(int steps)
{
    DrumPattern pattern;
    pattern.name = text("Schluss (Kick + Crash)");
    pattern.steps = juce::jlimit(1, DrumPattern::maxSteps, steps);
    pattern.velocity[kick][0] = 120;
    pattern.velocity[crash][0] = 120;
    return pattern;
}

DrumPattern withCrash(const DrumPattern& source)
{
    auto pattern = source;
    pattern.name = source.name + " + Crash";
    pattern.velocity[crash][0] = 115;
    pattern.velocity[openHat][0] = 0; // the crash takes the place of a cymbal on the one
    pattern.velocity[ride][0] = 0;
    return pattern;
}

int ensurePattern(Track& track, const DrumPattern& pattern)
{
    for (size_t index = 0; index < track.patterns.size(); ++index)
        if (track.patterns[index].name == pattern.name)
            return static_cast<int>(index);
    track.patterns.push_back(pattern);
    return static_cast<int>(track.patterns.size()) - 1;
}

void clearRange(ArrangementState& state, int trackIndex, juce::int64 from, juce::int64 to)
{
    if (! juce::isPositiveAndBelow(trackIndex, static_cast<int>(state.tracks.size())) || to <= from)
        return;
    const auto ids = [&]
    {
        std::vector<juce::Uuid> list;
        for (const auto& clip : state.tracks[static_cast<size_t>(trackIndex)].clips)
            list.push_back(clip.id);
        return list;
    };
    for (const auto& id : ids())
    {
        if (const auto* clip = state.findClip(id); clip != nullptr && clip->start < from && clip->end() > from)
            edit::split(state, id, from);
    }
    for (const auto& id : ids())
    {
        if (const auto* clip = state.findClip(id); clip != nullptr && clip->start < to && clip->end() > to)
            edit::split(state, id, to);
    }
    for (const auto& id : ids())
        if (const auto* clip = state.findClip(id); clip != nullptr && clip->start >= from && clip->end() <= to)
            edit::remove(state, id);
}

void apply(ArrangementState& state, int trackIndex, const DrumSection& section)
{
    if (! juce::isPositiveAndBelow(trackIndex, static_cast<int>(state.tracks.size())) || section.bars <= 0)
        return;
    const auto samplesPerBar = TimeGrid::of(state).samplesPerBar();
    const auto barStart = [&](int bar) { return static_cast<juce::int64>(std::llround(bar * samplesPerBar)); };
    clearRange(state, trackIndex, barStart(section.firstBar), barStart(section.firstBar + section.bars));
    auto& track = state.tracks[static_cast<size_t>(trackIndex)];
    const auto indexOf = [&track](const juce::String& name)
    {
        for (size_t index = 0; index < track.patterns.size(); ++index)
            if (track.patterns[index].name == name)
                return static_cast<int>(index);
        return -1;
    };
    const auto groove = indexOf(section.groove);
    if (groove < 0)
        return;
    const auto fillPattern = section.fill.isNotEmpty() ? indexOf(section.fill) : -1;
    const auto crashPattern = section.crash ? ensurePattern(track, withCrash(track.patterns[static_cast<size_t>(groove)])) : -1;

    // One pattern per bar, then runs of the same pattern become one clip.
    std::vector<int> bars;
    for (int bar = 0; bar < section.bars; ++bar)
    {
        auto pattern = groove;
        const auto fillHere = fillPattern >= 0 && section.fillEvery >= 0
                           && (section.fillEvery == 0 ? bar == section.bars - 1 : (bar + 1) % section.fillEvery == 0);
        if (fillHere && section.bars > 1)
            pattern = fillPattern;
        else if (bar == 0 && crashPattern >= 0)
            pattern = crashPattern;
        bars.push_back(pattern);
    }
    for (int bar = 0; bar < section.bars;)
    {
        auto end = bar + 1;
        while (end < section.bars && bars[static_cast<size_t>(end)] == bars[static_cast<size_t>(bar)])
            ++end;
        const auto& pattern = state.tracks[static_cast<size_t>(trackIndex)].patterns[static_cast<size_t>(bars[static_cast<size_t>(bar)])];
        edit::insert(state, edit::makePatternClip(bars[static_cast<size_t>(bar)], pattern.name, barStart(section.firstBar + bar),
                                                  barStart(section.firstBar + end) - barStart(section.firstBar + bar)), trackIndex);
        bar = end;
    }
}

AutoResult autoFill(int firstBar, int bars, int steps, juce::Random& random, const juce::String& wanted)
{
    AutoResult result;
    // Styles with grooves and fills for this time signature.
    juce::StringArray usable;
    for (const auto& genre : genres())
        if (! find(genre, steps, false).empty() && ! find(genre, steps, true).empty())
            usable.add(genre);
    if (usable.isEmpty())
        return result;
    result.genre = usable.contains(wanted) ? wanted : usable[random.nextInt(usable.size())];
    const auto [low, high] = tempoRange(result.genre);
    result.bpm = static_cast<double>(low + random.nextInt(high - low + 1));

    const auto grooves = find(result.genre, steps, false);
    const auto fills = find(result.genre, steps, true);
    const auto any = [&random](const std::vector<const Groove*>& list) { return list[static_cast<size_t>(random.nextInt(static_cast<int>(list.size())))]; };
    const auto* verse = any(grooves);
    auto* chorus = any(grooves);
    for (int attempt = 0; attempt < 8 && chorus == verse && grooves.size() > 1; ++attempt)
        chorus = any(grooves);
    const auto* fillA = any(fills);
    const auto* fillB = any(fills);
    for (const auto* groove : { verse, chorus, fillA, fillB })
        if (std::none_of(result.patterns.begin(), result.patterns.end(), [&](const DrumPattern& p) { return p.name == groove->pattern.name; }))
            result.patterns.push_back(groove->pattern);

    // The last bar ends the song (from five bars on).
    const auto hasEnding = bars >= 5;
    const auto body = hasEnding ? bars - 1 : bars;
    const auto phrase = body >= 16 ? 8 : 4;
    static const char* names[] { "Strophe", "Strophe", "Refrain", "Refrain" };
    int verses = 0, choruses = 0;
    auto bar = firstBar;
    for (int index = 0; bar < firstBar + body; ++index)
    {
        DrumSection section;
        const auto role = index % 4;
        const auto isChorus = role >= 2;
        section.name = text(names[role]) + " " + juce::String(isChorus ? ++choruses : ++verses);
        section.firstBar = bar;
        section.bars = std::min(phrase, firstBar + body - bar);
        section.groove = (isChorus ? chorus : verse)->pattern.name;
        section.fill = (isChorus ? fillB : fillA)->pattern.name;
        section.fillEvery = isChorus && section.bars >= 8 ? 4 : 0;   // the chorus gets a fill halfway, too
        section.crash = index > 0 || isChorus;                        // a crash marks every new part
        result.sections.push_back(section);
        bar += section.bars;
    }
    if (hasEnding)
    {
        const auto end = ending(steps);
        result.patterns.push_back(end);
        result.sections.push_back({ text("Schluss"), bar, 1, end.name, {}, -1, false });
    }
    return result;
}
juce::String partKind(const ChordSection& part)
{
    auto name = part.name.trim();
    while (name.isNotEmpty() && (juce::CharacterFunctions::isDigit(name.getLastCharacter()) || name.getLastCharacter() == '\''
                                 || name.getLastCharacter() == ' ' || name.getLastCharacter() == 0x2019 || name.getLastCharacter() == 0x2032))
        name = name.dropLastCharacters(1);
    // Generic names from the editor ("Teil 3") say nothing about the kind: the progression does.
    if (name.isEmpty() || name == text("Teil"))
        return "#" + part.text.removeCharacters(" ");
    return name.toLowerCase();
}

AutoResult autoFill(const std::vector<ChordSection>& chordParts, int steps, juce::Random& random, const juce::String& wanted)
{
    auto parts = chordParts;
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const ChordSection& p) { return p.bars <= 0; }), parts.end());
    std::stable_sort(parts.begin(), parts.end(), [](const ChordSection& a, const ChordSection& b) { return a.firstBar < b.firstBar; });
    if (parts.empty())
        return {};
    const auto first = parts.front().firstBar;
    int end = first;
    for (const auto& part : parts)
        end = std::max(end, part.firstBar + part.bars);

    // Style, grooves and fills as in the plain auto mode.
    auto result = autoFill(first, end - first, steps, random, wanted);
    if (result.sections.empty())
        return result;
    const auto grooves = find(result.genre, steps, false);
    const auto fills = find(result.genre, steps, true);
    const auto verse = result.sections.front().groove;
    juce::String chorus;
    for (const auto& section : result.sections)
        if (section.groove != verse && section.name != text("Schluss"))
            chorus = section.groove;
    if (chorus.isEmpty())
        chorus = verse;
    const auto verseFill = result.sections.front().fill;
    juce::String chorusFill = verseFill;
    for (const auto& section : result.sections)
        if (section.groove == chorus && section.fill.isNotEmpty())
            chorusFill = section.fill;
    const auto ending = result.sections.back().name == text("Schluss") ? result.sections.back().groove : juce::String();

    // One groove per kind of part.
    juce::StringArray kinds;
    std::map<juce::String, std::pair<juce::String, juce::String>> grooveOf; // kind -> groove, fill
    const auto addPattern = [&result](const DrumPattern& pattern)
    {
        if (std::none_of(result.patterns.begin(), result.patterns.end(), [&](const DrumPattern& p) { return p.name == pattern.name; }))
            result.patterns.push_back(pattern);
    };
    for (const auto& part : parts)
    {
        const auto kind = partKind(part);
        if (grooveOf.count(kind) > 0)
            continue;
        if (kind == "schluss" || kind == "ende" || kind == "outro" || kind == "coda")
        {
            grooveOf[kind] = { verse, verseFill }; // a closing part keeps the main groove
            continue;
        }
        const auto index = kinds.size();
        kinds.add(kind);
        if (index == 0) grooveOf[kind] = { verse, verseFill };
        else if (index == 1) grooveOf[kind] = { chorus, chorusFill };
        else
        {
            // A third kind (bridge, solo ...): another groove of the style if there is one.
            const auto* other = grooves[static_cast<size_t>(random.nextInt(static_cast<int>(grooves.size())))];
            for (const auto* candidate : grooves)
                if (candidate->pattern.name != verse && candidate->pattern.name != chorus)
                    other = candidate;
            const auto* fill = fills[static_cast<size_t>(random.nextInt(static_cast<int>(fills.size())))];
            addPattern(other->pattern);
            addPattern(fill->pattern);
            grooveOf[kind] = { other->pattern.name, fill->pattern.name };
        }
    }

    std::vector<DrumSection> sections;
    auto bar = first;
    const auto add = [&](const juce::String& name, int from, int count, const juce::String& kind)
    {
        if (count <= 0)
            return;
        DrumSection section;
        section.name = name;
        section.firstBar = from;
        section.bars = count;
        section.groove = grooveOf[kind].first;
        section.fill = grooveOf[kind].second;
        section.fillEvery = kinds.indexOf(kind) == 1 && count >= 8 ? 4 : 0;
        section.crash = ! sections.empty();
        sections.push_back(section);
    };
    for (const auto& part : parts)
    {
        if (part.firstBar < bar)
            continue; // overlapping parts: the earlier one wins
        if (part.firstBar > bar)
        {
            grooveOf[text("zwischenspiel")] = { verse, verseFill };
            add(text("Zwischenspiel"), bar, part.firstBar - bar, text("zwischenspiel"));
        }
        add(part.name.isNotEmpty() ? part.name : text("Teil"), part.firstBar, part.bars, partKind(part));
        bar = part.firstBar + part.bars;
    }
    // The last bar ends the song.
    if (ending.isNotEmpty() && ! sections.empty() && end - first >= 5)
    {
        auto& last = sections.back();
        if (last.bars > 1)
        {
            --last.bars;
            sections.push_back({ text("Schluss"), end - 1, 1, ending, {}, -1, false });
        }
    }
    else
        result.patterns.erase(std::remove_if(result.patterns.begin(), result.patterns.end(), [&](const DrumPattern& p) { return p.name == ending && ending.isNotEmpty(); }),
                              result.patterns.end());
    result.sections = sections;
    return result;
}
} // namespace daw::grooves
