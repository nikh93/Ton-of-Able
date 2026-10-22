#include <array>
#include <cmath>
#include <limits>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include "../src/audio/effects/Effects.h"
#include "../src/diagnostics/PerformanceMonitor.h"
#include "../src/library/LibraryManager.h"
#include "../src/library/CuratedAmpLibrary.h"
#include "../src/playback/FilePlayer.h"
#include "../src/presets/PresetManager.h"

namespace
{
class TemporaryDirectory final
{
public:
    TemporaryDirectory()
        : directory (juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("NamTestProgramTests-" + juce::Uuid().toString()))
    {
        directory.createDirectory();
    }

    ~TemporaryDirectory() { directory.deleteRecursively(); }

    juce::File getFile (const juce::String& name) const { return directory.getChildFile (name); }

private:
    juce::File directory;
};

void writeMonoWav (const juce::File& file, const std::array<float, 4>& samples)
{
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    jassert (stream != nullptr);
    const auto options = juce::AudioFormatWriterOptions {}
                             .withSampleRate (48000.0)
                             .withNumChannels (1)
                             .withBitsPerSample (16);
    auto writer = format.createWriterFor (stream, options);
    jassert (writer != nullptr);

    juce::AudioBuffer<float> buffer (1, static_cast<int> (samples.size()));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample (0, i, samples[static_cast<size_t> (i)]);
    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

void expectFiniteBuffer (juce::UnitTest& test, const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            test.expect (std::isfinite (buffer.getSample (channel, sample)));
}

class LibraryManagerTests final : public juce::UnitTest
{
public:
    LibraryManagerTests() : UnitTest ("LibraryManager serialization and relinking", "NAM Test Program") {}

    void runTest() override
    {
        beginTest ("ValueTree and JSON retain linked metadata");
        TemporaryDirectory temporary;
        const auto original = temporary.getFile ("British Crunch.nam");
        expect (original.replaceWithText ("fixture"));

        LibraryManager library ("nam", { ".nam" });
        juce::String error;
        const auto id = library.importLink (original, { "  amp ", "amp", "capture" }, R"({"author":"test"})", &error);
        expect (id.has_value(), error);
        expectEquals (library.items().size(), 1);
        expectEquals (library.items().getFirst().tags.joinIntoString (","), juce::String ("amp,capture"));

        LibraryManager fromTree ("nam", { ".nam" });
        expect (fromTree.loadValueTree (library.toValueTree(), &error), error);
        expectEquals (fromTree.items().size(), 1);
        expectEquals (fromTree.items().getFirst().displayName, juce::String ("British Crunch"));
        expectEquals (fromTree.items().getFirst().metadataJson, juce::String (R"({"author":"test"})"));

        LibraryManager fromJson ("nam", { ".nam" });
        expect (fromJson.loadJson (library.toJson(), &error), error);
        expectEquals (fromJson.items().size(), 1);
        expect (fromJson.items().getFirst().linkedFile == original);

        beginTest ("Missing links persist and can be relinked");
        expect (original.deleteFile());
        expect (library.find (*id)->isMissing());
        const auto replacement = temporary.getFile ("replacement.nam");
        expect (replacement.replaceWithText ("fixture"));
        expect (library.relink (*id, replacement, &error), error);
        expect (! library.find (*id)->isMissing());
        expect (library.find (*id)->linkedFile == replacement);
    }
};

class PresetManagerTests final : public juce::UnitTest
{
public:
    PresetManagerTests() : UnitTest ("PresetManager serialization", "NAM Test Program") {}

    void runTest() override
    {
        beginTest ("Preset and sound state survive ValueTree and JSON round trips");
        SoundState sound;
        sound.namId = "nam-42";
        sound.irId = "ir-99";
        sound.ampId = "marshall-jcm2000";
        sound.configurationId = "crunch";
        sound.inputGainDb = -4.5f;
        sound.delayTimeMs = 777.0f;
        sound.reverbMix = 0.73f;
        sound.chorusBypassed = false;

        PresetManager manager;
        const auto id = manager.create ("  Lead  ", sound);
        expect (id.has_value());
        expectEquals (manager.find (*id)->name, juce::String ("Lead"));

        juce::String error;
        auto tree = manager.toValueTree();
        PresetManager fromTree;
        expect (fromTree.loadValueTree (tree, &error), error);
        const auto* loaded = fromTree.find (*id);
        expect (loaded != nullptr);
        if (loaded != nullptr)
        {
            expectEquals (loaded->sound.namId, sound.namId);
            expectEquals (loaded->sound.ampId, sound.ampId);
            expectEquals (loaded->sound.configurationId, sound.configurationId);
            expectWithinAbsoluteError (loaded->sound.delayTimeMs, sound.delayTimeMs, 0.0001f);
            expectWithinAbsoluteError (loaded->sound.reverbMix, sound.reverbMix, 0.0001f);
            expect (! loaded->sound.chorusBypassed);
        }

        const auto json = PresetManager::valueTreeToJson (tree);
        const auto rebuiltTree = PresetManager::valueTreeFromJson (json, &error);
        expect (rebuiltTree.has_value(), error);
        PresetManager fromJson;
        if (rebuiltTree)
            expect (fromJson.loadValueTree (*rebuiltTree, &error), error);
        expect (fromJson.find (*id) != nullptr);

        beginTest ("A/B state retains both independent sounds");
        ABState ab { sound, SoundState::defaults(), false };
        ab.b.outputGainDb = 6.0f;
        const auto restored = ABState::fromValueTree (ab.toValueTree(), &error);
        expect (restored.has_value(), error);
        if (restored)
        {
            expect (! restored->usingA);
            expectEquals (restored->a.namId, sound.namId);
            expectWithinAbsoluteError (restored->b.outputGainDb, 6.0f, 0.0001f);
        }
    }
};

class CuratedAmpLibraryTests final : public juce::UnitTest
{
public:
    CuratedAmpLibraryTests() : UnitTest ("Curated amp library metadata", "NAM Test Program") {}

    void runTest() override
    {
        TemporaryDirectory temporary;
        const auto catalog = temporary.getFile ("catalog.json");
        expect (catalog.replaceWithText (R"({"version":1,"models":[{"ampId":"vox-ac15","ampName":"AC15","manufacturer":"Vox","configurationId":"top-boost","configurationName":"Top Boost","captureType":"AmpHead","category":"British Rock","gainLevel":"Crunch","namFile":"vox.nam","recommendedIrFile":"cab.wav","description":"test","tags":["British Rock","Crunch"]}]})"));
        CuratedAmpLibrary library;
        juce::String error;
        expect (library.load (catalog, &error), error);
        expectEquals (library.models().size(), 1);
        const auto* model = library.find ("vox-ac15", "top-boost");
        expect (model != nullptr);
        if (model != nullptr)
        {
            expectEquals (model->captureType, juce::String ("AmpHead"));
            expectEquals (model->recommendedIrFile, juce::String ("cab.wav"));
            expect (model->tags.contains ("Crunch"));
        }
    }
};

class FilePlayerTests final : public juce::UnitTest
{
public:
    FilePlayerTests() : UnitTest ("FilePlayer state behavior", "NAM Test Program") {}

    void runTest() override
    {
        TemporaryDirectory temporary;
        const auto wav = temporary.getFile ("mono-fixture.wav");
        writeMonoWav (wav, { 0.0f, 0.5f, -0.5f, 1.0f });

        FilePlayer player;
        beginTest ("Load leaves a mono WAV stopped at the beginning");
        const auto result = player.load (wav);
        expect (result.wasOk(), result.getErrorMessage());
        auto status = player.getStatus();
        expect (status.hasFile);
        expect (status.state == FilePlayer::State::stopped);
        expectEquals (status.positionSamples, static_cast<juce::int64> (0));
        expectEquals (status.lengthSamples, static_cast<juce::int64> (4));
        expectWithinAbsoluteError (status.sampleRate, 48000.0, 0.01);

        beginTest ("Play, end, stop, pause and looping are deterministic");
        std::array<float, 6> output {};
        player.play();
        player.process (output.data(), static_cast<int> (output.size()));
        expectWithinAbsoluteError (output[0], 0.0f, 0.0001f);
        expectWithinAbsoluteError (output[1], 0.5f, 0.0001f);
        expectWithinAbsoluteError (output[2], -0.5f, 0.0001f);
        expectWithinAbsoluteError (output[3], 1.0f, 0.0001f);
        expectWithinAbsoluteError (output[4], 0.0f, 0.0001f);
        expect (player.getStatus().state == FilePlayer::State::stopped);

        player.stop();
        player.pause();
        output.fill (9.0f);
        player.process (output.data(), static_cast<int> (output.size()));
        expectWithinAbsoluteError (output[0], 0.0f, 0.0001f);

        player.stop();
        player.setLooping (true);
        player.play();
        player.process (output.data(), static_cast<int> (output.size()));
        expectWithinAbsoluteError (output[4], 0.0f, 0.0001f);
        expectWithinAbsoluteError (output[5], 0.5f, 0.0001f);
        expect (player.getStatus().state == FilePlayer::State::playing);

        for (const auto bits : { 16, 24, 32 })
        {
            beginTest ("Stereo WAV downmix, block boundaries and looping / " + juce::String(bits) + " bit");
            const auto stereo = temporary.getFile ("stereo-" + juce::String(bits) + ".wav");
            constexpr int length = 8201; // Includes a partial final decode block.
            const std::array<float, 4> left { 0.5f, 0.0f, 0.5f, -0.5f };
            const std::array<float, 4> right { 0.5f, 0.5f, -0.5f, -0.25f };
            const std::array<float, 4> expected { 0.5f, 0.25f, 0.0f, -0.375f };
            {
                juce::WavAudioFormat format;
                std::unique_ptr<juce::OutputStream> stream (stereo.createOutputStream());
                auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                    .withSampleRate (44100.0).withNumChannels (2).withBitsPerSample (bits));
                expect (writer != nullptr);
                if (writer == nullptr) continue;
                juce::AudioBuffer<float> input (2, length);
                for (int i = 0; i < length; ++i)
                {
                    input.setSample (0, i, left[i % 4]);
                    input.setSample (1, i, right[i % 4]);
                }
                expect (writer->writeFromAudioSampleBuffer (input, 0, length));
            }
            FilePlayer stereoPlayer;
            expect (stereoPlayer.load (stereo).wasOk());
            expectEquals (stereoPlayer.getStatus().lengthSamples, static_cast<juce::int64>(length));
            stereoPlayer.setLooping (true);
            stereoPlayer.play();
            std::vector<float> rendered (length + 7);
            stereoPlayer.process (rendered.data(), static_cast<int>(rendered.size()));
            for (int i = 0; i < static_cast<int>(rendered.size()); ++i)
                expectWithinAbsoluteError (rendered[i], expected[(i % length) % 4], 0.0001f);
            expect (stereoPlayer.setOutputSampleRate (48000.0).wasOk());
            expectEquals (stereoPlayer.getStatus().lengthSamples,
                          static_cast<juce::int64>(std::ceil(length * 48000.0 / 44100.0)));
            stereoPlayer.play();
            stereoPlayer.process (rendered.data(), static_cast<int>(rendered.size()));
            for (const auto sample : rendered) expect (std::isfinite(sample));
            expect (stereoPlayer.load (temporary.getFile("missing.wav")).failed());
            expectEquals (stereoPlayer.getStatus().fileName, stereo.getFileName());
        }

        // Optional integration fixture, without a machine-specific path in the suite.
        const auto integrationPath = juce::SystemStats::getEnvironmentVariable("NAM_TEST_STEREO_WAV", "");
        if (integrationPath.isNotEmpty())
        {
            beginTest ("User stereo WAV decodes and plays at 48 kHz");
            FilePlayer integration;
            expect (integration.setOutputSampleRate (48000.0).wasOk());
            const auto loaded = integration.load (juce::File(integrationPath));
            expect (loaded.wasOk(), loaded.getErrorMessage());
            integration.play();
            std::array<float, 512> block {};
            double energy = 0.0;
            while (integration.getStatus().state == FilePlayer::State::playing)
            {
                integration.process (block.data(), static_cast<int>(block.size()));
                for (const auto sample : block)
                {
                    expect (std::isfinite(sample));
                    energy += sample * sample;
                }
            }
            expect (energy > 0.0);
        }
    }
};

class PerformanceMonitorTests final : public juce::UnitTest
{
public:
    PerformanceMonitorTests() : UnitTest ("PerformanceMonitor aggregate math", "NAM Test Program") {}

