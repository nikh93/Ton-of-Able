#include <cmath>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "../src/daw/Arrangement.h"
#include "../src/daw/ArrangementEngine.h"
#include "../src/daw/AudioCodecs.h"
#include "../src/daw/TimeStretcher.h"
#include "../src/audio/effects/GraphicEQProcessor.h"
#include "../src/presets/PresetManager.h"

namespace
{
using namespace daw;

AudioDataPtr makeRamp(int length, int channels = 1, double rate = 48000.0)
{
    auto data = std::make_shared<AudioData>();
    data->sampleRate = rate;
    data->samples.setSize(channels, length);
    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < length; ++sample)
            data->samples.setSample(channel, sample, static_cast<float>(sample) / static_cast<float>(length) * (channel == 0 ? 1.0f : -1.0f));
    data->computePeaks();
    return data;
}

AudioDataPtr makeSine(double frequency, int length, double rate = 48000.0)
{
    auto data = std::make_shared<AudioData>();
    data->sampleRate = rate;
    data->samples.setSize(1, length);
    for (int sample = 0; sample < length; ++sample)
        data->samples.setSample(0, sample, 0.5f * static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * frequency * sample / rate)));
    data->computePeaks();
    return data;
}

int zeroCrossings(const juce::AudioBuffer<float>& buffer, int from, int to)
{
    int crossings = 0;
    for (int sample = from + 1; sample < to; ++sample)
        if ((buffer.getSample(0, sample - 1) < 0.0f) != (buffer.getSample(0, sample) < 0.0f))
            ++crossings;
    return crossings;
}

ArrangementState stateWithClip(AudioDataPtr source, juce::int64 start, int tracks = 1)
{
    ArrangementState state;
    for (int index = 0; index < tracks; ++index)
        state.tracks.push_back(edit::makeTrack(state, "T" + juce::String(index)));
    edit::insert(state, edit::makeClip(juce::File::getCurrentWorkingDirectory().getChildFile("ramp.wav"), std::move(source), start), 0);
    return state;
}

class ArrangementEditTests final : public juce::UnitTest
{
public:
    ArrangementEditTests() : UnitTest("Arrangement editing", "DAW") {}

