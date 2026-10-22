#include "PresetManager.h"
#include <cmath>

namespace
{
constexpr auto soundType = "soundState", presetType = "preset", rootType = "presets", abType = "abState";
#define SOUND_FIELDS(X) \
 X(namId) X(irId) X(inputGainDb) X(outputGainDb) X(bassDb) X(midDb) X(trebleDb) X(namBypassed) X(irBypassed) \
 X(autoWahBypassed) X(chorusBypassed) X(delayBypassed) X(reverbBypassed) X(autoWahSensitivity) X(autoWahAttackMs) X(autoWahReleaseMs) X(autoWahMinimumHz) X(autoWahMaximumHz) X(autoWahQ) X(autoWahMix) X(autoWahUpward) \
 X(chorusRateHz) X(chorusDepthMs) X(chorusDelayMs) X(chorusFeedback) X(chorusMix) X(delayTimeMs) X(delayFeedback) X(delayMix) X(delayLowCutHz) X(delayHighCutHz) \
 X(reverbSize) X(reverbDecay) X(reverbDamping) X(reverbPreDelayMs) X(reverbMix) X(reverbLowCutHz) X(reverbHighCutHz)

bool requireType(const juce::ValueTree& tree, const juce::Identifier& type, juce::String* error)
{ if (tree.hasType(type)) return true; if (error != nullptr) *error = "Unexpected state document type."; return false; }

float safeNamPedalDb(double value)
{
    return std::isfinite(value) ? static_cast<float>(juce::jlimit(-24.0, 24.0, value)) : 0.0f;
}
}

juce::ValueTree SoundState::toValueTree() const
{
    juce::ValueTree tree { soundType };
#define WRITE_FIELD(name) tree.setProperty(#name, name, nullptr);
    SOUND_FIELDS(WRITE_FIELD)
#undef WRITE_FIELD
    tree.setProperty("ampId", ampId, nullptr);
    tree.setProperty("rigId", rigId, nullptr);
    tree.setProperty("configurationId", configurationId, nullptr);
    tree.setProperty("namInputTrimDb", namInputTrimDb, nullptr);
    tree.setProperty("namPedalPath", namPedalPath, nullptr);
    tree.setProperty("namPedalBypassed", namPedalBypassed, nullptr);
    tree.setProperty("namPedalInputDb", safeNamPedalDb(namPedalInputDb), nullptr);
    tree.setProperty("namPedalOutputDb", safeNamPedalDb(namPedalOutputDb), nullptr);
    for (const auto& item : { std::pair<const char*, juce::ValueTree>{"preVst3", preVst3State}, {"postVst3", postVst3State} })
    {
        juce::ValueTree wrapper(item.first);
        if (item.second.isValid()) wrapper.addChild(item.second.createCopy(), -1, nullptr);
        tree.addChild(wrapper, -1, nullptr);
    }
    tree.setProperty("graphicEqBypassed", graphicEqBypassed, nullptr);
    juce::StringArray bands;
    for (const auto band : graphicEqDb) bands.add(juce::String(band, 2));
    tree.setProperty("graphicEqDb", bands.joinIntoString(","), nullptr);
    tree.setProperty("compressorBypassed", compressorBypassed, nullptr);
    tree.setProperty("compressorPreAmp", compressorPreAmp, nullptr);
    tree.setProperty("compressorThresholdDb", compressorThresholdDb, nullptr);
    tree.setProperty("compressorRatio", compressorRatio, nullptr);
    tree.setProperty("compressorAttackMs", compressorAttackMs, nullptr);
    tree.setProperty("compressorReleaseMs", compressorReleaseMs, nullptr);
    tree.setProperty("compressorKneeDb", compressorKneeDb, nullptr);
    tree.setProperty("compressorMakeupDb", compressorMakeupDb, nullptr);
    tree.setProperty("compressorMix", compressorMix, nullptr);
    tree.setProperty("pitchBypassed", pitchBypassed, nullptr);
    tree.setProperty("pitchSemitones", pitchSemitones, nullptr);
    tree.setProperty("pitchMix", pitchMix, nullptr);
    tree.setProperty("pitchFormantPreserved", pitchFormantPreserved, nullptr);
    tree.setProperty("pitchHighQuality", pitchHighQuality, nullptr);
    tree.setProperty("driveBypassed", driveBypassed, nullptr); tree.setProperty("driveType", driveType, nullptr);
    tree.setProperty("driveAmount", driveAmount, nullptr); tree.setProperty("driveTone", driveTone, nullptr); tree.setProperty("driveWarmth", driveWarmth, nullptr); tree.setProperty("drivePresence", drivePresence, nullptr); tree.setProperty("driveSustain", driveSustain, nullptr); tree.setProperty("driveBias", driveBias, nullptr); tree.setProperty("driveLevel", driveLevel, nullptr); tree.setProperty("driveMix", driveMix, nullptr);
    return tree;
}

