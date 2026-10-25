#include "Harmony.h"

#include <map>

#include "DawText.h"

namespace daw::harmony
{
namespace
{
constexpr std::array<int, 7> majorScale { 0, 2, 4, 5, 7, 9, 11 };
const juce::juce_wchar degreeSign = 0xb0, halfDiminishedSign = 0xf8;

int wrap(int pitch) { return ((pitch % 12) + 12) % 12; }

/** Offset of the mode's tonic above its parent major scale (decides sharps or flats). */
int parentOffset(Mode mode)
{
    switch (mode)
    {
        case Mode::ionian: return 0;
        case Mode::dorian: return 2;
        case Mode::phrygian: return 4;
        case Mode::lydian: return 5;
        case Mode::mixolydian: return 7;
        case Mode::locrian: return 11;
        case Mode::aeolian: case Mode::harmonicMinor: case Mode::melodicMinor: return 9;
    }
    return 0;
}

int romanValue(const juce::String& numeral)
{
    static const std::map<juce::String, int> values { { "I", 1 }, { "II", 2 }, { "III", 3 }, { "IV", 4 }, { "V", 5 }, { "VI", 6 }, { "VII", 7 } };
    const auto found = values.find(numeral.toUpperCase());
    return found != values.end() ? found->second : 0;
}

juce::String romanOf(int degree, bool upper)
{
    static const char* numerals[] { "I", "II", "III", "IV", "V", "VI", "VII" };
    const juce::String numeral(numerals[juce::jlimit(1, 7, degree) - 1]);
    return upper ? numeral : numeral.toLowerCase();
}

juce::String triadSuffix(int third, int fifth)
{
    if (third == 3 && fifth == 6) return "dim";
    if (third == 3 && fifth == 7) return "m";
    if (third == 4 && fifth == 8) return "aug";
    if (third == 3) return "m";
    return {};
}

juce::String seventhSuffix(int third, int fifth, int seventh)
{
    if (third == 4 && fifth == 7) return seventh == 11 ? "maj7" : "7";
    if (third == 3 && fifth == 7) return seventh == 11 ? "m(maj7)" : "m7";
    if (third == 3 && fifth == 6) return seventh == 9 ? "dim7" : "m7b5";
    if (third == 4 && fifth == 8) return seventh == 11 ? "maj7#5" : "7#5";
    return triadSuffix(third, fifth) + "7";
}

/** Bars of a text ("|" or line breaks), each split into its tokens. */
std::vector<juce::StringArray> splitBars(const juce::String& text)
{
    std::vector<juce::StringArray> bars;
    auto parts = juce::StringArray::fromTokens(text.replaceCharacter('\n', '|').replaceCharacter('\r', ' '), "|", {});
    while (! parts.isEmpty() && parts[parts.size() - 1].trim().isEmpty())
        parts.remove(parts.size() - 1);
    for (const auto& part : parts)
    {
        auto tokens = juce::StringArray::fromTokens(part, " \t", {});
        tokens.removeEmptyStrings();
        bars.push_back(tokens);
    }
    return bars;
}

juce::String joinBars(const std::vector<juce::StringArray>& bars)
{
    juce::StringArray parts;
    for (const auto& bar : bars)
        parts.add(bar.joinIntoString(" "));
    return parts.joinIntoString(" | ");
}

using Kind = Progression::Kind;
Progression body(const char* name, const char* degrees) { return { text(name), text(degrees), Kind::body }; }
Progression cadence(const char* name, const char* degrees, Kind kind = Kind::cadence) { return { text(name), text(degrees), kind }; }
} // namespace

juce::String modeName(Mode mode)
{
    switch (mode)
    {
        case Mode::ionian: return text("Ionisch (Dur)");
        case Mode::dorian: return text("Dorisch");
        case Mode::phrygian: return text("Phrygisch");
        case Mode::lydian: return text("Lydisch");
        case Mode::mixolydian: return text("Mixolydisch");
        case Mode::aeolian: return text("Äolisch (Moll)");
        case Mode::locrian: return text("Lokrisch");
        case Mode::harmonicMinor: return text("Harmonisch Moll");
        case Mode::melodicMinor: return text("Melodisch Moll");
    }
    return {};
}

const std::array<int, 7>& scaleOf(Mode mode)
{
    static const std::array<std::array<int, 7>, numModes> scales { {
        { 0, 2, 4, 5, 7, 9, 11 },  // ionian
        { 0, 2, 3, 5, 7, 9, 10 },  // dorian
        { 0, 1, 3, 5, 7, 8, 10 },  // phrygian
        { 0, 2, 4, 6, 7, 9, 11 },  // lydian
        { 0, 2, 4, 5, 7, 9, 10 },  // mixolydian
        { 0, 2, 3, 5, 7, 8, 10 },  // aeolian
        { 0, 1, 3, 5, 6, 8, 10 },  // locrian
        { 0, 2, 3, 5, 7, 8, 11 },  // harmonic minor
        { 0, 2, 3, 5, 7, 9, 11 },  // melodic minor (ascending, "jazz minor")
    } };
    return scales[static_cast<size_t>(mode)];
}

juce::String noteName(int pitchClass, const Key& key)
{
    static const char* sharps[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H" };
    static const char* flats[] { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "H" };
    const auto parent = wrap(key.tonic - parentOffset(key.mode));
    const auto useFlats = parent == 1 || parent == 3 || parent == 5 || parent == 6 || parent == 8 || parent == 10;
    return (useFlats ? flats : sharps)[wrap(pitchClass)];
}

juce::String keyName(const Key& key)
{
    return noteName(key.tonic, key) + " " + modeName(key.mode);
}

std::optional<DegreeChord> resolve(const juce::String& token, const Key& key)
{
    const auto t = token.trim();
    int position = 0, accidental = 0;
    if (t.length() > 1 && (t[0] == 'b' || t[0] == '#'))
    {
        accidental = t[0] == 'b' ? -1 : 1;
        position = 1;
    }
    juce::String numeral;
    while (position < t.length() && juce::String("IViv").containsChar(t[position]))
        numeral << juce::String::charToString(t[position++]);
    const auto degree = romanValue(numeral);
    if (degree == 0)
        return std::nullopt;
    const auto upper = juce::CharacterFunctions::isUpperCase(numeral[0]);
    auto diminished = false, halfDiminished = false, augmented = false;
    if (position < t.length() && t[position] == degreeSign) { diminished = true; ++position; }
    else if (position < t.length() && t[position] == halfDiminishedSign) { halfDiminished = true; ++position; }
    else if (position < t.length() && t[position] == '+') { augmented = true; ++position; }
    const auto rest = t.substring(position);

    const auto root = wrap(key.tonic + majorScale[static_cast<size_t>(degree - 1)] + accidental);
    const auto& scale = scaleOf(key.mode);
    int index = -1;
    for (int i = 0; i < 7; ++i)
        if (wrap(key.tonic + scale[static_cast<size_t>(i)]) == root)
            index = i;
    const auto caseThird = (upper && ! diminished && ! halfDiminished) ? 4 : 3;
    int third, fifth, seventh;
    if (index >= 0)
    {
        const auto step = [&](int offset) { return wrap(scale[static_cast<size_t>((index + offset) % 7)] - scale[static_cast<size_t>(index)]); };
        third = step(2);
        fifth = step(4);
        seventh = step(6);
    }
    else
    {
        third = caseThird;
        fifth = 7;
        seventh = upper ? 10 : 10;
    }
    // The written case or sign wins over the scale (borrowed chords such as V in minor or iv in major).
    if (third != caseThird || diminished || halfDiminished || augmented)
    {
        third = caseThird;
        if (diminished || halfDiminished) fifth = 6;
        else if (augmented) fifth = 8;
        else if (fifth != 7) fifth = 7;
        if (halfDiminished) seventh = 10;
        else if (diminished && index < 0) seventh = 9;
    }

    DegreeChord chord;
    chord.root = root;
    const auto triad = triadSuffix(third, fifth);
    const auto accidentalText = accidental < 0 ? juce::String("b") : accidental > 0 ? juce::String("#") : juce::String();
    const auto numeralText = accidentalText + romanOf(degree, third == 4);
    if (rest == "7" || halfDiminished)
    {
        chord.suffix = seventhSuffix(third, fifth, seventh);
        static const std::map<juce::String, juce::String> shown { { "maj7", "maj7" }, { "7", "7" }, { "m7", "7" }, { "m(maj7)", "(maj7)" },
                                                                  { "m7b5", text("ø7") }, { "dim7", text("°7") }, { "7#5", "+7" }, { "maj7#5", "+maj7" } };
        const auto found = shown.find(chord.suffix);
        chord.roman = numeralText + (found != shown.end() ? found->second : chord.suffix);
    }
    else if (rest.isEmpty())
    {
        chord.suffix = triad;
        chord.roman = numeralText + (triad == "dim" ? text("°") : triad == "aug" ? juce::String("+") : juce::String());
    }
    else
    {
        auto literal = rest;
        if (literal.startsWithChar('(') && literal.endsWithChar(')'))
            literal = literal.substring(1, literal.length() - 1);
        if (triad == "m" && literal.startsWith("maj"))
            chord.suffix = "m(" + literal + ")";
        else if ((triad == "m" || triad == "dim") && literal.startsWith("m"))
            chord.suffix = literal;
        else
            chord.suffix = triad + literal;
        chord.roman = numeralText + (chord.suffix.startsWith("m(") ? chord.suffix.substring(1) : (triad == "m" && chord.suffix.startsWith("m") ? chord.suffix.substring(1) : chord.suffix));
    }
    return chord;
}

juce::String toChords(const juce::String& degrees, const Key& key)
{
    auto bars = splitBars(degrees);
    for (auto& bar : bars)
        for (auto& token : bar)
            if (const auto chord = resolve(token, key))
                token = noteName(chord->root, key) + chord->suffix;
    return joinBars(bars);
}

juce::String toRomans(const juce::String& degrees, const Key& key)
{
    auto bars = splitBars(degrees);
    for (auto& bar : bars)
        for (auto& token : bar)
            if (const auto chord = resolve(token, key))
                token = chord->roman;
    return joinBars(bars);
}

int Progression::bars() const
{
    return static_cast<int>(splitBars(degrees).size());
}

const std::vector<Progression>& library(Mode mode)
{
    // Common progressions per mode, collected from pop, rock, folk and jazz
    // practice; cadences are two bars and feed the auto mode.
    static const std::array<std::vector<Progression>, numModes> all { {
        // Ionisch (Dur)
        { body("Pendel I–IV", "I | IV"),
          body("Pop-Klassiker (Axis)", "I | V | vi | IV"),
          body("50er / Doo-Wop", "I | vi | IV | V"),
          body("Sensibel (Moll-Start)", "vi | IV | I | V"),
          body("Rock-Kadenz", "I | IV | V | IV"),
          body("Quintfall", "I | vi | ii | V"),
          body("Ballade", "I | iii | vi | IV"),
          body("Royal Road (J-Pop)", "IVmaj7 | V7 | iii7 | vi7"),
          body("Jazz-Turnaround", "Imaj7 vi7 | ii7 V7"),
          body("Jazz ii–V–I", "ii7 | V7 | Imaj7 | Imaj7"),
          body("Kanon (Pachelbel)", "I | V | vi | iii | IV | I | IV | V"),
          body("Pop-Strophe", "I | V | vi | IV | I | V | IV | IV"),
          body("Rhythm Changes A", "Imaj7 vi7 | ii7 V7 | iii7 vi7 | ii7 V7 | I(7) | IV IV(m) | iii7 vi7 | ii7 V7"),
          body("Blues 12 Takte", "I(7) | IV(7) | I(7) | I(7) | IV(7) | IV(7) | I(7) | I(7) | V(7) | IV(7) | I(7) | V(7)"),
          body("Song 16 Takte", "I | V | vi | IV | I | V | IV | IV | vi | IV | I | V | vi | IV | V | I"),
          cadence("Authentisch V–I", "V | I"),
          cadence("ii–V–I", "ii7 V7 | Imaj7"),
          cadence("Grundkadenz IV–V–I", "IV V | I"),
          cadence("Plagal IV–I", "IV | I"),
          cadence("Halbschluss", "ii | V", Kind::halfCadence),
          cadence("Halbschluss IV–V", "IV | V", Kind::halfCadence),
          cadence("Trugschluss V–vi", "V | vi", Kind::deceptive) },
        // Dorisch
        { body("Dorischer Vamp (i–IV)", "i7 | IV7"),
          body("Pendel i–ii", "i7 | ii7"),
          body("Dorisch-Rock", "i | IV | i | IV"),
          body("Dorisch klassisch", "i | bVII | IV | i"),
          body("Dorischer Kreis", "i | bIII | IV | i"),
          body("Modal Funk", "i7 | i7 | IV7 | IV7"),
          body("Dorisch 8 Takte", "i7 | IV7 | i7 | IV7 | bIII | IV | v7 | i7"),
          body("Dorisch 16 Takte", "i7 | IV7 | i7 | IV7 | bIII | bVII | IV | IV | i7 | ii7 | bIII | IV | i7 | bVII | IV | i7"),
          cadence("Dorischer Schluss IV–i", "IV | i"),
          cadence("bVII–i", "bVII | i"),
          cadence("v–i", "v7 | i7"),
          cadence("Halbschluss", "i | IV", Kind::halfCadence),
          cadence("Trugschluss", "IV | bIII", Kind::deceptive) },
        // Phrygisch
        { body("Phrygischer Pendel", "i | bII"),
          body("Phrygisch-Spanisch", "i | bII | bIII | bII"),
          body("Phrygisch-Metal", "i | bII | i | bvii"),
          body("Phrygisch dunkel", "i | iv | bII | i"),
          body("Phrygisch absteigend", "i | bvii | bVI | bII"),
          body("Phrygisch 8 Takte", "i | bII | bIII | bII | i | bvii | bVI | bII"),
          cadence("Phrygischer Schluss bII–i", "bII | i"),
          cadence("bvii–i", "bvii | i"),
          cadence("iv–i", "iv | i"),
          cadence("Halbschluss", "i | bII", Kind::halfCadence),
          cadence("Trugschluss", "bII | bVI", Kind::deceptive) },
        // Lydisch
        { body("Lydischer Pendel", "I | II"),
          body("Lydisch schwebend", "Imaj7 | II | Imaj7 | II"),
          body("Lydisch aufsteigend", "I | II | iii | II"),
          body("Lydisch (vi–vii–II–I)", "vi | vii | II | I"),
          body("Lydisch hell", "I | V | II | I"),
          body("Lydisch 8 Takte", "Imaj7 | II | Imaj7 | II | vi | vii | II | II"),
          cadence("Lydischer Schluss II–I", "II | Imaj7"),
          cadence("vii–I", "vii | I"),
          cadence("V–I", "V | I"),
          cadence("Halbschluss", "I | II", Kind::halfCadence),
          cadence("Trugschluss", "II | vi", Kind::deceptive) },
        // Mixolydisch
        { body("Mixo-Pendel", "I | bVII"),
          body("Mixo-Rock", "I | bVII | IV | I"),
          body("Mixo-Coda", "I | IV | bVII | IV"),
          body("Mixo mit v", "I | v | IV | I"),
          body("Mixo-Blues", "I7 | IV | bVII | I7"),
          body("Mixo 8 Takte", "I | bVII | IV | I | I | bVII | IV | IV"),
          cadence("Mixolydischer Schluss bVII–I", "bVII | I"),
          cadence("bVII–IV–I", "bVII IV | I"),
          cadence("v–I", "v | I"),
          cadence("Halbschluss", "I | bVII", Kind::halfCadence),
          cadence("Trugschluss", "bVII | vi", Kind::deceptive) },
        // Äolisch (Moll)
        { body("Moll-Pendel", "i | bVII"),
          body("Moll i–bVI", "i | bVI"),
          body("Epische Moll-Folge", "i | bVI | bIII | bVII"),
          body("Moll-Rock", "i | bVII | bVI | bVII"),
          body("Moll-Quintfall", "i | iv | bVII | bIII"),
          body("Moll-Kadenz", "i | iv | v | i"),
          body("Moll aufsteigend", "i | bVI | bVII | i"),
          body("Moll-Ballade", "i | bIII | bVII | iv"),
          body("Moll 8 Takte", "i | bVI | bIII | bVII | i | bVI | bVII | bVII"),
          body("Moll-Quintfall 8 Takte", "i7 | iv7 | bVII7 | bIIImaj7 | bVImaj7 | ii(m7b5) | V7 | i"),
          body("Moll 16 Takte", "i | bVI | bIII | bVII | i | bVI | bIII | bVII | iv | bVI | bVII | bVII | i | bVI | bVII | i"),
          cadence("Äolischer Schluss bVII–i", "bVII | i"),
          cadence("Epischer Schluss", "bVI bVII | i"),
          cadence("Plagal iv–i", "iv | i"),
          cadence("Moll-Kadenz mit Dur-V", "iv V | i"),
          cadence("Halbschluss", "iv | v", Kind::halfCadence),
          cadence("Halbschluss bVI–bVII", "bVI | bVII", Kind::halfCadence),
          cadence("Trugschluss", "bVII | bVI", Kind::deceptive) },
        // Lokrisch
        { body("Lokrischer Pendel", "i° | bII"),
          body("Lokrisch dunkel", "i° | bII | biii | bII"),
          body("Lokrisch mit Tritonus", "i° | bV | bII | i°"),
          body("Lokrisch 8 Takte", "i° | bII | biii | bII | i° | bvii | bVI | bII"),
          cadence("bII–i°", "bII | i°"),
          cadence("bvii–i°", "bvii | i°"),
          cadence("Halbschluss", "i° | bII", Kind::halfCadence),
          cadence("Trugschluss", "bII | bVI", Kind::deceptive) },
        // Harmonisch Moll
        { body("Pendel i–V", "i | V"),
          body("Moll-Grundkadenz", "i | iv | V | i"),
          body("Andalusische Kadenz", "i | bVII | bVI | V"),
          body("Moll mit bVI", "i | bVI | V | i"),
          body("Moll-Spannung", "i | iv | bVI | V"),
          body("Moll ii–V–i", "ii(m7b5) | V7 | i | i"),
          body("Harmonisch 8 Takte", "i | iv | V | i | bVI | iv | V | V"),
          body("Flamenco 8 Takte", "i | bVII | bVI | V | i | bVII | bVI | V7"),
          cadence("Authentisch V–i", "V7 | i"),
          cadence("iv–V–i", "iv V | i"),
          cadence("Moll ii–V–i", "ii(m7b5) V7 | i"),
          cadence("Halbschluss", "iv | V", Kind::halfCadence),
          cadence("Trugschluss V–bVI", "V | bVI", Kind::deceptive) },
        // Melodisch Moll
        { body("Jazz-Moll-Vamp", "i(maj7) | IV7"),
          body("Melodisch Moll i–IV", "i | IV | i | IV"),
          body("Moll ii–V–i (melodisch)", "i(maj7) | ii7 | V7 | i(maj7)"),
          body("Übermäßig", "i | bIII+ | IV | V"),
          body("Melodisch 8 Takte", "i(maj7) | i(maj7) | IV7 | IV7 | ii7 | V7 | i(maj7) | V7"),
          cadence("V7–i", "V7 | i(maj7)"),
          cadence("IV7–V7–i", "IV7 V7 | i(maj7)"),
          cadence("ii–V–i", "ii7 V7 | i(maj7)"),
          cadence("Halbschluss", "IV7 | V7", Kind::halfCadence),
          cadence("Trugschluss", "V7 | vi(m7b5)", Kind::deceptive) },
    } };
    return all[static_cast<size_t>(mode)];
}

std::vector<juce::StringArray> parseBars(const juce::String& text)
{
    auto bars = splitBars(text);
    for (size_t index = 0; index < bars.size(); ++index)
    {
        if (bars[index].size() == 1 && bars[index][0] == "%")
            bars[index] = index > 0 ? bars[index - 1] : juce::StringArray();
        else if (bars[index].size() == 1 && bars[index][0] == "-")
            bars[index].clear();
    }
    return bars;
}

void apply(ArrangementState& state, const ChordSection& section)
{
    if (section.bars <= 0)
        return;
    edit::setChord(state, section.firstBar, section.bars, {});
    const auto bars = parseBars(section.text);
    if (bars.empty())
        return;
    for (int index = 0; index < section.bars; ++index)
    {
        const auto& bar = bars[static_cast<size_t>(index) % bars.size()];
        const auto start = static_cast<double>(section.firstBar + index);
        if (bar.size() == 1)
            edit::setChord(state, start, 1.0, bar[0]);
        else if (bar.size() >= 2)
        {
            edit::setChord(state, start, 0.5, bar[0]);
            edit::setChord(state, start + 0.5, 0.5, bar[1]);
        }
    }
}

AutoResult autoFill(int firstBar, int bars, juce::Random& random, std::optional<Key> fixedKey)
{
    AutoResult result;
    if (fixedKey)
        result.key = *fixedKey;
    else
    {
        // Mostly major and minor, now and then a colourful mode; guitar-friendly keys more often.
        static const std::array<std::pair<Mode, int>, numModes> modeWeights { {
            { Mode::ionian, 30 }, { Mode::aeolian, 26 }, { Mode::dorian, 10 }, { Mode::mixolydian, 9 }, { Mode::harmonicMinor, 9 },
            { Mode::lydian, 6 }, { Mode::phrygian, 5 }, { Mode::melodicMinor, 4 }, { Mode::locrian, 1 } } };
        auto pick = random.nextInt(100);
        for (const auto& [mode, weight] : modeWeights)
        {
            if (pick < weight) { result.key.mode = mode; break; }
            pick -= weight;
        }
        static const std::array<int, 12> tonicWeights { 3, 1, 3, 1, 3, 2, 1, 3, 1, 3, 1, 1 };
        auto tonicPick = random.nextInt(23);
        for (int tonic = 0; tonic < 12; ++tonic)
        {
            if (tonicPick < tonicWeights[static_cast<size_t>(tonic)]) { result.key.tonic = tonic; break; }
            tonicPick -= tonicWeights[static_cast<size_t>(tonic)];
        }
    }
    const auto& entries = library(result.key.mode);
    std::vector<juce::String> bodies, finals, halves, deceptives;
    for (const auto& entry : entries)
    {
        auto parts = splitBars(entry.degrees);
        if (entry.kind == Kind::body && parts.size() >= 2 && parts.size() % 2 == 0)
        {
            // The opening half of a phrase: two bars (from longer progressions their start, or a whole two-bar figure).
            const std::vector<juce::StringArray> start(parts.begin(), parts.begin() + 2);
            bodies.push_back(joinBars(start));
        }
        else if (entry.kind == Kind::cadence) finals.push_back(entry.degrees);
        else if (entry.kind == Kind::halfCadence) halves.push_back(entry.degrees);
        else if (entry.kind == Kind::deceptive) deceptives.push_back(entry.degrees);
    }
    if (bodies.empty() || finals.empty())
        return result;
    if (halves.empty()) halves = finals;
    if (deceptives.empty()) deceptives = finals;
    const auto any = [&](const std::vector<juce::String>& list) { return list[static_cast<size_t>(random.nextInt(static_cast<int>(list.size())))]; };
    const auto bodyA = any(bodies);
    auto bodyB = any(bodies);
    for (int attempt = 0; attempt < 6 && bodyB == bodyA && bodies.size() > 1; ++attempt)
        bodyB = any(bodies);

    const auto keyText = keyName(result.key);
    const auto phrases = bars / 4;
    const auto rest = bars % 4;
    auto bar = firstBar;
    static const char* names[] { "A", "A'", "B", "A''" };
    for (int phrase = 0; phrase < phrases; ++phrase)
    {
        const auto last = phrase == phrases - 1 && rest < 2;
        const auto role = phrase % 4;
        const auto& opening = role == 2 ? bodyB : bodyA;
        const auto& close = last ? any(finals) : role == 1 ? any(random.nextBool() ? deceptives : finals) : any(halves);
        auto name = juce::String(names[role]);
        if (phrase >= 4)
            name << " " << juce::String(phrase / 4 + 1);
        result.sections.push_back({ name, bar, 4, toChords(opening + " | " + close, result.key), keyText });
        bar += 4;
    }
    if (rest > 0)
    {
        const auto& scale = scaleOf(result.key.mode);
        const auto tonicToken = scale[2] == 4 ? juce::String("I") : scale[4] == 6 ? text("i°") : juce::String("i");
        juce::String degrees;
        if (rest == 1) degrees = tonicToken;
        else if (rest == 2) degrees = any(finals);
        else degrees = tonicToken + " | " + any(finals);
        result.sections.push_back({ text("Schluss"), bar, rest, toChords(degrees, result.key), keyText });
    }
    return result;
}
} // namespace daw::harmony