    void runTest() override
    {
        beginTest("Split keeps the audio aligned on the timeline");
        {
            auto state = stateWithClip(makeRamp(1000), 100);
            const auto id = state.tracks[0].clips[0].id;
            const auto right = edit::split(state, id, 400);
            expect(right.has_value());
            expectEquals(static_cast<int>(state.tracks[0].clips.size()), 2);
            const auto& a = state.tracks[0].clips[0];
            const auto& b = state.tracks[0].clips[1];
            expectEquals(a.length, juce::int64(300));
            expectEquals(b.start, juce::int64(400));
            expectEquals(b.length, juce::int64(700));
            expectWithinAbsoluteError(b.offset, 300.0, 1.0e-9);
            expect(! edit::split(state, id, 100).has_value());
        }

        beginTest("Trimming is limited by the source material");
        {
            auto state = stateWithClip(makeRamp(1000), 100);
            const auto id = state.tracks[0].clips[0].id;
            expect(edit::trimStart(state, id, 250));
            expectEquals(state.findClip(id)->start, juce::int64(250));
            expectWithinAbsoluteError(state.findClip(id)->offset, 150.0, 1.0e-9);
            expect(edit::trimStart(state, id, 0)); // cannot reveal audio before the file start
            expectEquals(state.findClip(id)->start, juce::int64(100));
            expectWithinAbsoluteError(state.findClip(id)->offset, 0.0, 1.0e-9);
            expect(edit::trimEnd(state, id, 5000));
            expectEquals(state.findClip(id)->end(), juce::int64(1100));
            expect(edit::trimEnd(state, id, 600));
            expectEquals(state.findClip(id)->length, juce::int64(500));
        }

        beginTest("Stretching an edge changes the ratio, not the material");
        {
            auto state = stateWithClip(makeRamp(1000), 0);
            const auto id = state.tracks[0].clips[0].id;
            expect(edit::stretchEnd(state, id, 1500));
            expectWithinAbsoluteError(state.findClip(id)->stretchRatio, 1.5, 1.0e-9);
            expectEquals(state.findClip(id)->length, juce::int64(1500));
            expect(edit::stretchStart(state, id, 500)); // end stays at 1500
            expectEquals(state.findClip(id)->end(), juce::int64(1500));
            expectWithinAbsoluteError(state.findClip(id)->stretchRatio, 1.0, 1.0e-9);
            expect(edit::stretchEnd(state, id, 100000));
            expectWithinAbsoluteError(state.findClip(id)->stretchRatio, edit::maximumRatio, 1.0e-9);
        }

        beginTest("Moved clips overwrite what lies underneath");
        {
            auto state = stateWithClip(makeRamp(1000), 0, 2);
            const auto longId = state.tracks[0].clips[0].id;
            const auto shortId = edit::insert(state, edit::makeClip({}, makeRamp(200), 0), 1);
            expect(edit::move(state, shortId, 400, 0));
            expectEquals(static_cast<int>(state.tracks[1].clips.size()), 0);
            const auto& clips = state.tracks[0].clips;
            expectEquals(static_cast<int>(clips.size()), 3);
            expectEquals(clips[0].id.toString(), longId.toString());
            expectEquals(clips[0].end(), juce::int64(400));
            expectEquals(clips[1].id.toString(), shortId.toString());
            expectEquals(clips[2].start, juce::int64(600));
            expectWithinAbsoluteError(clips[2].offset, 600.0, 1.0e-9);
        }

        beginTest("Project serialisation round-trips with relative paths");
        {
            const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ArrangementTests-" + juce::Uuid().toString());
            auto state = stateWithClip(makeRamp(1000), 123);
            state.bpm = 97.5;
            state.loopEnabled = true;
            state.loopStart = 10;
            state.loopEnd = 990;
            auto& clip = state.tracks[0].clips[0];
            clip.file = directory.getChildFile("Audio").getChildFile("take.wav");
            clip.stretchRatio = 1.25;
            clip.gainDb = -3.0f;
            state.tracks[0].mixer->volumeDb = -6.0f;
            state.tracks[0].inputChannel = 1;
            const auto tree = toValueTree(state, directory);
            expectEquals(tree.getChild(0).getChild(0).getProperty("file").toString(), juce::String("Audio/take.wav"));

            const auto moved = directory.getSiblingFile("moved");
            const auto loaded = fromValueTree(tree, moved);
            expectEquals(static_cast<int>(loaded.tracks.size()), 1);
            expectWithinAbsoluteError(loaded.bpm, 97.5, 1.0e-9);
            expect(loaded.loopEnabled);
            const auto& c = loaded.tracks[0].clips[0];
            expect(c.file == moved.getChildFile("Audio/take.wav"));
            expectEquals(c.start, juce::int64(123));
            expectWithinAbsoluteError(c.stretchRatio, 1.25, 1.0e-9);
            expectWithinAbsoluteError(loaded.tracks[0].mixer->volumeDb.load(), -6.0f, 1.0e-6f);
            expectEquals(loaded.tracks[0].inputChannel, 1);
        }

        beginTest("Tempo grid snapping and formatting");
        {
            TimeGrid grid { 48000.0, 120.0, 4, 4 };
            expectWithinAbsoluteError(grid.samplesPerBeat(), 24000.0, 1.0e-9);
            expectEquals(grid.snap(25000, 0.25), juce::int64(24000));
            expectEquals(grid.snap(25000, 0.0), juce::int64(25000));
            expectEquals(grid.formatPosition(96000 + 24000 + 6000), juce::String("2.2.2"));
        }
    }
};

class ArrangementEngineTests final : public juce::UnitTest
{
public:
    ArrangementEngineTests() : UnitTest("Arrangement realtime engine", "DAW") {}