    void runTest() override
    {
        PerformanceMonitor monitor;
        beginTest ("Empty and reset snapshots are zero");
        auto snapshot = monitor.snapshot();
        expectEquals (snapshot.processedBlocks, 0ULL);
        expectWithinAbsoluteError (snapshot.averageMilliseconds, 0.0, 0.0);
        expectWithinAbsoluteError (snapshot.peakMilliseconds, 0.0, 0.0);
        expectWithinAbsoluteError (snapshot.realtimeLoad, 0.0, 0.0);

        beginTest ("Block count, peak, average and xrun counters remain coherent");
        monitor.beginBlock(); monitor.endBlock (480, 48000.0);
        monitor.beginBlock(); monitor.endBlock (0, 0.0);
        monitor.reportXrun(); monitor.reportXrun();
        snapshot = monitor.snapshot();
        expectEquals (snapshot.processedBlocks, 2ULL);
        expectEquals (snapshot.xruns, 2ULL);
        expect (std::isfinite (snapshot.averageMilliseconds) && snapshot.averageMilliseconds >= 0.0);
        expect (std::isfinite (snapshot.peakMilliseconds) && snapshot.peakMilliseconds >= snapshot.averageMilliseconds);
        expect (std::isfinite (snapshot.realtimeLoad) && snapshot.realtimeLoad >= 0.0);
        monitor.reset();
        snapshot = monitor.snapshot();
        expectEquals (snapshot.processedBlocks, 0ULL);
        expectEquals (snapshot.xruns, 0ULL);
    }
};

class EffectParameterTests final : public juce::UnitTest
{
public:
    EffectParameterTests() : UnitTest ("Effect parameter bounds and smoothing", "NAM Test Program") {}

