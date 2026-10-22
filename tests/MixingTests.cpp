#include <cmath>
#include <set>

#include <juce_core/juce_core.h>

#include "../src/audio/effects/CompressorProcessor.h"
#include "../src/audio/effects/WahPedalProcessor.h"
#include "../src/presets/PresetManager.h"
#include "../src/daw/Arrangement.h"
#include "../src/daw/ArrangementEngine.h"
#include "../src/ui/shortcuts/Shortcuts.h"

namespace
{
using namespace daw;

AudioDataPtr makeConstant(float value, int length, double rate = 48000.0)
{
    auto data = std::make_shared<AudioData>();
    data->sampleRate = rate;
    data->samples.setSize(1, length);
    for (int sample = 0; sample < length; ++sample)
        data->samples.setSample(0, sample, value);
    data->computePeaks();
    return data;
}

// A chain that halves the signal and counts its calls.
struct HalfProcessor final : TrackProcessor
{
    void prepare(double, int) override {}
    void process(float* mono, float* left, float* right, int numSamples) noexcept override
    {
        ++calls;
        for (int sample = 0; sample < numSamples; ++sample)
            left[sample] = right[sample] = mono[sample] * 0.5f;
    }
    int latencySamples() const override { return 0; }
    int calls = 0;
};

float rmsDb(const float* samples, int count)
{
    double sum = 0.0;
    for (int index = 0; index < count; ++index)
        sum += static_cast<double>(samples[index]) * samples[index];
    return juce::Decibels::gainToDecibels(static_cast<float>(std::sqrt(sum / std::max(1, count))), -120.0f);
}

class CompressorTests final : public juce::UnitTest
{
public:
    CompressorTests() : UnitTest("Compressor", "DAW") {}