std::optional<SoundState> SoundState::fromValueTree(const juce::ValueTree& tree, juce::String* error)
{
    if (! requireType(tree, soundType, error)) return {};
    SoundState result;
#define REQUIRE_FIELD(name) if (! tree.hasProperty(#name)) { if (error != nullptr) *error = "Missing sound property: " #name; return {}; }
    SOUND_FIELDS(REQUIRE_FIELD)
#undef REQUIRE_FIELD
    result.namId = tree.getProperty("namId").toString(); result.irId = tree.getProperty("irId").toString();
    result.ampId = tree.getProperty("ampId", {}).toString(); result.configurationId = tree.getProperty("configurationId", {}).toString();
    result.driveBypassed = static_cast<bool>(tree.getProperty("driveBypassed", true)); result.driveType = static_cast<int>(tree.getProperty("driveType", 0));
    result.namInputTrimDb = juce::jlimit(-18.f, 18.f, static_cast<float>(tree.getProperty("namInputTrimDb", 0.f)));
    // Optional properties keep presets saved before the NAM pedal compatible.
    result.rigId = tree.getProperty("rigId", juce::String()).toString();
    result.namPedalPath = tree.getProperty("namPedalPath", juce::String()).toString();
    result.namPedalBypassed = static_cast<bool>(tree.getProperty("namPedalBypassed", true));
    result.namPedalInputDb = safeNamPedalDb(static_cast<double>(tree.getProperty("namPedalInputDb", 0.0)));
    result.namPedalOutputDb = safeNamPedalDb(static_cast<double>(tree.getProperty("namPedalOutputDb", 0.0)));
    result.graphicEqBypassed = static_cast<bool>(tree.getProperty("graphicEqBypassed", true));
    const auto bands = juce::StringArray::fromTokens(tree.getProperty("graphicEqDb", juce::String()).toString(), ",", {});
    for (int band = 0; band < static_cast<int>(result.graphicEqDb.size()) && band < bands.size(); ++band)
    {
        const auto value = bands[band].getFloatValue();
        result.graphicEqDb[static_cast<size_t>(band)] = std::isfinite(value) ? juce::jlimit(-12.0f, 12.0f, value) : 0.0f;
    }
    // Optional: presets saved before the compressor load with it switched off.
    const auto number = [&tree](const char* name, float fallback, float low, float high)
    {
        const auto value = static_cast<float>(static_cast<double>(tree.getProperty(name, fallback)));
        return std::isfinite(value) ? juce::jlimit(low, high, value) : fallback;
    };
    result.compressorBypassed = static_cast<bool>(tree.getProperty("compressorBypassed", true));
    result.compressorPreAmp = static_cast<bool>(tree.getProperty("compressorPreAmp", false));
    result.compressorThresholdDb = number("compressorThresholdDb", -18.0f, -60.0f, 0.0f);
    result.compressorRatio = number("compressorRatio", 4.0f, 1.0f, 20.0f);
    result.compressorAttackMs = number("compressorAttackMs", 10.0f, 0.1f, 200.0f);
    result.compressorReleaseMs = number("compressorReleaseMs", 150.0f, 5.0f, 2000.0f);
    result.compressorKneeDb = number("compressorKneeDb", 6.0f, 0.0f, 18.0f);
    result.compressorMakeupDb = number("compressorMakeupDb", 0.0f, 0.0f, 24.0f);
    result.compressorMix = number("compressorMix", 1.0f, 0.0f, 1.0f);
    result.pitchBypassed = static_cast<bool>(tree.getProperty("pitchBypassed", true));
    result.pitchSemitones = juce::jlimit(-24.0f, 24.0f, static_cast<float>(tree.getProperty("pitchSemitones", -12.0f)));
    result.pitchMix = juce::jlimit(0.0f, 1.0f, static_cast<float>(tree.getProperty("pitchMix", 1.0f)));
    result.pitchFormantPreserved = static_cast<bool>(tree.getProperty("pitchFormantPreserved", false));
    result.pitchHighQuality = static_cast<bool>(tree.getProperty("pitchHighQuality", false));
    result.preVst3State = tree.getChildWithName("preVst3").getChild(0).createCopy();
    result.postVst3State = tree.getChildWithName("postVst3").getChild(0).createCopy();
    result.driveAmount = static_cast<float>(tree.getProperty("driveAmount", .35f)); result.driveTone = static_cast<float>(tree.getProperty("driveTone", .5f)); result.driveWarmth = static_cast<float>(tree.getProperty("driveWarmth", .5f)); result.drivePresence = static_cast<float>(tree.getProperty("drivePresence", .5f)); result.driveSustain = static_cast<float>(tree.getProperty("driveSustain", .5f)); result.driveBias = static_cast<float>(tree.getProperty("driveBias", .35f)); result.driveLevel = static_cast<float>(tree.getProperty("driveLevel", .8f)); result.driveMix = static_cast<float>(tree.getProperty("driveMix", 1.f));
#define READ_NUMERIC_FIELD(name) result.name = static_cast<decltype(result.name)>(tree.getProperty(#name));
    READ_NUMERIC_FIELD(inputGainDb) READ_NUMERIC_FIELD(outputGainDb) READ_NUMERIC_FIELD(bassDb) READ_NUMERIC_FIELD(midDb) READ_NUMERIC_FIELD(trebleDb)
    READ_NUMERIC_FIELD(namBypassed) READ_NUMERIC_FIELD(irBypassed) READ_NUMERIC_FIELD(autoWahBypassed) READ_NUMERIC_FIELD(chorusBypassed) READ_NUMERIC_FIELD(delayBypassed) READ_NUMERIC_FIELD(reverbBypassed)
    READ_NUMERIC_FIELD(autoWahSensitivity) READ_NUMERIC_FIELD(autoWahAttackMs) READ_NUMERIC_FIELD(autoWahReleaseMs) READ_NUMERIC_FIELD(autoWahMinimumHz) READ_NUMERIC_FIELD(autoWahMaximumHz) READ_NUMERIC_FIELD(autoWahQ) READ_NUMERIC_FIELD(autoWahMix) READ_NUMERIC_FIELD(autoWahUpward)
    READ_NUMERIC_FIELD(chorusRateHz) READ_NUMERIC_FIELD(chorusDepthMs) READ_NUMERIC_FIELD(chorusDelayMs) READ_NUMERIC_FIELD(chorusFeedback) READ_NUMERIC_FIELD(chorusMix)
    READ_NUMERIC_FIELD(delayTimeMs) READ_NUMERIC_FIELD(delayFeedback) READ_NUMERIC_FIELD(delayMix) READ_NUMERIC_FIELD(delayLowCutHz) READ_NUMERIC_FIELD(delayHighCutHz)
    READ_NUMERIC_FIELD(reverbSize) READ_NUMERIC_FIELD(reverbDecay) READ_NUMERIC_FIELD(reverbDamping) READ_NUMERIC_FIELD(reverbPreDelayMs) READ_NUMERIC_FIELD(reverbMix) READ_NUMERIC_FIELD(reverbLowCutHz) READ_NUMERIC_FIELD(reverbHighCutHz)
#undef READ_NUMERIC_FIELD
    return result;
}

