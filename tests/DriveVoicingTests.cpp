#include <juce_core/juce_core.h>
#include "audio/effects/DriveProcessor.h"
#include "presets/PresetManager.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
using Drive = nam::effects::DriveProcessor;
constexpr double rate = 48000.0;
constexpr int count = 8192;

std::vector<float> render(Drive::DriveType type, float amount, float amplitude,
                          float sustain = .65f, float wet = 1.f, bool pluck = false,
                          int blockSize = 128)
{
    Drive drive;
    drive.prepare({rate, static_cast<juce::uint32>(blockSize), 1});
    drive.setType(type); drive.setBypassed(false); drive.setDrive(amount);
    drive.setTone(.45f); drive.setWarmth(.7f); drive.setPresence(.4f);
    drive.setSustain(sustain); drive.setMix(wet); drive.setLevel(1.f);
    juce::AudioBuffer<float> block(1, blockSize);
    // Let parameter smoothing finish before the measured note starts.
    for (int i = 0; i < 4800; i += blockSize) { block.clear(); drive.process(block); }
    const int samples = pluck ? 96000 : count * 2;
    std::vector<float> result(static_cast<size_t>(samples));
    for (int start = 0; start < samples; start += blockSize)
    {
        const int n = juce::jmin(blockSize, samples - start);
        for (int i = 0; i < n; ++i)
        {
            const float t = static_cast<float>((start+i)/rate);
            const float envelope = pluck ? std::exp(-2.4f*t) : 1.f;
            block.setSample(0,i,amplitude*envelope*std::sin(juce::MathConstants<float>::twoPi*375.f*t));
        }
        auto* data = block.getWritePointer(0);
        juce::AudioBuffer<float> view(&data,1,n);
        drive.process(view);
        std::copy(data,data+n,result.begin()+start);
    }
    return result;
}

double rms(const std::vector<float>& signal, int start, int n)
{
    double sum = 0;
    for (int i=start; i<start+n; ++i) sum += signal[i]*signal[i];
    return std::sqrt(sum/n);
}

double harmonics(const std::vector<float>& signal)
{
    double sine=0, cosine=0, energy=0, dc=0;
    for (int i=0;i<count;++i)
    {
        const double x=signal[count+i], phase=juce::MathConstants<double>::twoPi*375.0*i/rate;
        sine+=x*std::sin(phase); cosine+=x*std::cos(phase); energy+=x*x; dc+=x;
    }
    const double fundamental=2.0*(sine*sine+cosine*cosine)/(count*count);
    return std::sqrt(juce::jmax(0.0,energy/count - fundamental - dc*dc/(count*count))/juce::jmax(1.e-15,fundamental));
}