    void runTest() override
    {
        using nam::effects::CompressorProcessor;
        beginTest("Static curve: hard knee, ratio and soft knee");
        expectWithinAbsoluteError(CompressorProcessor::staticCurve(-30.0f, -18.0f, 4.0f, 0.0f), -30.0f, 1.0e-4f);
        expectWithinAbsoluteError(CompressorProcessor::staticCurve(-6.0f, -18.0f, 4.0f, 0.0f), -15.0f, 1.0e-4f);
        expectWithinAbsoluteError(CompressorProcessor::staticCurve(0.0f, -20.0f, 20.0f, 0.0f), -19.0f, 1.0e-4f);
        // Within the knee the curve lies between the straight line and the compressed line.
        const auto knee = CompressorProcessor::staticCurve(-18.0f, -18.0f, 4.0f, 12.0f);
        expect(knee < -18.0f && knee > -18.0f - 6.0f * 0.75f, juce::String(knee));

        const auto steadyState = [this](float inputDb, float thresholdDb, float ratio, float makeupDb, float mix, bool bypassed)
        {
            CompressorProcessor compressor;
            compressor.prepare({ 48000.0, 512, 1 });
            compressor.setThresholdDb(thresholdDb);
            compressor.setRatio(ratio);
            compressor.setKneeDb(0.0f);
            compressor.setAttackMs(1.0f);
            compressor.setReleaseMs(50.0f);
            compressor.setMakeupDb(makeupDb);
            compressor.setMix(mix);
            compressor.setBypassed(bypassed);
            juce::AudioBuffer<float> buffer(1, 512);
            const auto amplitude = juce::Decibels::decibelsToGain(inputDb);
            float level = 0.0f;
            for (int block = 0; block < 60; ++block)
            {
                // A square wave keeps the peak detector exact.
                for (int sample = 0; sample < 512; ++sample)
                    buffer.setSample(0, sample, (sample / 24) % 2 == 0 ? amplitude : -amplitude);
                compressor.process(buffer);
                level = juce::Decibels::gainToDecibels(buffer.getMagnitude(0, 0, 512));
            }
            return std::pair { level, compressor.gainReductionDb() };
        };

        beginTest("Steady state follows the static curve");
        {
            const auto [level, reduction] = steadyState(-6.0f, -18.0f, 4.0f, 0.0f, 1.0f, false);
            logMessage("-6 dB in, -18 dB threshold, 4:1 -> " + juce::String(level, 2) + " dB, GR " + juce::String(reduction, 2));
            expectWithinAbsoluteError(level, -15.0f, 0.3f);
            expectWithinAbsoluteError(reduction, -9.0f, 0.3f);
        }
        beginTest("Below the threshold nothing changes; makeup adds gain");
        {
            expectWithinAbsoluteError(steadyState(-30.0f, -18.0f, 4.0f, 0.0f, 1.0f, false).first, -30.0f, 0.05f);
            expectWithinAbsoluteError(steadyState(-30.0f, -18.0f, 4.0f, 6.0f, 1.0f, false).first, -24.0f, 0.1f);
        }
        beginTest("Bypass is transparent, mix blends parallel compression");
        {
            expectWithinAbsoluteError(steadyState(-6.0f, -18.0f, 4.0f, 0.0f, 1.0f, true).first, -6.0f, 0.01f);
            // 50 % of -6 dB dry and 50 % of -15 dB compressed (same polarity).
            const auto expected = juce::Decibels::gainToDecibels(0.5f * juce::Decibels::decibelsToGain(-6.0f) + 0.5f * juce::Decibels::decibelsToGain(-15.0f));
            expectWithinAbsoluteError(steadyState(-6.0f, -18.0f, 4.0f, 0.0f, 0.5f, false).first, expected, 0.3f);
        }
        beginTest("Attack and release time constants");
        {
            CompressorProcessor compressor;
            compressor.prepare({ 48000.0, 4800, 1 });
            compressor.setThresholdDb(-20.0f);
            compressor.setRatio(20.0f);
            compressor.setKneeDb(0.0f);
            compressor.setAttackMs(10.0f);
            compressor.setReleaseMs(100.0f);
            juce::AudioBuffer<float> buffer(1, 480); // 10 ms
            buffer.clear();
            compressor.process(buffer);
            for (int sample = 0; sample < 480; ++sample)
                buffer.setSample(0, sample, 1.0f);
            compressor.process(buffer);
            // After one attack time constant about 63 % of the final ~19 dB reduction.
            const auto afterAttack = -compressor.gainReductionDb();
            expect(afterAttack > 10.0f && afterAttack < 14.0f, juce::String(afterAttack));
        }
    }
};

class WahPedalTests final : public juce::UnitTest
{
public:
    WahPedalTests() : UnitTest("Wah pedal", "DAW") {}

    void runTest() override
    {
        using nam::effects::WahPedalProcessor;
        const auto gainDb = [](float position, double frequency)
        {
            WahPedalProcessor wah;
            wah.prepare({ 48000.0, 4800, 1 });
            wah.setPosition(position);
            wah.setHeelHz(350.0f);
            wah.setToeHz(2200.0f);
            wah.setQ(6.0f);
            wah.setBypassed(false);
            juce::AudioBuffer<float> buffer(1, 4800);
            float out = 0.0f;
            for (int block = 0; block < 6; ++block)
            {
                for (int sample = 0; sample < 4800; ++sample)
                    buffer.setSample(0, sample, 0.1f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * frequency * (block * 4800 + sample) / 48000.0)));
                wah.process(buffer);
                out = buffer.getMagnitude(0, 2400, 2400);
            }
            return juce::Decibels::gainToDecibels(out / 0.1f);
        };

        beginTest("Exponential sweep between heel and toe");
        expectWithinAbsoluteError(WahPedalProcessor::centreFrequency(0.0f, 350.0f, 2200.0f), 350.0f, 0.01f);
        expectWithinAbsoluteError(WahPedalProcessor::centreFrequency(1.0f, 350.0f, 2200.0f), 2200.0f, 0.1f);
        expectWithinAbsoluteError(WahPedalProcessor::centreFrequency(0.5f, 350.0f, 2200.0f), std::sqrt(350.0f * 2200.0f), 0.1f);