    void runTest() override
    {
        constexpr int block = 256;
        juce::AudioBuffer<float> out(2, block);

        beginTest("Playback reproduces clip samples at the right position");
        {
            const auto source = makeRamp(4000, 2);
            ArrangementEngine engine;
            engine.prepare(48000.0, block);
            engine.publish(ArrangementEngine::buildSnapshot(stateWithClip(source, 1000)));
            engine.setPosition(1500);
            engine.play();
            out.clear();
            engine.process(nullptr, 0, nullptr, nullptr, out.getWritePointer(0), out.getWritePointer(1), block);
            expectWithinAbsoluteError(out.getSample(0, 10), source->samples.getSample(0, 510), 1.0e-6f);
            expectWithinAbsoluteError(out.getSample(1, 10), source->samples.getSample(1, 510), 1.0e-6f);
            expectEquals(engine.position(), juce::int64(1500 + block));
        }

        beginTest("Mute, solo and volume");
        {
            auto state = stateWithClip(makeRamp(4000), 0, 2);
            edit::insert(state, edit::makeClip({}, makeRamp(4000), 0), 1);
            ArrangementEngine engine;
            engine.prepare(48000.0, block);
            engine.publish(ArrangementEngine::buildSnapshot(state));
            engine.play();
            const auto render = [&]
            {
                // The first block ramps the track gains; measure the second.
                for (int pass = 0; pass < 2; ++pass)
                {
                    engine.setPosition(1000);
                    out.clear();
                    engine.process(nullptr, 0, nullptr, nullptr, out.getWritePointer(0), out.getWritePointer(1), block);
                }
                return out.getSample(0, 100);
            };
            const auto both = render();
            state.tracks[1].mixer->soloed = true;
            const auto solo = render();
            expectWithinAbsoluteError(solo, both * 0.5f, 1.0e-4f);
            state.tracks[1].mixer->soloed = false;
            state.tracks[0].mixer->muted = true;
            state.tracks[1].mixer->volumeDb = -6.0206f;
            expectWithinAbsoluteError(render(), both * 0.25f, 1.0e-4f);
        }

        beginTest("Loop wraps sample-accurately");
        {
            ArrangementEngine engine;
            engine.prepare(48000.0, block);
            engine.publish(ArrangementEngine::buildSnapshot(stateWithClip(makeRamp(4000), 0)));
            engine.setLoop(true, 1000, 1100);
            engine.setPosition(1050);
            engine.play();
            out.clear();
            engine.process(nullptr, 0, nullptr, nullptr, out.getWritePointer(0), out.getWritePointer(1), block);
            expectEquals(engine.position(), juce::int64(1000 + (block - 50) % 100));
        }

        beginTest("Offline mixdown renders the mix and single tracks (stems)");
        {
            auto state = stateWithClip(makeRamp(4000), 0, 2);
            edit::insert(state, edit::makeClip({}, makeRamp(4000), 0), 1);
            state.tracks[0].mixer->muted = true;
            state.tracks[1].mixer->pan = 1.0f;
            const auto snapshot = ArrangementEngine::buildSnapshot(state);
            juce::AudioBuffer<float> mix(2, 256), scratch;
            ArrangementEngine::renderOffline(*snapshot, 1000, 256, mix.getWritePointer(0), mix.getWritePointer(1), scratch);
            const auto expected = 1100.0f / 4000.0f;
            expectWithinAbsoluteError(mix.getSample(0, 100), 0.0f, 1.0e-6f); // track 2 panned hard right, track 1 muted
            expectWithinAbsoluteError(mix.getSample(1, 100), expected, 1.0e-5f);
            ArrangementEngine::renderOffline(*snapshot, 1000, 256, mix.getWritePointer(0), mix.getWritePointer(1), scratch, 0);
            expectWithinAbsoluteError(mix.getSample(0, 100), expected, 1.0e-5f); // stems ignore mute
        }

        beginTest("Recording with count-in writes exactly from the punch-in");
        {
            const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ArrangementTake-" + juce::Uuid().toString() + ".wav");
            ArrangementEngine engine;
            engine.prepare(48000.0, block);
            ArrangementState state;
            state.tracks.push_back(edit::makeTrack(state, "DI"));
            state.tracks[0].armed = true;
            state.tracks[0].inputChannel = 0;
            engine.publish(ArrangementEngine::buildSnapshot(state));
            expect(engine.startRecording({ { state.tracks[0].id, 0, file, 1 } }, 48000.0, 1000).wasOk());
            engine.setPosition(1000 - 512); // one "bar" of count-in
            engine.play();
            juce::AudioBuffer<float> input(1, block);
            juce::int64 position = 1000 - 512;
            for (int round = 0; round < 8; ++round)
            {
                for (int sample = 0; sample < block; ++sample)
                    input.setSample(0, sample, static_cast<float>(position + sample) / 10000.0f);
                const float* inputs[] { input.getReadPointer(0) };
                out.clear();
                engine.process(inputs, 1, nullptr, nullptr, out.getWritePointer(0), out.getWritePointer(1), block);
                position += block;
            }
            const auto takes = engine.stopRecording();
            expectEquals(static_cast<int>(takes.size()), 1);
            expectEquals(takes[0].startPosition, juce::int64(1000));
            expectEquals(takes[0].samples, juce::int64(8 * block - 512));

            juce::String error;
            const auto recorded = loadAudioFile(file, 0.0, error);
            expect(recorded != nullptr, error);
            if (recorded != nullptr)
            {
                expectEquals(recorded->length(), takes[0].samples);
                expectWithinAbsoluteError(recorded->samples.getSample(0, 0), 1000.0f / 10000.0f, 1.0e-5f);
                expectWithinAbsoluteError(recorded->samples.getSample(0, 700), 1700.0f / 10000.0f, 1.0e-5f);
            }
            file.deleteFile();
        }
    }
};