class DriveVoicingTests final : public juce::UnitTest
{
public:
    DriveVoicingTests() : UnitTest("Drive musical behaviour", "NAM Test Program") {}
    void runTest() override
    {
        beginTest("Warm drive audibly saturates quiet signals and follows picking strength");
        const auto low = render(Drive::DriveType::WarmOverdrive,.05f,.025f);
        const auto quiet = render(Drive::DriveType::WarmOverdrive,.48f,.025f);
        const auto strong = render(Drive::DriveType::WarmOverdrive,.48f,.15f);
        const double lowTHD=harmonics(low), quietTHD=harmonics(quiet), strongTHD=harmonics(strong);
        std::cout << "Warm drive harmonic ratios low-gain/quiet/strong: " << lowTHD << "/" << quietTHD << "/" << strongTHD << '\n';
        expect(quietTHD > .015, "Quiet picking must produce measurable overdrive");
        expect(quietTHD > lowTHD*1.5, "Drive must affect saturation, not just loudness");
        expect(strongTHD > quietTHD*1.4, "Strong picking must saturate more");
        expect(rms(strong,count,count) > rms(quiet,count,count)*1.15, "Retain dynamic contrast");

        beginTest("Lead sustain improves the late-note to attack ratio");
        const auto shortNote=render(Drive::DriveType::SingingLead,.52f,.15f,0.f,1.f,true);
        const auto longNote=render(Drive::DriveType::SingingLead,.52f,.15f,1.f,1.f,true);
        const double shortRatio=rms(shortNote,72000,12000)/rms(shortNote,2400,12000);
        const double longRatio=rms(longNote,72000,12000)/rms(longNote,2400,12000);
        std::cout << "Lead late/early RMS sustain 0/1: " << shortRatio << "/" << longRatio << '\n';
        expect(longRatio > shortRatio*1.2);

        beginTest("All voices settle to silence without DC or a zero-crossing step");
        for (int voice=0;voice<4;++voice)
        {
            const auto silent=render(static_cast<Drive::DriveType>(voice),.8f,0.f);
            expect(rms(silent,count,count)<1.e-7);
        }
        double mean=0;
        for (int i=count;i<count*2;++i) mean+=strong[i]/count;
        expect(std::abs(mean)<.0001, "Asymmetric clipping must not feed DC into NAM");

        beginTest("Dry and wet share converter latency at partial mix");
        const auto dry=render(Drive::DriveType::WarmOverdrive,.48f,.1f,.65f,0.f);
        const auto wet=render(Drive::DriveType::WarmOverdrive,.48f,.1f);
        const auto half=render(Drive::DriveType::WarmOverdrive,.48f,.1f,.65f,.5f);
        double error=0;
        for (int i=count;i<count*2;++i) error=juce::jmax(error,std::abs(static_cast<double>(half[i]-.5f*(dry[i]+wet[i]))));
        expect(error<1.e-5);

        beginTest("Drive has consistent output for different callback lengths");
        const auto small=render(Drive::DriveType::SingingLead,.52f,.1f,.65f,1.f,false,32);
        const auto large=render(Drive::DriveType::SingingLead,.52f,.1f,.65f,1.f,false,512);
        double blockError=0;
        for (int i=count;i<count*2;++i) blockError=juce::jmax(blockError,std::abs(static_cast<double>(small[i]-large[i])));
        expect(blockError<1.e-5);

        beginTest("AMP PUSH roundtrips in presets and old presets default to unity");
        SoundState state; state.namInputTrimDb=5.5f;
        auto tree=state.toValueTree(); auto restored=SoundState::fromValueTree(tree);
        expect(restored.has_value()); if (restored) expectEquals(restored->namInputTrimDb,5.5f);
        tree.removeProperty("namInputTrimDb",nullptr); restored=SoundState::fromValueTree(tree);
        expect(restored.has_value()); if (restored) expectEquals(restored->namInputTrimDb,0.f);

        beginTest("VST3 states survive preset JSON and A/B without losing plugin identity");
        state.preVst3State=juce::ValueTree("Vst3Slot");
        state.preVst3State.setProperty("descriptionXml","BYOD identity",nullptr);
        state.preVst3State.setProperty("stateBase64","opaque plugin state",nullptr);
        state.postVst3State=juce::ValueTree("Vst3Slot");
        state.postVst3State.setProperty("descriptionXml","Supermassive identity",nullptr);
        state.postVst3State.setProperty("bypassed",true,nullptr);
        const auto json=PresetManager::valueTreeToJson(state.toValueTree());
        const auto decoded=PresetManager::valueTreeFromJson(json);
        expect(decoded.has_value());
        if (decoded)
        {
            restored=SoundState::fromValueTree(*decoded);
            expect(restored.has_value());
            if (restored) { expect(restored->preVst3State.isEquivalentTo(state.preVst3State)); expect(restored->postVst3State.isEquivalentTo(state.postVst3State)); }
        }
        ABState ab; ab.a=state; ab.b=SoundState{};
        const auto recovered=ABState::fromValueTree(ab.toValueTree());
        expect(recovered.has_value());
        if (recovered) { expect(recovered->a.preVst3State.isEquivalentTo(state.preVst3State)); expect(!recovered->b.preVst3State.isValid()); }
    }
};
DriveVoicingTests driveVoicingTests;