        beginTest("Heel boosts the lows, toe the highs");
        const auto heelAtHeel = gainDb(0.0f, 350.0), heelAtToe = gainDb(0.0f, 2200.0);
        const auto toeAtToe = gainDb(1.0f, 2200.0), toeAtHeel = gainDb(1.0f, 350.0);
        logMessage("heel: " + juce::String(heelAtHeel, 1) + " dB @350 Hz, " + juce::String(heelAtToe, 1) + " dB @2200 Hz; toe: "
                   + juce::String(toeAtToe, 1) + " dB @2200 Hz, " + juce::String(toeAtHeel, 1) + " dB @350 Hz");
        expectWithinAbsoluteError(heelAtHeel, 6.0f, 0.5f);
        expectWithinAbsoluteError(toeAtToe, 6.0f, 0.5f);
        expect(heelAtToe < -15.0f && toeAtHeel < -15.0f);

        beginTest("Fast sweeps stay stable; bypass is transparent");
        {
            WahPedalProcessor wah;
            wah.prepare({ 48000.0, 64, 1 });
            wah.setBypassed(false);
            wah.setQ(15.0f);
            juce::Random random(3);
            juce::AudioBuffer<float> buffer(1, 64);
            float peak = 0.0f;
            for (int block = 0; block < 3000; ++block)
            {
                wah.setPosition(random.nextFloat()); // jerky mouse moves
                for (int sample = 0; sample < 64; ++sample)
                    buffer.setSample(0, sample, random.nextFloat() - 0.5f);
                wah.process(buffer);
                peak = std::max(peak, buffer.getMagnitude(0, 0, 64));
            }
            expect(std::isfinite(peak) && peak < 4.0f, juce::String(peak));
            wah.setBypassed(true);
            for (int block = 0; block < 20; ++block)
                wah.process(buffer); // let the bypass fade finish
            for (int sample = 0; sample < 64; ++sample)
                buffer.setSample(0, sample, 0.25f);
            wah.process(buffer);
            expectWithinAbsoluteError(buffer.getSample(0, 10), 0.25f, 1.0e-6f);
        }

        beginTest("Presets keep the pedal, older presets load it switched off");
        {
            SoundState sound;
            sound.pedalWahBypassed = false;
            sound.pedalWahMouse = true;
            sound.pedalWahPosition = 0.3f;
            sound.pedalWahToeHz = 1800.0f;
            const auto restored = SoundState::fromValueTree(sound.toValueTree());
            expect(restored.has_value());
            if (restored)
            {
                expect(! restored->pedalWahBypassed && restored->pedalWahMouse);
                expectWithinAbsoluteError(restored->pedalWahPosition, 0.3f, 1.0e-6f);
                expectWithinAbsoluteError(restored->pedalWahToeHz, 1800.0f, 1.0e-3f);
            }
            auto old = sound.toValueTree();
            for (const auto* name : { "pedalWahBypassed", "pedalWahMouse", "pedalWahPosition", "pedalWahHeelHz", "pedalWahToeHz", "pedalWahQ", "pedalWahMix" })
                old.removeProperty(name, nullptr);
            const auto legacy = SoundState::fromValueTree(old);
            expect(legacy.has_value() && legacy->pedalWahBypassed && ! legacy->pedalWahMouse);
        }
    }
};

class SendReturnTests final : public juce::UnitTest
{
public:
    SendReturnTests() : UnitTest("Send effects and return tracks", "DAW") {}

