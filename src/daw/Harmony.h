#pragma once

#include "Arrangement.h"

/** Keys, scale degrees and a library of chord progressions for the lead
    sheet: the church modes plus harmonic and melodic minor. Progressions are
    written in scale degrees ("Stufen") relative to the major scale, as in pop
    and jazz notation: "i | bVI | bIII | bVII", "ii7 V7 | Imaj7". Within a bar
    chords are separated by spaces (two chords = half bars), bars by "|".

    Degree tokens: optional b/# (relative to the major scale), a Roman numeral
    (upper case major, lower case minor, ° diminished, + augmented), then
    nothing (the scale's triad), "7" (the scale's seventh chord) or another
    suffix ("sus4", "9", "(maj7)" ...). On a scale tone the scale decides the
    chord unless the case asks for another third (borrowed chords). */
namespace daw::harmony
{
enum class Mode { ionian, dorian, phrygian, lydian, mixolydian, aeolian, locrian, harmonicMinor, melodicMinor };
constexpr int numModes = 9;
[[nodiscard]] juce::String modeName(Mode);
/** Semitones of the seven scale degrees above the tonic. */
[[nodiscard]] const std::array<int, 7>& scaleOf(Mode);

struct Key
{
    int tonic = 9; // pitch class, C = 0
    Mode mode = Mode::aeolian;
    bool operator==(const Key&) const = default;
};
/** Spelled as the key's signature suggests (sharps or flats), B natural as "H". */
[[nodiscard]] juce::String noteName(int pitchClass, const Key&);
[[nodiscard]] juce::String keyName(const Key&); // "A Äolisch (Moll)"

struct DegreeChord
{
    int root = 0;          // pitch class
    juce::String suffix;   // chord symbol after the root: "", "m", "7", "m7b5" ...
    juce::String roman;    // the degree as shown: "bVI", "ii7", "viiø7"
};
/** One degree token in a key; nullopt if it is no degree. */
[[nodiscard]] std::optional<DegreeChord> resolve(const juce::String& token, const Key&);
/** A degree progression as chord names ("Am | F | C | G") or as normalised degrees. */
[[nodiscard]] juce::String toChords(const juce::String& degrees, const Key&);
[[nodiscard]] juce::String toRomans(const juce::String& degrees, const Key&);

struct Progression
{
    enum class Kind { body, cadence, halfCadence, deceptive };
    juce::String name;
    juce::String degrees;
    Kind kind = Kind::body;
    [[nodiscard]] int bars() const;
};
/** Common progressions of a mode in different lengths (2 to 16 bars), plus its cadences. */
[[nodiscard]] const std::vector<Progression>& library(Mode);

/** A chord text as bars: per bar one or two chord names; "%" repeats the bar before, "-" is a rest. */
[[nodiscard]] std::vector<juce::StringArray> parseBars(const juce::String& text);
/** Writes a section onto the lead sheet: its bars are cleared, then the progression is repeated over them. */
void apply(ArrangementState&, const ChordSection&);

/** Auto mode: a key (random unless given) and sections that fill bars from
    firstBar with phrases of four bars, each closing with a fitting cadence
    (half cadence in the middle, a full cadence at the end), A A' B A form. */
struct AutoResult
{
    Key key;
    std::vector<ChordSection> sections;
};
[[nodiscard]] AutoResult autoFill(int firstBar, int bars, juce::Random&, std::optional<Key> key = std::nullopt);
} // namespace daw::harmony