    void runTest() override
    {
        beginTest ("Shared linear smoothing reaches a target gradually");
        nam::effects::SmoothedParameter parameter;
        parameter.prepare (1000.0, 0.010, 0.0f);
        parameter.set (1.0f);
        const auto first = parameter.next();
        expect (first > 0.0f && first < 1.0f);
        float final = first;
        for (int i = 0; i < 9; ++i) final = parameter.next();
        expectWithinAbsoluteError (final, 1.0f, 0.0001f);

        beginTest ("Bypass uses the documented ten millisecond crossfade");
        nam::effects::BypassRamp bypass;
        bypass.prepare (1000.0, false);
        bypass.setBypassed (true);
        const auto firstWet = bypass.nextWetMix();
        expect (firstWet > 0.0f && firstWet < 1.0f);
        float lastWet = firstWet;
        for (int i = 0; i < 9; ++i) lastWet = bypass.nextWetMix();
        expectWithinAbsoluteError (lastWet, 0.0f, 0.0001f);

        beginTest ("Extreme public effect parameters produce finite audio");
        juce::dsp::ProcessSpec spec { 48000.0, 128, 2 };
        juce::AudioBuffer<float> source (2, 128);
        for (int i = 0; i < source.getNumSamples(); ++i)
            source.setSample (0, i, i == 0 ? 1.0f : 0.0f), source.setSample (1, i, i == 0 ? -1.0f : 0.0f);

        nam::effects::EQProcessor eq; eq.prepare (spec); eq.setLowGainDb (-999.0f); eq.setMidGainDb (999.0f); eq.setHighGainDb (std::numeric_limits<float>::infinity()); eq.process (source); expectFiniteBuffer (*this, source);
        source.clear(); source.setSample (0, 0, 1.0f);
        nam::effects::AutoWahProcessor wah; wah.prepare (spec); wah.setSensitivity (-5.0f); wah.setResonance (999.0f); wah.process (source); expectFiniteBuffer (*this, source);
        source.clear(); source.setSample (0, 0, 1.0f);
        nam::effects::ChorusProcessor chorus; chorus.prepare (spec); chorus.setRateHz (999.0f); chorus.setDepth (-1.0f); chorus.setMix (2.0f); chorus.process (source); expectFiniteBuffer (*this, source);
        source.clear(); source.setSample (0, 0, 1.0f);
        nam::effects::DelayProcessor delay; delay.prepare (spec); delay.setTimeSeconds (999.0f); delay.setFeedback (999.0f); delay.setMix (-1.0f); delay.process (source); expectFiniteBuffer (*this, source);
        source.clear(); source.setSample (0, 0, 1.0f);
        nam::effects::ReverbProcessor reverb; reverb.prepare (spec); reverb.setRoomSize (-1.0f); reverb.setDamping (2.0f); reverb.setMix (999.0f); reverb.process (source); expectFiniteBuffer (*this, source);
    }
};

class DriveProcessorTests final : public juce::UnitTest
{
public:
    DriveProcessorTests() : UnitTest ("Drive processor stability and voicing", "NAM Test Program") {}

