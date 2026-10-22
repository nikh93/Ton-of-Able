#include "Drums.h"

#include <algorithm>
#include <cmath>

namespace daw
{
namespace
{
constexpr double twoPi = 6.283185307179586;
enum Lane { kick, snare, rim, clap, closedHat, pedalHat, openHat, lowTom, midTom, highTom, crash, ride };

// Gate for note-offs sent to plugin instruments: one bar, so cymbals ring.
constexpr double gateSteps = 16.0;

DrumPattern makePattern(const juce::String& name, float swing,
                        std::initializer_list<std::pair<int, std::vector<std::pair<int, int>>>> hits)
{
    DrumPattern pattern;
    pattern.name = name;
    pattern.swing = swing;
    for (const auto& [lane, steps] : hits)
        for (const auto& [step, velocity] : steps)
            pattern.velocity[static_cast<size_t>(lane)][static_cast<size_t>(step)] = static_cast<juce::uint8>(velocity);
    return pattern;
}

std::vector<std::pair<int, int>> every(int first, int stride, int velocity, int accent = -1)
{
    std::vector<std::pair<int, int>> steps;
    for (int step = first; step < 16; step += stride)
        steps.emplace_back(step, accent > 0 && step % 4 == 0 ? accent : velocity);
    return steps;
}
} // namespace

const std::array<DrumLane, numDrumLanes>& drumLanes()
{
    static const std::array<DrumLane, numDrumLanes> lanes { {
        { "Kick", 36 }, { "Snare", 38 }, { "Rimshot", 37 }, { "Clap", 39 },
        { "HiHat zu", 42 }, { "HiHat Pedal", 44 }, { "HiHat offen", 46 },
        { "Tom tief", 45 }, { "Tom mittel", 47 }, { "Tom hoch", 50 },
        { "Crash", 49 }, { "Ride", 51 },
    } };
    return lanes;
}

bool DrumPattern::isEmpty() const noexcept
{
    for (const auto& lane : velocity)
        for (int step = 0; step < steps; ++step)
            if (lane[static_cast<size_t>(step)] != 0)
                return false;
    return true;
}

juce::ValueTree DrumPattern::toValueTree() const
{
    juce::ValueTree tree("Pattern");
    tree.setProperty("name", name, nullptr);
    tree.setProperty("steps", steps, nullptr);
    tree.setProperty("swing", swing, nullptr);
    for (size_t lane = 0; lane < velocity.size(); ++lane)
    {
        juce::String row;
        for (int step = 0; step < steps; ++step)
            row << juce::String::toHexString(static_cast<int>(velocity[lane][static_cast<size_t>(step)])).paddedLeft('0', 2);
        tree.setProperty("lane" + juce::String(static_cast<int>(lane)), row, nullptr);
    }
    return tree;
}

DrumPattern DrumPattern::fromValueTree(const juce::ValueTree& tree)
{
    DrumPattern pattern;
    pattern.name = tree.getProperty("name").toString();
    pattern.steps = juce::jlimit(1, maxSteps, static_cast<int>(tree.getProperty("steps", 16)));
    pattern.swing = juce::jlimit(0.0f, 0.6f, static_cast<float>(tree.getProperty("swing", 0.0f)));
    for (size_t lane = 0; lane < pattern.velocity.size(); ++lane)
    {
        const auto row = tree.getProperty("lane" + juce::String(static_cast<int>(lane))).toString();
        for (int step = 0; step < pattern.steps && step * 2 + 1 < row.length(); ++step)
            pattern.velocity[lane][static_cast<size_t>(step)] = static_cast<juce::uint8>(juce::jlimit(0, 127, row.substring(step * 2, step * 2 + 2).getHexValue32()));
    }
    return pattern;
}

std::vector<DrumPattern> defaultDrumPatterns()
{
    return {
        makePattern("A Rock", 0.0f, { { kick, { { 0, 115 }, { 8, 110 }, { 10, 95 } } }, { snare, { { 4, 112 }, { 12, 112 } } },
                                      { closedHat, every(0, 2, 80, 100) } }),
        makePattern("B Rock Fill", 0.0f, { { kick, { { 0, 115 }, { 8, 110 } } }, { snare, { { 4, 112 }, { 10, 90 } } },
                                           { closedHat, { { 0, 95 }, { 2, 80 }, { 4, 95 }, { 6, 80 }, { 8, 95 } } },
                                           { highTom, { { 12, 105 }, { 13, 95 } } }, { midTom, { { 14, 105 } } }, { lowTom, { { 15, 110 } } } }),
        makePattern("C Half-Time", 0.0f, { { kick, { { 0, 115 }, { 6, 95 }, { 11, 90 } } }, { snare, { { 8, 118 } } },
                                           { closedHat, every(0, 2, 80, 100) }, { crash, { { 0, 100 } } } }),
        makePattern("D Four on the Floor", 0.0f, { { kick, every(0, 4, 118) }, { clap, { { 4, 105 }, { 12, 105 } } },
                                                   { openHat, every(2, 4, 85) }, { closedHat, every(1, 2, 55) } }),
        makePattern("E Metal Double Bass", 0.0f, { { kick, every(0, 1, 100, 115) }, { snare, { { 4, 120 }, { 12, 120 } } },
                                                    { crash, { { 0, 110 } } }, { ride, every(4, 4, 95) } }),
        makePattern("F Shuffle", 0.5f, { { kick, { { 0, 112 }, { 6, 85 }, { 8, 108 } } }, { snare, { { 4, 110 }, { 12, 110 } } },
                                         { ride, every(0, 2, 85, 100) }, { pedalHat, { { 4, 70 }, { 12, 70 } } } }),
        makePattern("G Funk", 0.12f, { { kick, { { 0, 118 }, { 3, 95 }, { 10, 105 } } },
                                       { snare, { { 4, 115 }, { 7, 45 }, { 9, 45 }, { 12, 115 }, { 15, 50 } } },
                                       { closedHat, every(0, 1, 60, 95) }, { openHat, { { 14, 85 } } } }),
        makePattern("H Leer", 0.0f, {}),
    };
}

int collectPatternEvents(const DrumPattern& pattern, double samplesPerStep,
                         juce::int64 clipStart, juce::int64 clipEnd, double patternOffset,
                         juce::int64 position, int length, DrumEvent* out, int maxEvents) noexcept
{
    const auto from = std::max(position, clipStart);
    const auto to = std::min(position + length, clipEnd);
    if (from >= to || samplesPerStep <= 0.0 || pattern.steps <= 0)
        return 0;
    const auto patternLength = pattern.steps * samplesPerStep;
    const auto gate = gateSteps * samplesPerStep;
    // Pattern-time window of this block.
    const auto a = static_cast<double>(from - clipStart) + patternOffset;
    const auto b = static_cast<double>(to - clipStart) + patternOffset;
    const auto blockBase = from - position;
    int count = 0;
    const auto emit = [&](double time, int note, int velocity)
    {
        if (time < a || time >= b || count >= maxEvents)
            return;
        const auto offset = static_cast<int>(blockBase + static_cast<juce::int64>(std::floor(time - a)));
        out[count++] = { juce::jlimit(0, length - 1, offset), static_cast<juce::uint8>(note), static_cast<juce::uint8>(velocity) };
    };
    const auto firstCycle = static_cast<juce::int64>(std::floor((a - gate) / patternLength));
    const auto lastCycle = static_cast<juce::int64>(std::floor(b / patternLength));
    for (auto cycle = firstCycle; cycle <= lastCycle; ++cycle)
        for (int step = 0; step < pattern.steps; ++step)
        {
            const auto swingDelay = step % 2 == 1 ? static_cast<double>(pattern.swing) * samplesPerStep : 0.0;
            const auto onTime = static_cast<double>(cycle) * patternLength + step * samplesPerStep + swingDelay;
            if (onTime + gate < a || onTime >= b)
                continue;
            for (size_t lane = 0; lane < pattern.velocity.size(); ++lane)
                if (const auto velocity = pattern.velocity[lane][static_cast<size_t>(step)]; velocity > 0)
                {
                    const auto note = drumLanes()[lane].note;
                    emit(onTime, note, velocity);
                    if (onTime >= patternOffset) // only release notes this clip played
                        emit(onTime + gate, note, 0);
                }
        }
    return count;
}

void sortEvents(DrumEvent* events, int count) noexcept
{
    // Small arrays: insertion sort keeps note-offs before simultaneous note-ons.
    for (int i = 1; i < count; ++i)
    {
        const auto event = events[i];
        auto j = i - 1;
        while (j >= 0 && (events[j].offset > event.offset || (events[j].offset == event.offset && events[j].velocity > 0 && event.velocity == 0)))
        {
            events[j + 1] = events[j];
            --j;
        }
        events[j + 1] = event;
    }
}

void TrackInstrument::preview(int note, int velocity) noexcept
{
    const auto write = writeIndex.load(std::memory_order_relaxed);
    const auto next = (write + 1) % queueSize;
    if (next == readIndex.load(std::memory_order_acquire))
        return; // queue full: drop the audition
    queue[static_cast<size_t>(write)].store((juce::jlimit(0, 127, note) << 8) | juce::jlimit(1, 127, velocity), std::memory_order_relaxed);
    writeIndex.store(next, std::memory_order_release);
}

int TrackInstrument::takePreviews(DrumEvent* out, int maxEvents) noexcept
{
    int count = 0;
    auto read = readIndex.load(std::memory_order_relaxed);
    while (count < maxEvents && read != writeIndex.load(std::memory_order_acquire))
    {
        const auto packed = queue[static_cast<size_t>(read)].load(std::memory_order_relaxed);
        out[count++] = { 0, static_cast<juce::uint8>(packed >> 8), static_cast<juce::uint8>(packed & 0xff) };
        read = (read + 1) % queueSize;
    }
    readIndex.store(read, std::memory_order_release);
    return count;
}

juce::ValueTree builtInInstrumentSettings()
{
    juce::ValueTree settings("Instrument");
    settings.setProperty("type", "builtin", nullptr);
    return settings;
}

//==============================================================================
DrumSynth::DrumSynth()
{
    for (size_t index = 0; index < voices.size(); ++index)
        voices[index].noise = static_cast<juce::uint32>(0x9e3779b9u * (index + 1));
}

void DrumSynth::prepare(double rate, int)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    for (auto& voice : voices)
        voice.active = false;
}

