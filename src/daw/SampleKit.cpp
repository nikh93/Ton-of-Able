#include "SampleKit.h"

#include <mutex>

#include <juce_audio_formats/juce_audio_formats.h>

namespace daw
{
namespace
{
int parseNote(const juce::String& value)
{
    const auto text = value.trim().toLowerCase();
    if (text.containsOnly("0123456789-"))
        return juce::jlimit(0, 127, text.getIntValue());
    // Note names such as c4, f#2 or eb3 (c4 = 60).
    static const int offsets[] { 9, 11, 0, 2, 4, 5, 7 }; // a b c d e f g
    if (text.isEmpty() || text[0] < 'a' || text[0] > 'g')
        return 0;
    auto note = offsets[text[0] - 'a'];
    int index = 1;
    if (index < text.length() && text[index] == '#') { ++note; ++index; }
    else if (index < text.length() && text[index] == 'b') { --note; ++index; }
    return juce::jlimit(0, 127, note + (text.substring(index).getIntValue() + 1) * 12);
}

void applyOpcode(SampleKit::Region& region, const juce::String& key, const juce::String& value)
{
    if (key == "key") { region.loKey = region.hiKey = parseNote(value); }
    else if (key == "lokey") region.loKey = parseNote(value);
    else if (key == "hikey") region.hiKey = parseNote(value);
    else if (key == "lovel") region.loVel = juce::jlimit(0, 127, value.getIntValue());
    else if (key == "hivel") region.hiVel = juce::jlimit(0, 127, value.getIntValue());
    else if (key == "lorand") region.loRand = juce::jlimit(0.0f, 1.0f, value.getFloatValue());
    else if (key == "hirand") region.hiRand = juce::jlimit(0.0f, 1.0f, value.getFloatValue());
    else if (key == "sample") region.sample = value.trim().replaceCharacter('\\', '/');
    else if (key == "pan") region.pan = juce::jlimit(-100.0f, 100.0f, value.getFloatValue());
    else if (key == "volume") region.volumeDb = value.getFloatValue();
    else if (key == "group") region.group = value.getIntValue();
    else if (key == "off_by" || key == "offby") region.offBy = value.getIntValue();
}

std::mutex cacheLock;
std::map<juce::String, std::weak_ptr<const SampleKit::Data>>& cache()
{
    static std::map<juce::String, std::weak_ptr<const SampleKit::Data>> instances;
    return instances;
}

std::shared_ptr<const SampleKit::Data> resampled(const juce::String& key, const SampleKit::Data& source, double rate)
{
    const auto cacheKey = key + "@" + juce::String(rate);
    std::lock_guard<std::mutex> lock(cacheLock);
    if (auto existing = cache()[cacheKey].lock())
        return existing;
    auto data = std::make_shared<SampleKit::Data>(source);
    data->sampleRate = rate;
    // Samples are shared between layers: convert each one once.
    std::map<const SampleKit::Sample*, std::shared_ptr<const SampleKit::Sample>> converted;
    for (auto& layers : data->notes)
        for (auto& layer : layers)
        {
            auto& slot = converted[layer.sample.get()];
            if (slot == nullptr)
            {
                const auto& input = *layer.sample;
                auto output = std::make_shared<SampleKit::Sample>();
                output->sampleRate = rate;
                const auto ratio = input.sampleRate / rate;
                const auto convert = [ratio](const std::vector<float>& from, std::vector<float>& to)
                {
                    if (from.empty() || std::abs(ratio - 1.0) < 1.0e-9)
                    {
                        to = from;
                        return;
                    }
                    const auto length = static_cast<int>(static_cast<double>(from.size()) / ratio);
                    to.resize(static_cast<size_t>(std::max(0, length)));
                    std::vector<float> padded(from);
                    padded.resize(padded.size() + 8, 0.0f); // the interpolator reads a little ahead
                    juce::LagrangeInterpolator interpolator;
                    interpolator.process(ratio, padded.data(), to.data(), length);
                };
                convert(input.left, output->left);
                convert(input.right, output->right);
                slot = output;
            }
            layer.sample = slot;
        }
    cache()[cacheKey] = data;
    return data;
}
} // namespace

std::vector<SampleKit::Region> SampleKit::parseSfz(const juce::String& source)
{
    std::vector<Region> regions;
    Region global, group, current;
    enum class Header { none, global, group, region } header = Header::none;
    const auto target = [&]() -> Region& { return header == Header::region ? current : header == Header::group ? group : global; };
    const auto flush = [&]
    {
        if (header == Header::region && current.sample.isNotEmpty())
            regions.push_back(current);
    };

    for (auto line : juce::StringArray::fromLines(source))
    {
        line = line.upToFirstOccurrenceOf("//", false, false).trim();
        // An opcode's value runs to the next opcode or header on the same
        // line, so sample paths may contain spaces.
        juce::String key, value;
        const auto finish = [&]
        {
            if (key.isNotEmpty())
                applyOpcode(target(), key, value.trim());
            key.clear();
            value.clear();
        };
        for (auto token : juce::StringArray::fromTokens(line, " \t", ""))
        {
            while (token.startsWithChar('<') && token.containsChar('>'))
            {
                finish();
                const auto name = token.substring(1, token.indexOfChar('>')).toLowerCase();
                token = token.fromFirstOccurrenceOf(">", false, false);
                flush();
                if (name == "region") { header = Header::region; current = group; }
                else if (name == "group") { header = Header::group; group = global; }
                else if (name == "global" || name == "master") { header = Header::global; global = Region(); group = global; }
                else header = Header::none; // <control>, <curve> ... are not needed
            }
            if (token.isEmpty())
                continue;
            if (const auto equals = token.indexOfChar('='); equals > 0)
            {
                finish();
                key = token.substring(0, equals).toLowerCase();
                value = token.substring(equals + 1);
            }
            else if (key.isNotEmpty())
                value << " " << token;
        }
        finish();
    }
    flush();
    return regions;
}

const std::vector<SampleKit::KitInfo>& SampleKit::bundledKits()
{
    // The drum machine lanes use General MIDI toms 45 / 47 / 50. The AVL kits
    // put the hi-hat swish on 48 and a crash choke on 50, so the toms are
    // mapped to the kits' own drums (high, mid, low).
    static const std::vector<KitInfo> kits {
        // SM Drums: deeply sampled stereo kit, 8-16 velocity layers and 3-4 round robins per drum.
        { "sm-drums", "SM Drums (Studio-Rock, Stereo)", "SM-Drums/SM_Drums.sfz", {} },
        { "avl-black-pearl", "Black Pearl (Pearl, Sabian)", "AVL-Drumkits/Black_Pearl_5pc.sfz", { { 50, 45 }, { 47, 47 }, { 45, 41 } } },
        { "avl-red-zeppelin", "Red Zeppelin (Ludwig, Zildjian)", "AVL-Drumkits/Red_Zeppelin_5pc.sfz", { { 50, 45 }, { 47, 41 }, { 45, 43 } } },
    };
    return kits;
}

const SampleKit::KitInfo* SampleKit::findKit(const juce::String& id)
{
    for (const auto& kit : bundledKits())
        if (id == kit.id)
            return &kit;
    return nullptr;
}

juce::ValueTree SampleKit::settingsFor(const juce::String& kitId)
{
    juce::ValueTree settings("Instrument");
    settings.setProperty("type", "kit", nullptr);
    settings.setProperty("kit", kitId, nullptr);
    return settings;
}

std::shared_ptr<SampleKit> SampleKit::load(const juce::File& kitsDirectory, const juce::String& kitId, juce::String& error)
{
    const auto* info = findKit(kitId);
    if (info == nullptr)
    {
        error = "Unbekanntes Kit: " + kitId;
        return nullptr;
    }
    const auto sfz = kitsDirectory.getChildFile(info->sfzFile);
    std::array<int, 128> map {};
    for (int note = 0; note < 128; ++note)
        map[static_cast<size_t>(note)] = note;
    for (const auto& [from, to] : info->noteMap)
        map[static_cast<size_t>(from)] = to;

    const auto key = sfz.getFullPathName();
    std::shared_ptr<const Data> original;
    {
        std::lock_guard<std::mutex> lock(cacheLock);
        original = cache()[key].lock();
    }
    if (original == nullptr)
    {
        if (! sfz.existsAsFile())
        {
            error = "Kit-Datei fehlt: " + sfz.getFullPathName();
            return nullptr;
        }
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        auto data = std::make_shared<Data>();
        std::map<juce::String, std::shared_ptr<const Sample>> samples;
        for (const auto& region : parseSfz(sfz.loadFileAsString()))
        {
            auto& sample = samples[region.sample];
            if (sample == nullptr)
            {
                const auto file = sfz.getParentDirectory().getChildFile(region.sample);
                std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                if (reader == nullptr)
                {
                    error = "Sample fehlt oder ist nicht lesbar: " + file.getFullPathName();
                    return nullptr;
                }
                auto decoded = std::make_shared<Sample>();
                decoded->sampleRate = reader->sampleRate;
                const auto length = static_cast<int>(reader->lengthInSamples);
                juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), length);
                reader->read(&buffer, 0, length, 0, true, true);
                const auto* first = buffer.getReadPointer(0);
                decoded->left.assign(first, first + length);
                if (buffer.getNumChannels() > 1)
                {
                    const auto* second = buffer.getReadPointer(1);
                    decoded->right.assign(second, second + length);
                }
                sample = decoded;
            }
            Layer layer;
            layer.loVel = region.loVel;
            layer.hiVel = region.hiVel;
            layer.loRand = region.loRand;
            layer.hiRand = region.hiRand;
            layer.sample = sample;
            const auto angle = (region.pan + 100.0f) / 200.0f * juce::MathConstants<float>::halfPi;
            const auto level = juce::Decibels::decibelsToGain(region.volumeDb);
            // Constant power, normalised so a centred region keeps unity gain.
            layer.gainLeft = level * std::cos(angle) * juce::MathConstants<float>::sqrt2;
            layer.gainRight = level * std::sin(angle) * juce::MathConstants<float>::sqrt2;
            layer.group = region.group;
            layer.offBy = region.offBy;
            for (int note = region.loKey; note <= region.hiKey; ++note)
                data->notes[static_cast<size_t>(note)].push_back(layer);
        }
        if (! samples.empty())
            data->sampleRate = samples.begin()->second->sampleRate;
        std::lock_guard<std::mutex> lock(cacheLock);
        cache()[key] = data;
        original = data;
    }
    return std::shared_ptr<SampleKit>(new SampleKit(sfz, info->name, map, original));
}