    void runTest() override
    {
        beginTest ("All drive voices remain finite at supported rates and block sizes");
        for (const auto sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            for (const auto blockSize : { 32, 64, 128, 512 })
            {
                nam::effects::DriveProcessor drive;
                drive.prepare ({ sampleRate, static_cast<juce::uint32> (blockSize), 1 });
                drive.setBypassed (false);
                drive.setDrive (1.0f); drive.setTone (1.0f); drive.setWarmth (1.0f);
                drive.setPresence (1.0f); drive.setSustain (1.0f); drive.setBias (1.0f);
                drive.setLevel (1.5f); drive.setMix (1.0f);

                for (int voice = 0; voice < 4; ++voice)
                {
                    drive.setType (static_cast<nam::effects::DriveProcessor::DriveType> (voice));
                    for (int block = 0; block < 6; ++block)
                    {
                        juce::AudioBuffer<float> source (1, blockSize);
                        for (int sample = 0; sample < blockSize; ++sample)
                        {
                            const auto phase = static_cast<float> ((block * blockSize + sample) * 2.0 * juce::MathConstants<double>::pi * 220.0 / sampleRate);
                            source.setSample (0, sample, 0.82f * std::sin (phase) + (sample == 0 ? 0.35f : 0.0f));
                        }
                        drive.process (source);
                        expectFiniteBuffer (*this, source);
                    }
                }
            }
        }

        beginTest ("Drive voices produce distinctly voiced output");
        std::array<float, 4> signatures {};
        for (int voice = 0; voice < 4; ++voice)
        {
            nam::effects::DriveProcessor drive;
            drive.prepare ({ 48000.0, 256, 1 });
            drive.setBypassed (false);
            drive.setType (static_cast<nam::effects::DriveProcessor::DriveType> (voice));
            drive.setDrive (.72f); drive.setTone (.58f); drive.setWarmth (.75f);
            drive.setPresence (.7f); drive.setSustain (.65f); drive.setBias (.8f); drive.setLevel (1.0f);
            juce::AudioBuffer<float> source (1, 256);
            for (int block = 0; block < 5; ++block)
            {
                for (int sample = 0; sample < source.getNumSamples(); ++sample)
                    source.setSample (0, sample, .7f * std::sin (static_cast<float> ((block * 256 + sample) * 2.0 * juce::MathConstants<double>::pi * 440.0 / 48000.0)));
                drive.process (source);
            }
            signatures[static_cast<size_t> (voice)] = source.getRMSLevel (0, 0, source.getNumSamples());
        }
        expect (std::abs (signatures[0] - signatures[1]) > 0.003f);
        expect (std::abs (signatures[1] - signatures[2]) > 0.003f);
        expect (std::abs (signatures[2] - signatures[3]) > 0.003f);
    }
};

class EffectRegressionTests final : public juce::UnitTest
{
public:
    EffectRegressionTests() : UnitTest("Effect regressions", "NAM Test Program") {}
    void runTest() override
    {
        beginTest("Rack edit preserves live amp gains, EQ and assets");
        SoundState live, edited;
        live.inputGainDb = 8.f; live.outputGainDb = -11.f; live.bassDb = 4.f;
        live.namBypassed = true; live.namId = "live-model"; live.ampId = "live-amp";
        edited.reverbDecay = 7.f; edited.reverbBypassed = false;
        edited.retainAmpFrom(live);
        expectEquals(edited.inputGainDb, 8.f); expectEquals(edited.outputGainDb, -11.f);
        expectEquals(edited.bassDb, 4.f); expect(edited.namBypassed);
        expectEquals(edited.namId, live.namId); expectEquals(edited.ampId, live.ampId);
        expectEquals(edited.reverbDecay, 7.f); expect(!edited.reverbBypassed);

        beginTest("Audio device restart preserves effect parameters and bypass");
        nam::effects::SmoothedParameter parameter;
        parameter.prepare(48000.0,.03,.2f); parameter.set(.8f);
        parameter.prepare(96000.0,.03,.2f);
        expectWithinAbsoluteError(parameter.next(),.8f,.0001f);
        nam::effects::BypassRamp bypass;
        bypass.prepare(48000.0,false); bypass.setBypassed(true);
        bypass.prepare(96000.0,false);
        expectEquals(bypass.nextWetMix(),0.f);

        for (double rate : {44100.0, 48000.0, 96000.0})
        {
            beginTest("Chorus delay milliseconds at " + juce::String(rate));
            nam::effects::ChorusProcessor chorus;
            chorus.prepare({rate, 256, 1}); chorus.setDepth(0.f); chorus.setCenterDelayMs(12.f);
            chorus.setMix(1.f); chorus.setFeedback(0.f);
            juce::AudioBuffer<float> block(1, 256);
            for (int n = 0; n < 40; ++n) { block.clear(); chorus.process(block); }
            int peakIndex = 0; float peak = 0.f;
            for (int n = 0; n < 6; ++n)
            {
                block.clear(); if (n == 0) block.setSample(0, 0, 1.f);
                chorus.process(block);
                for (int i = 0; i < 256; ++i)
                    if (std::abs(block.getSample(0,i)) > peak) { peak = std::abs(block.getSample(0,i)); peakIndex = n*256+i; }
            }
            expect(std::abs(peakIndex - juce::roundToInt(rate * .012)) <= 1);

            beginTest("Auto-wah remains bounded at maximum Q and responds to direction at " + juce::String(rate));
            nam::effects::AutoWahProcessor up, down;
            up.prepare({rate,256,1}); down.prepare({rate,256,1});
            for (auto* wah : {&up, &down})
            {
                wah->setSensitivity(.8f); wah->setResonance(12.f);
                wah->setFrequencyRange(180.f,8000.f); wah->setMix(1.f);
            }
            down.setDirectionUp(false);
            juce::AudioBuffer<float> other(1,256);
            double difference = 0, energy = 0; bool bounded = true;
            for (int n = 0; n < 100; ++n)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float t = static_cast<float>((n*256+i)/rate);
                    const float input = .03f * std::exp(-4.f*t) * (std::sin(2764.f*t) + .4f*std::sin(8294.f*t));
                    block.setSample(0,i,input); other.setSample(0,i,input);
                }
                up.process(block); down.process(other);
                for (int i = 0; i < 256; ++i)
                {
                    float a=block.getSample(0,i), b=other.getSample(0,i);
                    bounded &= std::isfinite(a) && std::isfinite(b) && std::abs(a)<2.f && std::abs(b)<2.f;
                    difference += (a-b)*(a-b); energy += a*a;
                }
            }
            expect(bounded); expect(energy > .001); expect(difference > .001);
        }