    void runTest() override
    {
        beginTest("Return tracks stay at the bottom; clips never land on them");
        {
            ArrangementState state;
            edit::addTrack(state, edit::makeTrack(state, "Guitar"));
            edit::addTrack(state, *edit::makeReturnTrack(state));      // A
            edit::addTrack(state, *edit::makeReturnTrack(state));      // B
            const auto index = edit::addTrack(state, edit::makeTrack(state, "Bass"));
            expectEquals(index, 1);
            expectEquals(state.firstReturnIndex(), 2);
            expectEquals(state.returnTrackIndex(0), 2);
            expectEquals(state.returnTrackIndex(1), 3);
            edit::addTrack(state, *edit::makeReturnTrack(state));
            edit::addTrack(state, *edit::makeReturnTrack(state));
            expect(! edit::makeReturnTrack(state).has_value(), "at most four returns");
            expect(edit::insert(state, edit::makeClip({}, makeConstant(0.5f, 100), 0), 2).isNull());
            const auto clip = edit::insert(state, edit::makeClip({}, makeConstant(0.5f, 100), 0), 0);
            expect(! edit::move(state, clip, 0, 3));
            expect(edit::move(state, clip, 10, 1));
        }

        constexpr int block = 256;
        const auto makeState = [](std::shared_ptr<HalfProcessor> returnChain)
        {
            ArrangementState state;
            edit::addTrack(state, edit::makeTrack(state, "Guitar"));
            edit::insert(state, edit::makeClip({}, makeConstant(0.4f, 48000), 0), 0);
            auto ret = *edit::makeReturnTrack(state);
            ret.fx->processor = std::move(returnChain);
            ret.fx->enabled = true;
            edit::addTrack(state, std::move(ret));
            state.tracks[0].mixer->sendGain[0] = 0.5f;
            return state;
        };

        beginTest("Post-fader send into a return chain (live and offline agree)");
        {
            auto chain = std::make_shared<HalfProcessor>();
            auto state = makeState(chain);
            ArrangementEngine engine;
            engine.prepare(48000.0, block);
            engine.publish(ArrangementEngine::buildSnapshot(state));
            engine.play();
            juce::AudioBuffer<float> out(2, block);
            for (int pass = 0; pass < 2; ++pass) // the first block ramps gains and sends
            {
                engine.setPosition(1000);
                out.clear();
                engine.process(nullptr, 0, nullptr, nullptr, out.getWritePointer(0), out.getWritePointer(1), block);
            }
            // Dry 0.4 plus the return: 0.4 * send 0.5 -> chain halves -> 0.1.
            expectWithinAbsoluteError(out.getSample(0, 100), 0.5f, 1.0e-5f);
            expect(chain->calls > 0);

            juce::AudioBuffer<float> mix(2, block), scratch;
            ArrangementEngine::renderOffline(*ArrangementEngine::buildSnapshot(state), 1000, block, mix.getWritePointer(0), mix.getWritePointer(1), scratch,
                                             { .useProcessors = true });
            expectWithinAbsoluteError(mix.getSample(1, 100), 0.5f, 1.0e-5f);
            // Stem of the return: only what was sent to it.
            ArrangementEngine::renderOffline(*ArrangementEngine::buildSnapshot(state), 1000, block, mix.getWritePointer(0), mix.getWritePointer(1), scratch,
                                             { .onlyTrack = 1, .useProcessors = true });
            expectWithinAbsoluteError(mix.getSample(0, 100), 0.1f, 1.0e-5f);
            // Stem of the guitar: dry only.
            ArrangementEngine::renderOffline(*ArrangementEngine::buildSnapshot(state), 1000, block, mix.getWritePointer(0), mix.getWritePointer(1), scratch,
                                             { .onlyTrack = 0, .useProcessors = true });
            expectWithinAbsoluteError(mix.getSample(0, 100), 0.4f, 1.0e-5f);
        }

        beginTest("Sends follow the fader and mute; returns are solo-safe");
        {
            auto state = makeState(std::make_shared<HalfProcessor>());
            juce::AudioBuffer<float> mix(2, block), scratch;
            const auto render = [&]
            {
                ArrangementEngine::renderOffline(*ArrangementEngine::buildSnapshot(state), 1000, block, mix.getWritePointer(0), mix.getWritePointer(1), scratch,
                                                 { .useProcessors = true });
                return mix.getSample(0, 100);
            };
            state.tracks[0].mixer->volumeDb = -6.0206f;
            expectWithinAbsoluteError(render(), 0.25f, 1.0e-4f); // 0.2 dry + 0.05 return
            state.tracks[0].mixer->volumeDb = 0.0f;
            state.tracks[0].mixer->soloed = true;
            expectWithinAbsoluteError(render(), 0.5f, 1.0e-5f);  // the return keeps playing
            state.tracks[0].mixer->soloed = false;
            state.tracks[0].mixer->muted = true;
            expectWithinAbsoluteError(render(), 0.0f, 1.0e-6f);  // muted: nothing sent either
            state.tracks[0].mixer->muted = false;
            state.tracks[1].mixer->muted = true;
            expectWithinAbsoluteError(render(), 0.4f, 1.0e-5f);
        }

        beginTest("Detached chains do not run live");
        {
            auto chain = std::make_shared<HalfProcessor>();
            auto state = makeState(chain);
            const std::set<juce::Uuid> detached { state.tracks[1].id };
            ArrangementEngine engine;
            engine.prepare(48000.0, block);
            engine.publish(ArrangementEngine::buildSnapshot(state, &detached));
            engine.play();
            juce::AudioBuffer<float> out(2, block);
            engine.process(nullptr, 0, nullptr, nullptr, out.getWritePointer(0), out.getWritePointer(1), block);
            expectEquals(chain->calls, 0);
        }

        beginTest("Projects keep returns and send levels");
        {
            auto state = makeState(std::make_shared<HalfProcessor>());
            state.tracks[0].mixer->sendGain[0] = 0.7f;
            const auto restored = fromValueTree(toValueTree(state, {}), {});
            expectEquals(static_cast<int>(restored.tracks.size()), 2);
            expectEquals(restored.tracks[1].returnSlot, 0);
            expectWithinAbsoluteError(restored.tracks[0].mixer->sendGain[0].load(), 0.7f, 1.0e-6f);
            expectEquals(restored.tracks[1].clips.size(), size_t(0));
        }
    }
};