// Doubles the signal and adds a constant, so tests can tell where it ran.
struct MarkerProcessor final : TrackProcessor
{
    void prepare(double, int) override { ++prepared; }
    void process(float* mono, float* left, float* right, int numSamples) noexcept override
    {
        for (int sample = 0; sample < numSamples; ++sample)
            left[sample] = right[sample] = mono[sample] * 2.0f + 0.25f;
    }
    int latencySamples() const override { return 0; }
    int prepared = 0;
};

class TrackFxTests final : public juce::UnitTest
{
public:
    TrackFxTests() : UnitTest("Per-track effect chains", "DAW") {}

    void runTest() override
    {
        constexpr int block = 128;
        juce::AudioBuffer<float> out(2, block);

        beginTest("Insert mode runs the chain on playback only when enabled");
        {
            auto state = stateWithClip(makeRamp(4000), 0);
            auto processor = std::make_shared<MarkerProcessor>();
            state.tracks[0].fx->processor = processor;
            ArrangementEngine engine;
            engine.prepare(48000.0, block);
            engine.publish(ArrangementEngine::buildSnapshot(state));
            expectEquals(processor->prepared, 0); // published after prepare(): the controller prepares new chains itself
            engine.play();
            const auto render = [&]
            {
                for (int pass = 0; pass < 2; ++pass)
                {
                    engine.setPosition(1000);
                    out.clear();
                    engine.process(nullptr, 0, nullptr, nullptr, out.getWritePointer(0), out.getWritePointer(1), block);
                }
                return out.getSample(0, 10);
            };
            const auto dry = render();
            state.tracks[0].fx->enabled = true;
            expectWithinAbsoluteError(render(), dry * 2.0f + 0.25f, 1.0e-5f);
            state.tracks[0].fx->printWhileRecording = true; // printed takes are not processed again
            expectWithinAbsoluteError(render(), dry, 1.0e-5f);
        }

        beginTest("Print mode records the processed input, insert mode the dry input");
        {
            for (const auto print : { true, false })
            {
                const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("FxTake-" + juce::Uuid().toString() + ".wav");
                ArrangementState state;
                state.tracks.push_back(edit::makeTrack(state, "DI"));
                state.tracks[0].armed = true;
                state.tracks[0].inputChannel = 0;
                state.tracks[0].fx->processor = std::make_shared<MarkerProcessor>();
                state.tracks[0].fx->enabled = true;
                state.tracks[0].fx->printWhileRecording = print;
                ArrangementEngine engine;
                engine.prepare(48000.0, block);
                engine.publish(ArrangementEngine::buildSnapshot(state));
                expect(engine.startRecording({ { state.tracks[0].id, 0, file, print ? 2 : 1 } }, 48000.0, 0).wasOk());
                engine.setPosition(0);
                engine.play();
                juce::AudioBuffer<float> input(1, block);
                input.clear();
                input.setSample(0, 5, 0.1f);
                const float* inputs[] { input.getReadPointer(0) };
                out.clear();
                engine.process(inputs, 1, nullptr, nullptr, out.getWritePointer(0), out.getWritePointer(1), block);
                engine.stopRecording();
                juce::String error;
                const auto recorded = loadAudioFile(file, 0.0, error);
                expect(recorded != nullptr, error);
                if (recorded != nullptr)
                {
                    expectEquals(recorded->numChannels(), print ? 2 : 1);
                    expectWithinAbsoluteError(recorded->samples.getSample(0, 5), print ? 0.45f : 0.1f, 1.0e-4f);
                }
                // Monitoring always goes through the chain.
                expectWithinAbsoluteError(out.getSample(0, 5), 0.45f, 1.0e-4f);
                file.deleteFile();
            }
        }

        beginTest("Track effect settings are saved with the project");
        {
            ArrangementState state;
            state.tracks.push_back(edit::makeTrack(state, "Lead"));
            state.tracks[0].fx->enabled = true;
            state.tracks[0].fx->printWhileRecording = true;
            state.tracks[0].fx->settings = juce::ValueTree("soundState").setProperty("graphicEqBypassed", false, nullptr);
            const auto loaded = fromValueTree(toValueTree(state, {}), {});
            expect(loaded.tracks[0].fx->enabled.load());
            expect(loaded.tracks[0].fx->printWhileRecording.load());
            expect(loaded.tracks[0].fx->settings.hasType("soundState"));
        }
    }
};