SampleKit::SampleKit(juce::File sfz, juce::String kitName, std::array<int, 128> map, std::shared_ptr<const Data> source)
    : sfzFile(std::move(sfz)), displayName(std::move(kitName)), noteMap(map), original(std::move(source))
{
}

std::shared_ptr<TrackInstrument> SampleKit::createOfflineCopy() const
{
    return std::shared_ptr<SampleKit>(new SampleKit(sfzFile, displayName, noteMap, original));
}

void SampleKit::prepare(double rate, int)
{
    sampleRate = rate;
    data = resampled(sfzFile.getFullPathName(), *original, rate);
    for (auto& voice : voices)
        voice = {};
}

void SampleKit::trigger(int note, int velocity) noexcept
{
    if (data == nullptr || ! juce::isPositiveAndBelow(note, 128))
        return;
    const auto& layers = data->notes[static_cast<size_t>(noteMap[static_cast<size_t>(note)])];
    // Round robins: each matching layer covers a slice of a random number.
    random = random * 1664525u + 1013904223u;
    const auto pick = static_cast<float>(random >> 8) / static_cast<float>(1u << 24);
    const Layer* chosen = nullptr;
    for (const auto& layer : layers)
        if (velocity >= layer.loVel && velocity <= layer.hiVel && (chosen == nullptr || (pick >= layer.loRand && pick < layer.hiRand)))
            chosen = &layer;
    if (chosen == nullptr)
        return;

    // Choke groups: an open hi-hat stops when the closed one is played, a crash when it is grabbed.
    if (chosen->group != 0)
        for (auto& voice : voices)
            if (voice.layer != nullptr && voice.layer->offBy == chosen->group)
                voice.choking = true;

    // Reuse a free voice, otherwise the oldest one.
    auto* target = &voices[0];
    for (auto& voice : voices)
    {
        if (voice.layer == nullptr)
        {
            target = &voice;
            break;
        }
        if (clock - voice.age > clock - target->age)
            target = &voice;
    }
    // Within a layer the level still follows the velocity a little, plus a
    // tiny random variation so repeated hits do not sound identical.
    const auto span = static_cast<float>(std::max(1, chosen->hiVel - chosen->loVel));
    const auto withinLayer = static_cast<float>(velocity - chosen->loVel) / span;
    random = random * 1664525u + 1013904223u;
    const auto humanise = 1.0f + (static_cast<float>(random >> 9) / static_cast<float>(1u << 23) - 0.5f) * 0.1f;
    target->layer = chosen;
    target->position = 0;
    target->gain = (0.78f + 0.22f * withinLayer) * humanise;
    target->fade = 1.0f;
    target->choking = false;
    target->age = clock++;
}