juce::ValueTree Preset::toValueTree() const
{
    juce::ValueTree tree { presetType }; tree.setProperty("id", id.toString(), nullptr); tree.setProperty("name", name, nullptr);
    tree.setProperty("createdMs", created.toMilliseconds(), nullptr); tree.setProperty("modifiedMs", modified.toMilliseconds(), nullptr);
    tree.addChild(sound.toValueTree(), -1, nullptr); return tree;
}
std::optional<Preset> Preset::fromValueTree(const juce::ValueTree& tree, juce::String* error)
{
    if (! requireType(tree, presetType, error) || ! tree.hasProperty("id") || ! tree.hasProperty("name")) { if (error != nullptr) *error = "Invalid preset."; return {}; }
    auto sound = SoundState::fromValueTree(tree.getChildWithName(soundType), error); if (! sound) return {};
    return Preset { juce::Uuid(tree.getProperty("id").toString()), tree.getProperty("name").toString(), *sound,
                    juce::Time(static_cast<juce::int64>(tree.getProperty("createdMs", 0))), juce::Time(static_cast<juce::int64>(tree.getProperty("modifiedMs", 0))) };
}

juce::ValueTree ABState::toValueTree() const
{ juce::ValueTree tree { abType }; tree.setProperty("usingA", usingA, nullptr); auto first = a.toValueTree(); first.setProperty("slot", "a", nullptr); auto second = b.toValueTree(); second.setProperty("slot", "b", nullptr); tree.addChild(first, -1, nullptr); tree.addChild(second, -1, nullptr); return tree; }
std::optional<ABState> ABState::fromValueTree(const juce::ValueTree& tree, juce::String* error)
{
    if (! requireType(tree, abType, error)) return {}; juce::ValueTree aTree, bTree; for (int i = 0; i < tree.getNumChildren(); ++i) { auto child = tree.getChild(i); if (child.getProperty("slot").toString() == "a") aTree = child; else if (child.getProperty("slot").toString() == "b") bTree = child; }
    auto a = SoundState::fromValueTree(aTree, error), b = SoundState::fromValueTree(bTree, error); if (! a || ! b) return {}; return ABState { *a, *b, static_cast<bool>(tree.getProperty("usingA", true)) };
}