class GraphicEqTests final : public juce::UnitTest
{
public:
    GraphicEqTests() : UnitTest("Graphic EQ", "DAW") {}

    void runTest() override
    {
        const auto levelAt = [](float frequency, float gainDb, bool bypassed)
        {
            nam::effects::GraphicEQProcessor eq;
            eq.prepare({ 48000.0, 512, 1 });
            eq.setBypassed(bypassed);
            eq.setBandGainDb(5, gainDb); // 1 kHz
            juce::AudioBuffer<float> buffer(1, 512);
            double energy = 0.0;
            for (int block = 0; block < 40; ++block)
            {
                for (int sample = 0; sample < 512; ++sample)
                    buffer.setSample(0, sample, std::sin(2.0f * juce::MathConstants<float>::pi * frequency * static_cast<float>(block * 512 + sample) / 48000.0f));
                eq.process(buffer);
                if (block >= 30)
                    for (int sample = 0; sample < 512; ++sample)
                        energy += buffer.getSample(0, sample) * buffer.getSample(0, sample);
            }
            return juce::Decibels::gainToDecibels(static_cast<float>(std::sqrt(energy / (10 * 512)) * std::sqrt(2.0)));
        };

        beginTest("A band boosts its centre frequency");
        expectWithinAbsoluteError(levelAt(1000.0f, 6.0f, false), 6.0f, 0.5f);
        expectWithinAbsoluteError(levelAt(1000.0f, -9.0f, false), -9.0f, 0.6f);
        beginTest("Bypass and distant bands stay flat");
        expectWithinAbsoluteError(levelAt(1000.0f, 6.0f, true), 0.0f, 0.1f);
        expectWithinAbsoluteError(levelAt(60.0f, 6.0f, false), 0.0f, 0.3f);

        beginTest("Graphic EQ settings persist in presets");
        SoundState sound;
        sound.graphicEqBypassed = false;
        sound.graphicEqDb[3] = -4.5f;
        const auto loaded = SoundState::fromValueTree(sound.toValueTree());
        expect(loaded.has_value());
        if (loaded)
        {
            expect(! loaded->graphicEqBypassed);
            expectWithinAbsoluteError(loaded->graphicEqDb[3], -4.5f, 0.01f);
        }
    }
};

class RubberBandTests final : public juce::UnitTest
{
public:
    RubberBandTests() : UnitTest("Rubber Band time stretch", "DAW") {}