class NamPedalStateTests final : public juce::UnitTest
{
public:
    NamPedalStateTests() : UnitTest("NAM pedal preset state", "NAM Test Program") {}

    void runTest() override
    {
        const auto jsonRoundtrip = [](const juce::ValueTree& tree)
        {
            // Exercise the actual JSON text representation, including path escaping.
            return PresetManager::valueTreeFromJson(juce::JSON::parse(
                juce::JSON::toString(PresetManager::valueTreeToJson(tree))));
        };
        const auto expectPedal = [this](const SoundState& actual, const SoundState& expected)
        {
            expectEquals(actual.namPedalPath, expected.namPedalPath);
            expect(actual.namPedalBypassed == expected.namPedalBypassed);
            expectEquals(actual.namPedalInputDb, expected.namPedalInputDb);
            expectEquals(actual.namPedalOutputDb, expected.namPedalOutputDb);
        };
        const auto removePedal = [](juce::ValueTree tree)
        {
            for (const auto* key : { "namPedalPath", "namPedalBypassed", "namPedalInputDb", "namPedalOutputDb" })
                tree.removeProperty(key, nullptr);
        };
        SoundState pedal;
        pedal.namPedalPath = "D:\\Amp Modeler\\resources\\pedals\\Warm Lead.nam";
        pedal.namPedalBypassed = false;
        pedal.namPedalInputDb = 7.5f;
        pedal.namPedalOutputDb = -4.25f;

        beginTest("NAM pedal survives sound JSON text and preset create/replace/save/load");
        const auto soundJson = jsonRoundtrip(pedal.toValueTree());
        expect(soundJson.has_value());
        if (soundJson)
        {
            const auto restored = SoundState::fromValueTree(*soundJson);
            expect(restored.has_value());
            if (restored) expectPedal(*restored, pedal);
        }
        PresetManager presets;
        const auto id = presets.create("Pedal lead", pedal);
        expect(id.has_value());
        if (id)
        {
            const auto* created = presets.find(*id);
            expect(created != nullptr);
            if (created) expectPedal(created->sound, pedal);
            auto replacement = pedal;
            replacement.namPedalPath = "D:\\Amp Modeler\\resources\\pedals\\Other.nam";
            replacement.namPedalBypassed = true;
            replacement.namPedalInputDb = -12.f;
            replacement.namPedalOutputDb = 3.f;
            expect(presets.replace(*id, replacement));
            const auto presetJson = jsonRoundtrip(presets.toValueTree());
            expect(presetJson.has_value());
            if (presetJson)
            {
                PresetManager loaded;
                expect(loaded.loadValueTree(*presetJson));
                const auto* saved = loaded.find(*id);
                expect(saved != nullptr);
                if (saved) expectPedal(saved->sound, replacement);
            }
        }

        beginTest("A/B JSON preserves distinct pedal settings and the active slot");
        ABState ab;
        ab.a = pedal;
        ab.b.namPedalPath = "B-pedal.nam";
        ab.b.namPedalBypassed = true;
        ab.b.namPedalInputDb = -24.f;
        ab.b.namPedalOutputDb = 24.f;
        ab.usingA = false;
        const auto abJson = jsonRoundtrip(ab.toValueTree());
        expect(abJson.has_value());
        if (abJson)
        {
            const auto restored = ABState::fromValueTree(*abJson);
            expect(restored.has_value());
            if (restored)
            {
                expect(! restored->usingA);
                expectPedal(restored->a, ab.a);
                expectPedal(restored->b, ab.b);
            }
        }

        beginTest("Legacy sounds, presets and A/B slots default to an empty bypassed pedal at unity");
        const auto defaults = SoundState::defaults();
        expect(defaults.namPedalPath.isEmpty());
        expect(defaults.namPedalBypassed);
        expectEquals(defaults.namPedalInputDb, 0.f);
        expectEquals(defaults.namPedalOutputDb, 0.f);
        auto legacySound = pedal.toValueTree();
        removePedal(legacySound);
        const auto oldSoundJson = jsonRoundtrip(legacySound);
        expect(oldSoundJson.has_value());
        if (oldSoundJson)
        {
            const auto restored = SoundState::fromValueTree(*oldSoundJson);
            expect(restored.has_value());
            if (restored) expectPedal(*restored, defaults);
        }
        auto legacyPresets = presets.toValueTree();
        removePedal(legacyPresets.getChild(0).getChildWithName("soundState"));
        const auto oldPresetJson = jsonRoundtrip(legacyPresets);
        expect(oldPresetJson.has_value());
        if (oldPresetJson && id)
        {
            PresetManager loaded;
            expect(loaded.loadValueTree(*oldPresetJson));
            const auto* saved = loaded.find(*id);
            expect(saved != nullptr);
            if (saved) expectPedal(saved->sound, defaults);
        }
        auto legacyAB = ab.toValueTree();
        removePedal(legacyAB.getChild(0));
        removePedal(legacyAB.getChild(1));
        const auto oldABJson = jsonRoundtrip(legacyAB);
        expect(oldABJson.has_value());
        if (oldABJson)
        {
            const auto restored = ABState::fromValueTree(*oldABJson);
            expect(restored.has_value());
            if (restored)
            {
                expectPedal(restored->a, defaults);
                expectPedal(restored->b, defaults);
                expect(! restored->usingA);
            }
        }

        beginTest("NAM pedal trims clamp on reading and writing; non-finite values become unity");
        struct TrimCase { double value; float expected; };
        const TrimCase cases[] {
            { -100.0, -24.f }, { -24.0, -24.f }, { 6.5, 6.5f },
            { 24.0, 24.f }, { 100.0, 24.f },
            { std::numeric_limits<double>::quiet_NaN(), 0.f },
            { std::numeric_limits<double>::infinity(), 0.f },
            { -std::numeric_limits<double>::infinity(), 0.f }
        };
        for (const auto& test : cases)
        {
            auto tree = pedal.toValueTree();
            tree.setProperty("namPedalInputDb", test.value, nullptr);
            tree.setProperty("namPedalOutputDb", test.value, nullptr);
            const auto restored = SoundState::fromValueTree(tree);
            expect(restored.has_value());
            if (restored)
            {
                expectEquals(restored->namPedalInputDb, test.expected);
                expectEquals(restored->namPedalOutputDb, test.expected);
            }
            auto unsafe = pedal;
            unsafe.namPedalInputDb = static_cast<float>(test.value);
            unsafe.namPedalOutputDb = static_cast<float>(test.value);
            const auto safeTree = unsafe.toValueTree();
            expectEquals(static_cast<float>(safeTree.getProperty("namPedalInputDb")), test.expected);
            expectEquals(static_cast<float>(safeTree.getProperty("namPedalOutputDb")), test.expected);
            const auto safeJson = jsonRoundtrip(safeTree);
            expect(safeJson.has_value());
            if (safeJson)
            {
                const auto saved = SoundState::fromValueTree(*safeJson);
                expect(saved.has_value());
                if (saved)
                {
                    expectEquals(saved->namPedalInputDb, test.expected);
                    expectEquals(saved->namPedalOutputDb, test.expected);
                }
            }
        }

        beginTest("Internal effects edits retain the live NAM pedal without losing the knob edit");
        SoundState rackSnapshot;
        rackSnapshot.namPedalPath = "stale-pedal.nam";
        rackSnapshot.driveAmount = .79f;
        rackSnapshot.reverbMix = .43f;
        rackSnapshot.retainAmpFrom(pedal);
        expectPedal(rackSnapshot, pedal);
        expectEquals(rackSnapshot.driveAmount, .79f);
        expectEquals(rackSnapshot.reverbMix, .43f);
        rackSnapshot.retainAmpFrom(defaults);
        expectPedal(rackSnapshot, defaults);
    }
};
NamPedalStateTests namPedalStateTests;
}
