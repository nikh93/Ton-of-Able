#pragma once

#include "Arrangement.h"

/** Drum grooves and fills by style for the drum arrangement editor, and the
    sections that place them on a drum track: a groove repeated over a range
    of bars, a fill every few bars, a crash on the first beat. */
namespace daw::grooves
{
struct Groove
{
    juce::String genre;
    DrumPattern pattern; // its name is "<style>: <name>"
    bool fill = false;
};
[[nodiscard]] const juce::StringArray& genres();
/** Typical tempo of a style (bpm), for the auto mode. */
[[nodiscard]] std::pair<int, int> tempoRange(const juce::String& genre);
[[nodiscard]] const std::vector<Groove>& library();
/** Grooves (or fills) of a style (empty: all styles) that fit a bar of stepsPerBar sixteenths. */
[[nodiscard]] std::vector<const Groove*> find(const juce::String& genre, int stepsPerBar, bool fills);
/** Sixteenth steps in one bar of the time signature (4/4: 16, 3/4 and 6/8: 12). */
[[nodiscard]] int stepsPerBar(const ArrangementState&);

/** A last bar: kick and crash on the one. */
[[nodiscard]] DrumPattern ending(int steps);
/** The pattern with a crash on its first step, named "<name> + Crash". */
[[nodiscard]] DrumPattern withCrash(const DrumPattern&);
/** Index of the pattern with this name on the track, adding the pattern if missing. */
int ensurePattern(Track&, const DrumPattern&);
/** Removes the clips of a track inside [from, to) (clips across the edges are split). */
void clearRange(ArrangementState&, int trackIndex, juce::int64 from, juce::int64 to);
/** Writes a section onto a drum track; its patterns must exist on the track (the crash variant is added). */
void apply(ArrangementState&, int trackIndex, const DrumSection&);

/** Auto mode: a style (random unless given), a verse and a chorus groove with
    fills, sections of eight bars (four for short songs) - verse, verse,
    chorus, chorus ... - with fills at the phrase ends, crashes on the new
    parts and a final bar with kick and crash. */
struct AutoResult
{
    juce::String genre;
    std::vector<DrumSection> sections;
    std::vector<DrumPattern> patterns; // to add to the track
    double bpm = 120.0;                // a tempo that suits the style
};
[[nodiscard]] AutoResult autoFill(int firstBar, int bars, int stepsPerBar, juce::Random&, const juce::String& genre = {});
/** Auto mode that follows the parts of the chord progression editor: one drum
    section per chord section (same boundaries and names). Parts of the same
    kind - same name apart from numbers and primes (A, A', A''; "Strophe 2"),
    else the same progression - share a groove; the first kind gets the verse
    groove, the second the chorus groove, further kinds other grooves of the
    style. Bars between the parts are filled with the verse groove, the last
    bar becomes the ending. */
[[nodiscard]] AutoResult autoFill(const std::vector<ChordSection>& parts, int stepsPerBar, juce::Random&, const juce::String& genre = {});
/** The kind of a part: its name without numbers and primes, else its progression. */
[[nodiscard]] juce::String partKind(const ChordSection&);
} // namespace daw::grooves