void DrumSynth::trigger(int note, int velocity) noexcept
{
    int lane = -1;
    for (size_t index = 0; index < drumLanes().size(); ++index)
        if (drumLanes()[index].note == note)
            lane = static_cast<int>(index);
    if (lane < 0)
        return;
    if (lane == closedHat || lane == pedalHat)
        for (auto& voice : voices)
            if (voice.active && voice.lane == openHat)
                voice.choking = true; // closing the hi-hat chokes the open one
    // Prefer a free voice, otherwise steal round-robin.
    auto* chosen = &voices[static_cast<size_t>(nextVoice)];
    for (auto& voice : voices)
        if (! voice.active)
        {
            chosen = &voice;
            break;
        }
    nextVoice = (nextVoice + 1) % static_cast<int>(voices.size());
    const auto seed = chosen->noise;
    *chosen = Voice {};
    chosen->noise = seed == 0 ? 22222u : seed;
    chosen->active = true;
    chosen->lane = lane;
    chosen->gain = std::pow(static_cast<float>(velocity) / 127.0f, 1.5f);
}

float DrumSynth::renderVoice(Voice& v) noexcept
{
    const auto dt = 1.0 / sampleRate;
    const auto t = v.time;
    const auto noise = [&v]
    {
        v.noise ^= v.noise << 13;
        v.noise ^= v.noise >> 17;
        v.noise ^= v.noise << 5;
        return static_cast<float>(v.noise) / 2147483648.0f - 1.0f;
    };
    const auto highPass = [&v](float x, float coefficient)
    {
        v.highPass = coefficient * (v.highPass + x - v.previous);
        v.previous = x;
        return v.highPass;
    };
    const auto metallic = [&v, dt](double scale)
    {
        // The six square oscillators of the TR-808 cymbal circuit.
        static constexpr double frequencies[] { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
        float sum = 0.0f;
        for (size_t index = 0; index < v.metal.size(); ++index)
        {
            v.metal[index] += frequencies[index] * scale * dt;
            v.metal[index] -= std::floor(v.metal[index]);
            sum += v.metal[index] < 0.5 ? 1.0f : -1.0f;
        }
        return sum / 6.0f;
    };

    float sample = 0.0f;
    double length = 1.0;
    switch (v.lane)
    {
        case kick:
        {
            v.phase += (48.0 + 120.0 * std::exp(-t / 0.03)) * dt;
            sample = static_cast<float>(std::sin(twoPi * v.phase) * std::exp(-t / 0.42)) + noise() * static_cast<float>(std::exp(-t / 0.002)) * 0.25f;
            length = 1.6;
            break;
        }
        case snare:
        {
            v.phase += 190.0 * dt;
            v.phase2 += 330.0 * dt;
            const auto tone = (std::sin(twoPi * v.phase) + 0.6 * std::sin(twoPi * v.phase2)) * std::exp(-t / 0.08) * 0.45;
            sample = static_cast<float>(tone) + highPass(noise(), 0.86f) * static_cast<float>(std::exp(-t / 0.17)) * 0.8f;
            length = 0.9;
            break;
        }
        case rim:
            sample = static_cast<float>(std::sin(twoPi * 1750.0 * t) * std::exp(-t / 0.01) * 0.6) + noise() * static_cast<float>(std::exp(-t / 0.004)) * 0.35f;
            length = 0.15;
            break;
        case clap:
        {
            const auto n = noise();
            v.lowPass += 0.35f * (n - v.lowPass);
            v.lowPass2 += 0.08f * (n - v.lowPass2);
            double envelope = 0.0;
            for (int burst = 0; burst < 3; ++burst)
                if (t >= burst * 0.011)
                    envelope = std::max(envelope, std::exp(-(t - burst * 0.011) / 0.005));
            if (t >= 0.022)
                envelope = std::max(envelope, 0.7 * std::exp(-(t - 0.022) / 0.13));
            sample = (v.lowPass - v.lowPass2) * static_cast<float>(envelope) * 2.2f;
            length = 0.9;
            break;
        }
        case closedHat:
        case pedalHat:
        case openHat:
        {
            const auto decay = v.lane == openHat ? 0.4 : v.lane == pedalHat ? 0.035 : 0.05;
            const auto source = metallic(1.0) * 0.7f + noise() * 0.3f;
            const auto filtered = highPass(source, 0.55f);
            v.lowPass += 0.6f * (filtered - v.lowPass);
            sample = (filtered - v.lowPass * 0.3f) * static_cast<float>(std::exp(-t / decay)) * 0.6f;
            length = decay * 7.0;
            break;
        }
        case lowTom:
        case midTom:
        case highTom:
        {
            const auto base = v.lane == lowTom ? 98.0 : v.lane == midTom ? 132.0 : 176.0;
            v.phase += base * (1.0 + 0.6 * std::exp(-t / 0.04)) * dt;
            sample = static_cast<float>(std::sin(twoPi * v.phase) * std::exp(-t / 0.3) * 0.85) + noise() * static_cast<float>(std::exp(-t / 0.004)) * 0.1f;
            length = 1.6;
            break;
        }
        case crash:
        {
            const auto source = noise() * 0.6f + metallic(2.2) * 0.4f;
            const auto attack = static_cast<float>(std::min(1.0, t / 0.002));
            sample = highPass(source, 0.7f) * static_cast<float>(std::exp(-t / 1.2)) * attack * 0.55f;
            length = 5.0;
            break;
        }
        case ride:
        {
            const auto source = metallic(3.1) * 0.5f + noise() * 0.2f;
            sample = highPass(source, 0.75f) * static_cast<float>(std::exp(-t / 0.9)) * 0.45f
                   + static_cast<float>(std::sin(twoPi * 3300.0 * t) * std::exp(-t / 0.25) * 0.15);
            length = 4.0;
            break;
        }
        default: break;
    }

    v.time += dt;
    if (v.choking)
    {
        v.fade -= static_cast<float>(1.0 / (0.006 * sampleRate));
        if (v.fade <= 0.0f)
            v.active = false;
    }
    if (v.time > length)
        v.active = false;
    return sample * v.gain * std::max(0.0f, v.fade);
}

void DrumSynth::render(const DrumEvent* events, int numEvents, float* left, float* right, int numSamples) noexcept
{
    static constexpr float pans[] { 0.0f, 0.0f, 0.1f, 0.0f, 0.25f, 0.25f, 0.25f, -0.35f, 0.0f, 0.35f, -0.4f, 0.4f };
    DrumEvent previews[16];
    const auto previewCount = takePreviews(previews, 16);
    for (int index = 0; index < previewCount; ++index)
        trigger(previews[index].note, previews[index].velocity);

    int next = 0;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (; next < numEvents && events[next].offset <= sample; ++next)
            if (events[next].velocity > 0)
                trigger(events[next].note, events[next].velocity);
        float l = 0.0f, r = 0.0f;
        for (auto& voice : voices)
            if (voice.active)
            {
                const auto value = renderVoice(voice);
                const auto pan = pans[voice.lane];
                l += value * std::min(1.0f, 1.0f - pan);
                r += value * std::min(1.0f, 1.0f + pan);
            }
        left[sample] = l * 0.8f;
        right[sample] = r * 0.8f;
    }
}
} // namespace daw