    void runTest() override
    {
        const auto source = makeSine(441.0, 48000);
        const auto sourceCrossings = zeroCrossings(source->samples, 4800, 43200);

        beginTest("Stretching keeps the pitch and pins the length");
        {
            const auto stretched = renderStretched(*source, 1.5, 0.0);
            expect(stretched != nullptr);
            expectEquals(stretched->length(), juce::int64(72000));
            const auto crossings = zeroCrossings(stretched->samples, 7200, 64800);
            expectWithinAbsoluteError(static_cast<double>(crossings) / 1.5, static_cast<double>(sourceCrossings), sourceCrossings * 0.03);
        }

        beginTest("Pitch shifting keeps the length");
        {
            const auto shifted = renderStretched(*source, 1.0, 12.0);
            expect(shifted != nullptr);
            expectEquals(shifted->length(), juce::int64(48000));
            const auto crossings = zeroCrossings(shifted->samples, 4800, 43200);
            expectWithinAbsoluteError(static_cast<double>(crossings), 2.0 * sourceCrossings, sourceCrossings * 0.06);
        }

        beginTest("Cancelled renders return nothing");
        {
            std::atomic<bool> cancelled { true };
            expect(renderStretched(*source, 2.0, 0.0, &cancelled) == nullptr);
        }

        beginTest("Snapshot prefers the current Rubber Band render");
        {
            auto state = stateWithClip(source, 0);
            edit::setStretchRatio(state, state.tracks[0].clips[0].id, 2.0);
            auto& clip = state.tracks[0].clips[0]; // edits rebuild the clip list
            auto snapshot = ArrangementEngine::buildSnapshot(state);
            expect(snapshot->tracks[0].clips[0].stretched == nullptr); // varispeed preview
            clip.stretched = renderStretched(*source, 2.0, 0.0);
            clip.renderedRatio = 2.0;
            snapshot = ArrangementEngine::buildSnapshot(state);
            expect(snapshot->tracks[0].clips[0].stretched == clip.stretched.get());
        }
    }
};

class CodecTests final : public juce::UnitTest
{
public:
    CodecTests() : UnitTest("Audio import/export formats", "DAW") {}

    void runTest() override
    {
        const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("CodecTests-" + juce::Uuid().toString());
        directory.createDirectory();
        const auto source = makeRamp(9000, 2);

        for (const auto format : { codecs::ExportFormat::wav24, codecs::ExportFormat::wav32Float, codecs::ExportFormat::aiff24,
                                   codecs::ExportFormat::flac24, codecs::ExportFormat::oggVorbis })
        {
            const auto& info = codecs::infoFor(format);
            beginTest(juce::String("Native export and re-import: ") + info.name);
            const auto file = directory.getChildFile(juce::String("export-") + juce::String(static_cast<int>(format)) + "." + info.extension);
            codecs::ExportWriter writer(file, format, 48000.0, 2);
            expect(writer.openResult().wasOk(), writer.openResult().getErrorMessage());
            expect(writer.write(source->samples.getArrayOfReadPointers(), source->samples.getNumSamples()));
            expect(writer.finish().wasOk());
            expect(codecs::canDecodeNatively(file));
            juce::String error;
            const auto loaded = loadAudioFile(file, 48000.0, error);
            expect(loaded != nullptr, error);
            if (loaded != nullptr && format != codecs::ExportFormat::oggVorbis)
            {
                expectEquals(loaded->length(), source->length());
                expectWithinAbsoluteError(loaded->samples.getSample(1, 4500), source->samples.getSample(1, 4500), 1.0e-4f);
            }
        }

        beginTest("Video and compressed formats are offered for import");
        expect(codecs::importWildcard().contains("*.mp4"));
        expect(codecs::importWildcard().contains("*.m4a"));
        expect(codecs::isVideoFile(juce::File("clip.MOV")));
        expect(codecs::exportFormats().size() >= 9);
        directory.deleteRecursively();
    }
};

ArrangementEditTests arrangementEditTests;
ArrangementEngineTests arrangementEngineTests;
TrackFxTests trackFxTests;
GraphicEqTests graphicEqTests;
RubberBandTests rubberBandTests;
CodecTests codecTests;
} // namespace
