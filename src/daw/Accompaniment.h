#pragma once

#include "Arrangement.h"

namespace daw::accompaniment
{
/** The notes of a chord symbol: root and bass as pitch classes (C = 0),
    intervals above the root in semitones (9 = ninth as 14 and so on). */
struct ChordTones
{
    int root = 0;
    int bass = 0;               // the slash bass, else the root
    std::vector<int> intervals; // sorted, including 0
};
/** Null for "N.C." and names that are no chord (edit::isChordName). H and B are both B natural, Bb is B flat. */
[[nodiscard]] std::optional<ChordTones> parse(const juce::String& name);
/** The chord with the colour of the style added (subtle: add9, rich: 7, 9, 11 / 13). */
[[nodiscard]] ChordTones coloured(ChordTones, int colour);

/** The notes of one layer over these chords, in quarter notes from fromBar.
    quartersPerBar: a bar's length in quarter notes (time signature). */
[[nodiscard]] MidiNotes generate(const std::vector<ChordSymbol>&, const AccompanimentStyle&, double quartersPerBar, double fromBar = 0.0);

/** The clips of a layer: one per run of chords without a gap, named after the layer. */
[[nodiscard]] std::vector<Clip> makeClips(const ArrangementState&, const AccompanimentStyle&);
/** Tempo-independent fingerprint of a track's clips, to notice edits by hand. */
[[nodiscard]] juce::String signature(const Track&, double sampleRate);
/** Rebuilds the clips of an accompaniment track from the chords (keeping clip ids) and links it. */
void regenerate(ArrangementState&, Track&);
/** After an edit: linked tracks whose clips were changed by hand are unlinked,
    the others follow the chords and the time signature. */
void update(ArrangementState&);

[[nodiscard]] juce::String layerName(AccompanimentStyle::Layer);
} // namespace daw::accompaniment