class FreezeTests final : public juce::UnitTest
{
public:
    FreezeTests() : UnitTest("Track freezing", "DAW") {}

    void runTest() override
    {
        constexpr int block = 256;
        ArrangementState state;
        edit::addTrack(state, edit::makeTrack(state, "Guitar"));
        edit::insert(state, edit::makeClip({}, makeConstant(0.4f, 4800), 0), 0);
        auto chain = std::make_shared<HalfProcessor>();
        state.tracks[0].fx->processor = chain;
        state.tracks[0].fx->enabled = true;
        state.tracks[0].fx->settings = juce::ValueTree("soundState").setProperty("x", 1, nullptr);
        state.tracks[0].mixer->volumeDb = -6.0206f;
        state.tracks[0].mixer->pan = 1.0f;

        beginTest("The freeze render is pre-fader and runs the chain");
        juce::AudioBuffer<float> render(2, 4800), scratch;
        ArrangementEngine::renderOffline(*ArrangementEngine::buildSnapshot(state), 0, 4800, render.getWritePointer(0), render.getWritePointer(1), scratch,
                                         { .onlyTrack = 0, .preFader = true, .useProcessors = true, .processorBlock = 64 });
        expectWithinAbsoluteError(render.getSample(0, 1000), 0.2f, 1.0e-6f); // halved, unity gain, centred
        expectWithinAbsoluteError(render.getSample(1, 1000), 0.2f, 1.0e-6f);

        beginTest("A frozen track plays its render instead of the chain");
        auto frozenData = std::make_shared<AudioData>();
        frozenData->sampleRate = 48000.0;
        frozenData->samples = render;
        frozenData->computePeaks();
        auto frozen = std::make_shared<FrozenRender>();
        frozen->data = frozenData;
        frozen->signature = freezeSignature(state.tracks[0], state);
        state.tracks[0].frozen = frozen;
        expect(state.tracks[0].isFrozen(state));
        chain->calls = 0;
        ArrangementEngine engine;
        engine.prepare(48000.0, block);
        engine.publish(ArrangementEngine::buildSnapshot(state));
        engine.play();
        juce::AudioBuffer<float> out(2, block);
        for (int pass = 0; pass < 2; ++pass)
        {
            engine.setPosition(1000);
            out.clear();
            engine.process(nullptr, 0, nullptr, nullptr, out.getWritePointer(0), out.getWritePointer(1), block);
        }
        expectEquals(chain->calls, 0);
        // Fader and pan still apply after the frozen render: -6 dB, hard right.
        expectWithinAbsoluteError(out.getSample(0, 100), 0.0f, 1.0e-6f);
        expectWithinAbsoluteError(out.getSample(1, 100), 0.1f, 1.0e-4f);

        beginTest("Editing the track thaws it; undoing restores the freeze");
        auto edited = state;
        edited.tracks[0].clips[0].gainDb = -3.0f;
        expect(! edited.tracks[0].isFrozen(edited));
        expect(state.tracks[0].isFrozen(state));
        auto fxEdited = state;
        state.tracks[0].fx->settings.setProperty("x", 2, nullptr); // effect settings are shared by all undo states
        expect(! fxEdited.tracks[0].isFrozen(fxEdited));
        state.tracks[0].fx->settings.setProperty("x", 1, nullptr);
        auto retempo = state;
        retempo.bpm = 90.0;
        expect(! retempo.tracks[0].isFrozen(retempo));
        auto fader = state;
        fader.tracks[0].mixer->volumeDb = -20.0f; // the fader is after the freeze
        expect(fader.tracks[0].isFrozen(fader));
        fader.tracks[0].mixer->volumeDb = -6.0206f;

        beginTest("Projects keep the frozen render");
        const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("FreezeTest-" + juce::Uuid().toString());
        directory.getChildFile("Audio").createDirectory();
        auto stored = std::make_shared<FrozenRender>(*frozen);
        stored->file = directory.getChildFile("Audio").getChildFile("Guitar (eingefroren).wav");
        expect(stored->file.replaceWithText("placeholder"));
        state.tracks[0].frozen = stored;
        const auto tree = toValueTree(state, directory);
        expectEquals(tree.getChild(0).getChildWithName("Frozen").getProperty("file").toString(), juce::String("Audio/Guitar (eingefroren).wav"));
        const auto restored = fromValueTree(tree, directory);
        expect(restored.tracks[0].frozen != nullptr);
        if (restored.tracks[0].frozen != nullptr)
        {
            expect(restored.tracks[0].frozen->file == stored->file);
            expect(restored.tracks[0].frozen->data == nullptr); // loaded later by the controller
        }
        directory.deleteRecursively();
    }
};