        beginTest("Cave decay retains more late energy than Room and silence stays silent");
        std::array<double,2> tail {};
        for (int setting=0; setting<2; ++setting)
        {
            nam::effects::ReverbProcessor reverb;
            reverb.prepare({48000.0,256,1}); reverb.setMix(1.f);
            reverb.setDecay(setting == 0 ? .7f : 7.f); reverb.setSize(setting == 0 ? .25f : 1.f);
            juce::AudioBuffer<float> block(1,256);
            for (int n=0;n<200;++n) { block.clear(); reverb.process(block); }
            expectEquals(block.getMagnitude(0,256), 0.f);
            for (int n=0;n<750;++n)
            {
                block.clear(); if (n == 0) block.setSample(0,0,1.f);
                reverb.process(block);
                for (int i=0;i<256;++i)
                {
                    const float value=block.getSample(0,i);
                    if (!std::isfinite(value)) { expect(false, "Non-finite reverb tail"); return; }
                    if (n>375) tail[setting] += value*value;
                }
            }
        }
        expect(tail[1] > tail[0]*10.0); expect(tail[1] > 1.0e-8);
    }
};

EffectRegressionTests effectRegressionTests;
LibraryManagerTests libraryManagerTests;
PresetManagerTests presetManagerTests;
CuratedAmpLibraryTests curatedAmpLibraryTests;
FilePlayerTests filePlayerTests;
PerformanceMonitorTests performanceMonitorTests;
EffectParameterTests effectParameterTests;
DriveProcessorTests driveProcessorTests;
} // namespace
