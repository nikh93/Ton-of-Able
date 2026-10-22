#include <cmath>
#include <filesystem>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "NAM/get_dsp.h"
#include "library/CuratedAmpLibrary.h"

namespace
{
// Every model in the shipped catalog must exist, load with the bundled NAM
// core and produce sound; every bundled IR must be a readable 48 kHz WAV.
class BundledModelTests final : public juce::UnitTest
{
public:
    BundledModelTests() : UnitTest("Bundled default models", "NAM Test Program") {}

    void runTest() override
    {
        const auto defaults = juce::File(TON_OF_ABLE_RESOURCES_DIR).getChildFile("defaults");
        CuratedAmpLibrary catalog;
        juce::String error;
        beginTest("Catalog loads");
        expect(catalog.load(defaults.getChildFile("curated_catalog.json"), &error), error);

        for (const auto& model : catalog.models())
        {
            beginTest("Model plays: " + model.ampName + " / " + model.configurationName);
            const auto file = defaults.getChildFile(model.namFile);
            expect(file.existsAsFile(), model.namFile);
            if (! file.existsAsFile())
                continue;
            if (model.recommendedIrFile.isNotEmpty())
                expect(defaults.getChildFile(model.recommendedIrFile).existsAsFile(), model.recommendedIrFile);
            try
            {
                nam::DspLoadOptions options;
                options.prewarm = false;
                auto dsp = nam::get_dsp(std::filesystem::path(file.getFullPathName().toWideCharPointer()), options);
                expect(dsp != nullptr);
                const auto expected = dsp->GetExpectedSampleRate();
                expect(expected <= 0.0 || std::abs(expected - 48000.0) < 0.5, juce::String(expected));
                constexpr int block = 512;
                dsp->Reset(48000.0, block);
                std::vector<NAM_SAMPLE> input(block), output(block);
                double energy = 0.0;
                bool finite = true;
                for (int pass = 0; pass < 20; ++pass)
                {
                    for (int sample = 0; sample < block; ++sample)
                        input[static_cast<size_t>(sample)] = static_cast<NAM_SAMPLE>(0.2 * std::sin(2.0 * juce::MathConstants<double>::pi * 110.0 * (pass * block + sample) / 48000.0));
                    NAM_SAMPLE* in[] { input.data() };
                    NAM_SAMPLE* out[] { output.data() };
                    dsp->process(in, out, block);
                    for (const auto value : output)
                    {
                        finite = finite && std::isfinite(value);
                        energy += static_cast<double>(value) * value;
                    }
                }
                expect(finite, "non-finite output");
                expect(energy > 1.0e-3, "silent output");
            }
            catch (const std::exception& exception)
            {
                expect(false, model.namFile + ": " + exception.what());
            }
        }

        beginTest("Bundled IRs are readable 48 kHz WAVs");
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        int count = 0;
        for (const auto& entry : juce::RangedDirectoryIterator(defaults, false, "*.wav", juce::File::findFiles))
        {
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(entry.getFile()));
            expect(reader != nullptr, entry.getFile().getFileName());
            if (reader != nullptr)
            {
                expectWithinAbsoluteError(reader->sampleRate, 48000.0, 0.5);
                expect(reader->lengthInSamples > 0);
            }
            ++count;
        }
        expect(count >= 7, juce::String(count));
    }
};

BundledModelTests bundledModelTests;
} // namespace