class ShortcutTests final : public juce::UnitTest
{
public:
    ShortcutTests() : UnitTest("Keyboard shortcuts", "UI") {}

    void runTest() override
    {
        beginTest("Command ids are unique and no default key is used twice");
        std::set<juce::CommandID> ids;
        juce::Array<juce::KeyPress> keys;
        for (const auto& definition : nam::ui::commandDefinitions())
        {
            expect(ids.insert(definition.id).second, "duplicate id " + juce::String(definition.id));
            for (const auto& key : definition.defaultKeys)
            {
                expect(! keys.contains(key), juce::String("key used twice: ") + key.getTextDescription() + " (" + definition.name + ")");
                keys.add(key);
            }
        }
        expect(nam::ui::findCommand(nam::ui::commands::freezeTrack) != nullptr);

        beginTest("German key names");
        expectEquals(nam::ui::describeKey(juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)),
                     juce::String("Strg + Umschalt + Z"));
        expectEquals(nam::ui::describeKey(juce::KeyPress(juce::KeyPress::spaceKey)), juce::String("Leertaste"));
        expectEquals(nam::ui::describeKey(juce::KeyPress(juce::KeyPress::F9Key)), juce::String("F9"));
        expectEquals(nam::ui::describeKey(juce::KeyPress(juce::KeyPress::homeKey)), juce::String("Pos1"));
    }
};

CompressorTests compressorTests;
WahPedalTests wahPedalTests;
SendReturnTests sendReturnTests;
FreezeTests freezeTests;
ShortcutTests shortcutTests;
} // namespace