const Preset* PresetManager::find(const juce::Uuid& id) const noexcept { for (const auto& p : entries) if (p.id == id) return &p; return nullptr; }
std::optional<juce::Uuid> PresetManager::create(juce::String name, const SoundState& sound)
{ name = name.trim(); if (name.isEmpty()) return {}; const auto now = juce::Time::getCurrentTime(); Preset p { juce::Uuid(), name, sound, now, now }; entries.add(p); return p.id; }
bool PresetManager::replace(const juce::Uuid& id, const SoundState& sound) { for (auto& p : entries) if (p.id == id) { p.sound = sound; p.modified = juce::Time::getCurrentTime(); return true; } return false; }
bool PresetManager::rename(const juce::Uuid& id, juce::String name) { name = name.trim(); if (name.isEmpty()) return false; for (auto& p : entries) if (p.id == id) { p.name = name; p.modified = juce::Time::getCurrentTime(); return true; } return false; }
bool PresetManager::erase(const juce::Uuid& id) { for (int i = 0; i < entries.size(); ++i) if (entries.getReference(i).id == id) { entries.remove(i); return true; } return false; }
juce::ValueTree PresetManager::toValueTree() const { juce::ValueTree root { rootType }; root.setProperty("version", 1, nullptr); for (const auto& p : entries) root.addChild(p.toValueTree(), -1, nullptr); return root; }
bool PresetManager::loadValueTree(const juce::ValueTree& root, juce::String* error)
{ if (! requireType(root, rootType, error)) return false; juce::Array<Preset> parsed; for (int i = 0; i < root.getNumChildren(); ++i) { auto p = Preset::fromValueTree(root.getChild(i), error); if (! p) return false; parsed.add(*p); } entries = std::move(parsed); return true; }

juce::var PresetManager::valueTreeToJson(const juce::ValueTree& tree)
{
    auto* object = new juce::DynamicObject(); object->setProperty("type", tree.getType().toString());
    auto* properties = new juce::DynamicObject(); for (int i = 0; i < tree.getNumProperties(); ++i) { const auto key = tree.getPropertyName(i); properties->setProperty(key, tree.getProperty(key)); } object->setProperty("properties", juce::var(properties));
    juce::Array<juce::var> children; for (int i = 0; i < tree.getNumChildren(); ++i) children.add(valueTreeToJson(tree.getChild(i))); object->setProperty("children", children); return juce::var(object);
}
std::optional<juce::ValueTree> PresetManager::valueTreeFromJson(const juce::var& json, juce::String* error)
{
    auto* object = json.getDynamicObject(); if (object == nullptr || ! object->hasProperty("type")) { if (error != nullptr) *error = "Invalid ValueTree JSON."; return {}; }
    juce::ValueTree tree { object->getProperty("type").toString() }; if (auto* props = object->getProperty("properties").getDynamicObject()) for (int i = 0; i < props->getProperties().size(); ++i) { const auto key = props->getProperties().getName(i); tree.setProperty(key, props->getProperty(key), nullptr); }
    if (auto* children = object->getProperty("children").getArray()) for (const auto& child : *children) { auto result = valueTreeFromJson(child, error); if (! result) return {}; tree.addChild(*result, -1, nullptr); } return tree;
}
