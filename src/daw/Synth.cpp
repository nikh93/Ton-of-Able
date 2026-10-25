#include "Synth.h"

#include <cmath>

namespace daw
{
namespace
{
constexpr double twoPi = 6.283185307179586;

// PolyBLEP-corrected sawtooth: no aliasing whistle in the high notes.
float saw(double phase, double increment) noexcept
{
    auto value = 2.0 * phase - 1.0;
    if (phase < increment)
    {
        const auto t = phase / increment;
        value -= t + t - t * t - 1.0;
    }
    else if (phase > 1.0 - increment)
    {
        const auto t = (phase - 1.0) / increment;
        value -= t * t + t + t + 1.0;
    }
    return static_cast<float>(value);
}

double frequencyOf(int note) noexcept { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }
} // namespace

juce::String Synth::presetName(Preset preset)
{
    switch (preset)
    {
        case Preset::pad: return "Pad";
        case Preset::keys: return "Keys";
        case Preset::bass: return "Bass";
        case Preset::lead: return "Lead";
        case Preset::atmo: return "Atmo";
        case Preset::swell: return "Swell";
        case Preset::sub: return "Sub";
        case Preset::glass: return "Glass";
    }
    return "Pad";
}

Synth::Preset Synth::presetFromSettings(const juce::ValueTree& settings)
{
    return static_cast<Preset>(juce::jlimit(0, numPresets - 1, static_cast<int>(settings.getProperty("preset", 0))));
}

Synth::Synth(Preset p) : preset(p)
{
    switch (preset)
    {
        //                         attack decay sustain release cutoff env  reso  detune sub   gain
        case Preset::pad:  parameters = { 0.6f, 1.5f, 0.8f, 1.8f, 900.0f, 1.5f, 0.15f, 0.12f, 0.2f, 0.20f }; break;
        case Preset::keys: parameters = { 0.005f, 0.9f, 0.35f, 0.35f, 1400.0f, 2.5f, 0.1f, 0.04f, 0.1f, 0.24f }; break;
        case Preset::bass: parameters = { 0.004f, 0.35f, 0.6f, 0.12f, 300.0f, 2.8f, 0.3f, 0.03f, 0.7f, 0.32f }; break;
        case Preset::lead: parameters = { 0.01f, 0.3f, 0.75f, 0.25f, 2200.0f, 1.8f, 0.35f, 0.07f, 0.0f, 0.22f }; break;
        case Preset::atmo: parameters = { 1.2f, 2.5f, 0.85f, 3.5f, 1000.0f, 0.8f, 0.2f, 0.16f, 0.12f, 0.17f, 0.9f, 0.6f }; break;
        case Preset::swell: parameters = { 3.0f, 3.0f, 0.9f, 4.5f, 850.0f, 1.2f, 0.25f, 0.18f, 0.1f, 0.17f, 1.0f, 0.7f }; break;
        case Preset::sub: parameters = { 0.08f, 1.0f, 0.9f, 0.9f, 180.0f, 0.6f, 0.05f, 0.02f, 0.9f, 0.30f, 0.0f, 0.0f }; break;
        case Preset::glass: parameters = { 0.004f, 1.4f, 0.1f, 1.8f, 2600.0f, 1.0f, 0.15f, 0.06f, 0.0f, 0.14f, 1.0f, 0.3f }; break;
    }
}

void Synth::prepare(double rate, int)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    chorusLine.assign(static_cast<size_t>(sampleRate * 0.05) + 2, 0.0f);
    chorusWrite = 0;
    allNotesOff();
    for (auto& voice : voices)
        voice = {};
}

void Synth::noteOn(int note, int velocity) noexcept
{
    // Re-trigger a sounding voice of the same note, else take an idle or the oldest one.
    Voice* chosen = nullptr;
    for (auto& voice : voices)
        if (voice.stage != 0 && voice.note == note)
            chosen = &voice;
    if (chosen == nullptr)
        for (auto& voice : voices)
            if (voice.stage == 0 && chosen == nullptr)
                chosen = &voice;
    if (chosen == nullptr)
    {
        chosen = &voices[0];
        for (auto& voice : voices)
            if (voice.age < chosen->age)
                chosen = &voice;
    }
    if (chosen->stage == 0)
    {
        chosen->phase1 = 0.0;
        chosen->phase2 = 0.37;
        chosen->phaseSub = 0.0;
        chosen->low = chosen->band = 0.0f;
        chosen->level = 0.0f;
    }
    chosen->note = note;
    chosen->held = true;
    chosen->velocity = static_cast<float>(velocity) / 127.0f;
    chosen->stage = 1;
    chosen->age = ++clock;
}

void Synth::noteOff(int note) noexcept
{
    for (auto& voice : voices)
        if (voice.note == note && voice.held)
        {
            voice.held = false;
            voice.stage = 3;
        }
}

void Synth::allNotesOff() noexcept
{
    for (auto& voice : voices)
        if (voice.stage != 0)
        {
            voice.held = false;
            voice.stage = 3;
        }
}