void SampleKit::render(const DrumEvent* events, int numEvents, float* left, float* right, int numSamples) noexcept
{
    DrumEvent previews[16];
    const auto previewCount = takePreviews(previews, 16);
    for (int index = 0; index < previewCount; ++index)
        trigger(previews[index].note, previews[index].velocity);

    std::fill(left, left + numSamples, 0.0f);
    std::fill(right, right + numSamples, 0.0f);
    const auto chokeStep = static_cast<float>(1.0 / (0.008 * sampleRate)); // 8 ms fade
    constexpr float outputGain = 0.3f; // the samples are normalised hot: a full kit hit stays below 0 dBFS

    int next = 0;
    int start = 0;
    while (start < numSamples)
    {
        // Render up to the next event, then trigger it.
        for (; next < numEvents && events[next].offset <= start; ++next)
            if (events[next].velocity > 0)
                trigger(events[next].note, events[next].velocity);
        const auto end = next < numEvents ? std::min(numSamples, std::max(start + 1, events[next].offset)) : numSamples;
        for (auto& voice : voices)
        {
            if (voice.layer == nullptr)
                continue;
            const auto& samples = voice.layer->sample->left;
            const auto& rightSamples = voice.layer->sample->right.empty() ? samples : voice.layer->sample->right;
            const auto gainLeft = voice.gain * voice.layer->gainLeft * outputGain;
            const auto gainRight = voice.gain * voice.layer->gainRight * outputGain;
            for (int index = start; index < end; ++index)
            {
                if (voice.position >= samples.size() || voice.fade <= 0.0f)
                {
                    voice.layer = nullptr;
                    break;
                }
                left[index] += samples[voice.position] * voice.fade * gainLeft;
                right[index] += rightSamples[voice.position] * voice.fade * gainRight;
                ++voice.position;
                if (voice.choking)
                    voice.fade -= chokeStep;
            }
        }
        start = end;
    }
}
} // namespace daw
