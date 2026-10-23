#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "daw/SampleKit.h"

namespace
{
using daw::SampleKit;

// The bundled acoustic kits: the SFZ subset parses, every sample exists,
// each drum lane sounds, velocity picks layers and the hi-hat chokes.
class DrumKitTests final : public juce::UnitTest
{
public:
    DrumKitTests() : UnitTest("Bundled drum kits", "DAW") {}

    static float peak(SampleKit& kit, int note, int velocity, int length = 4800)
    {
        std::vector<float> left(static_cast<size_t>(length)), right(static_cast<size_t>(length));
        const daw::DrumEvent event { 0, static_cast<juce::uint8>(note), static_cast<juce::uint8>(velocity) };
        kit.render(&event, 1, left.data(), right.data(), length);
        float value = 0.0f;
        for (size_t index = 0; index < left.size(); ++index)
            value = std::max({ value, std::abs(left[index]), std::abs(right[index]) });
        return value;
    }

    static float energy(SampleKit& kit, int length)
    {
        std::vector<float> left(static_cast<size_t>(length)), right(static_cast<size_t>(length));
        kit.render(nullptr, 0, left.data(), right.data(), length);
        float sum = 0.0f;
        for (size_t index = 0; index < left.size(); ++index)
            sum += left[index] * left[index] + right[index] * right[index];
        return sum;
    }

    void runTest() override
    {
        beginTest("SFZ parser");
        {
            const auto regions = SampleKit::parseSfz("// comment\r\n<group> key=36 pan=-20 loop_mode=one_shot\r\n"
                                                     "<region> lovel=1 hivel=64 sample=Samples/Kick Soft.flac\r\n"
                                                     "<region> lovel=65 sample=Samples/Kick-Hard.flac\r\n"
                                                     "<group> group=2 key=46 off_by=1\r\n<region> sample=a.wav\r\n");
            expectEquals(static_cast<int>(regions.size()), 3);
            if (regions.size() == 3)
            {
                expectEquals(regions[0].loKey, 36);
                expectEquals(regions[0].hiVel, 64);
                expectEquals(regions[0].sample, juce::String("Samples/Kick Soft.flac"));
                expectEquals(regions[1].loVel, 65);
                expectEquals(regions[1].hiVel, 127);
                expectWithinAbsoluteError(regions[1].pan, -20.0f, 0.001f);
                expectEquals(regions[2].group, 2);
                expectEquals(regions[2].offBy, 1);
                expectEquals(regions[2].loKey, 46);
            }
        }

        const auto kits = juce::File(TON_OF_ABLE_RESOURCES_DIR).getChildFile("drums");
        expect(kits.getChildFile("AVL-Drumkits/LICENSE").existsAsFile(), "Licence must ship with the samples");
        for (const auto& info : SampleKit::bundledKits())
        {
            beginTest(juce::String("Kit plays: ") + info.name);
            juce::String error;
            auto kit = SampleKit::load(kits, info.id, error);
            expect(kit != nullptr, error);
            if (kit == nullptr)
                continue;
            kit->prepare(48000.0, 512);
            for (const auto& lane : daw::drumLanes())
                expectGreaterThan(peak(*kit, lane.note, 110), 0.02f, juce::String(lane.name) + " is silent");
            energy(*kit, 48000 * 4); // let everything ring out

            const auto soft = peak(*kit, 38, 20);
            energy(*kit, 48000 * 2);
            const auto hard = peak(*kit, 38, 127);
            expectGreaterThan(hard, soft * 1.5f, "velocity layers");
            energy(*kit, 48000 * 2);

            // Open hi-hat rings; a closed hi-hat stops it.
            peak(*kit, 46, 110, 480);
            const auto ringing = energy(*kit, 4800);
            peak(*kit, 46, 110, 480);
            peak(*kit, 42, 1, 480);  // choke with a quiet closed hat
            const auto choked = energy(*kit, 4800);
            expectGreaterThan(ringing, choked * 2.0f, "closed hi-hat chokes the open one");

            // Kick, snare, crash and hi-hat at full force together must not clip.
            {
                energy(*kit, 48000 * 4);
                const daw::DrumEvent hits[] { { 0, 36, 127 }, { 0, 38, 127 }, { 0, 49, 127 }, { 0, 42, 127 } };
                std::vector<float> left(9600), right(9600);
                kit->render(hits, 4, left.data(), right.data(), 9600);
                float loudest = 0.0f;
                for (size_t index = 0; index < left.size(); ++index)
                    loudest = std::max({ loudest, std::abs(left[index]), std::abs(right[index]) });
                logMessage(juce::String("Full hit peak: ") + juce::String(loudest, 3));
                expectLessThan(loudest, 1.0f, "full hit clips");
                expectGreaterThan(loudest, 0.25f, "kit is too quiet");
            }

            beginTest(juce::String("Kit at 44.1 kHz and export copy: ") + info.name);
            auto copy = std::static_pointer_cast<SampleKit>(kit->createOfflineCopy());
            copy->prepare(44100.0, 256);
            expectGreaterThan(peak(*copy, 36, 100), 0.02f);
        }

        beginTest("New drum tracks use the acoustic kit");
        expectEquals(SampleKit::settingsFor(SampleKit::defaultKitId())["type"].toString(), juce::String("kit"));
        expect(SampleKit::findKit(SampleKit::defaultKitId()) != nullptr);
    }
};

DrumKitTests drumKitTests;
} // namespace