float Synth::renderVoice(Voice& voice) noexcept
{
    const auto& p = parameters;
    const auto dt = static_cast<float>(1.0 / sampleRate);
    switch (voice.stage)
    {
        case 1:
            voice.level += dt / std::max(0.001f, p.attack);
            if (voice.level >= 1.0f) { voice.level = 1.0f; voice.stage = 2; }
            break;
        case 2:
            voice.level += (p.sustain - voice.level) * std::min(1.0f, dt * 4.0f / std::max(0.01f, p.decay));
            break;
        case 3:
            voice.level -= voice.level * std::min(1.0f, dt * 5.0f / std::max(0.01f, p.release)) + dt * 0.0005f;
            if (voice.level <= 0.0001f) { voice = {}; return 0.0f; }
            break;
        default: return 0.0f;
    }
    const auto frequency = frequencyOf(voice.note);
    const auto increment = frequency / sampleRate;
    const auto detune = std::pow(2.0, p.detune / 12.0);
    const auto value = 0.5f * (saw(voice.phase1, increment) + saw(voice.phase2, increment * detune))
                     + p.sub * static_cast<float>(std::sin(twoPi * voice.phaseSub));
    voice.phase1 += increment;
    voice.phase2 += increment * detune;
    voice.phaseSub += increment * 0.5;
    voice.phase1 -= std::floor(voice.phase1);
    voice.phase2 -= std::floor(voice.phase2);
    voice.phaseSub -= std::floor(voice.phaseSub);

    // State-variable low-pass; the envelope and the velocity open it.
    const auto cutoff = std::min(sampleRate * 0.45, static_cast<double>(p.cutoff) * std::pow(2.0, driftOctaves + p.envelopeAmount * voice.level * (0.5f + 0.5f * voice.velocity)));
    const auto f = static_cast<float>(2.0 * std::sin(juce::MathConstants<double>::pi * cutoff / sampleRate));
    const auto q = 1.0f - p.resonance * 0.9f;
    voice.low += f * voice.band;
    const auto high = value - voice.low - q * voice.band;
    voice.band += f * high;
    return voice.low * voice.level * (0.35f + 0.65f * voice.velocity);
}

void Synth::render(const DrumEvent* events, int numEvents, float* left, float* right, int numSamples) noexcept
{
    DrumEvent previews[16];
    const auto previewCount = takePreviews(previews, 16);
    const auto apply = [this](const DrumEvent& event)
    {
        if (event.note == allNotesOffNote)
            allNotesOff();
        else if (event.velocity > 0)
            noteOn(event.note, event.velocity);
        else
            noteOff(event.note);
    };
    for (int index = 0; index < previewCount; ++index)
        apply(previews[index]);
    // The filter wanders slowly (one cycle in about 11 s): the pad breathes.
    driftPhase += numSamples / sampleRate * 0.09;
    driftPhase -= std::floor(driftPhase);
    driftOctaves = parameters.drift * static_cast<float>(std::sin(twoPi * driftPhase));
    const auto chorus = parameters.chorus > 0.0f && chorusLine.size() > 4;
    const auto lineSize = static_cast<int>(chorusLine.size());
    const auto chorusIncrement = 0.23 / sampleRate;
    int next = 0;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (; next < numEvents && events[next].offset <= sample; ++next)
            apply(events[next]);
        float sum = 0.0f;
        for (auto& voice : voices)
            if (voice.stage != 0)
                sum += renderVoice(voice);
        // Gentle saturation keeps chords from clipping.
        const auto out = std::tanh(sum * parameters.gain * 1.6f) * 0.7f;
        if (! chorus)
        {
            left[sample] = out;
            right[sample] = out;
            continue;
        }
        // Stereo chorus: two taps around 14 ms, modulated in opposite directions.
        chorusLine[static_cast<size_t>(chorusWrite)] = out;
        const auto tap = [&](double modulation)
        {
            const auto delay = sampleRate * (0.014 + 0.004 * modulation);
            auto position = static_cast<double>(chorusWrite) - delay;
            while (position < 0.0)
                position += lineSize;
            const auto index = static_cast<int>(position);
            const auto fraction = static_cast<float>(position - index);
            const auto a = chorusLine[static_cast<size_t>(index % lineSize)], b = chorusLine[static_cast<size_t>((index + 1) % lineSize)];
            return a + (b - a) * fraction;
        };
        const auto modulation = std::sin(twoPi * chorusPhase);
        const auto wet = parameters.chorus * 0.6f;
        left[sample] = out * (1.0f - 0.3f * parameters.chorus) + wet * tap(modulation);
        right[sample] = out * (1.0f - 0.3f * parameters.chorus) + wet * tap(-modulation);
        chorusWrite = (chorusWrite + 1) % lineSize;
        chorusPhase += chorusIncrement;
        if (chorusPhase >= 1.0)
            chorusPhase -= 1.0;
    }
}

juce::ValueTree synthInstrumentSettings(Synth::Preset preset)
{
    juce::ValueTree settings("Instrument");
    settings.setProperty("type", "synth", nullptr);
    settings.setProperty("preset", static_cast<int>(preset), nullptr);
    return settings;
}
} // namespace daw
