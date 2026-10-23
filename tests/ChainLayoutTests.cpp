#include <juce_core/juce_core.h>

#include "audio/ChainNodes.h"
#include "audio/plugins/PluginCatalog.h"
#include "presets/PresetManager.h"

namespace
{
// The editable signal chain: order, blocks beside it, links and the
// migration of sounds saved with the fixed chain.
class ChainLayoutTests final : public juce::UnitTest
{
public:
    ChainLayoutTests() : UnitTest("Signal chain layout", "Presets") {}

    static juce::StringArray order(const chain::Layout& layout)
    {
        juce::StringArray ids;
        for (const auto& node : layout.serial)
            ids.add(node.id);
        return ids;
    }

    static juce::ValueTree plugin(const juce::String& name)
    {
        juce::ValueTree state("Vst3Slot");
        state.setProperty("version", 1, nullptr);
        state.setProperty("descriptionXml", "<PLUGIN name=\"" + name + "\"/>", nullptr);
        state.setProperty("stateBase64", "AAAA", nullptr);
        return state;
    }

    void runTest() override
    {
        beginTest("Standard order matches the classic chain");
        const auto standard = chain::Layout::standard();
        expectEquals(order(standard).joinIntoString(","),
                     juce::String("pitch,wah,drive,pedal,amp,ir,ampEq,geq,compressor,autoWah,chorus,delay,reverb"));
        expect(chain::Layout::standard(true).compressorBeforeAmp());
        expect(! standard.compressorBeforeAmp());

        beginTest("Moving blocks");
        auto layout = standard;
        layout.moveBefore("delay", "drive");
        expectEquals(layout.serialIndex("delay"), layout.serialIndex("drive") - 1);
        layout.moveAfter("pitch", "reverb");
        expectEquals(layout.serial.back().id, juce::String("pitch"));
        layout.setCompressorBeforeAmp(true);
        expect(layout.compressorBeforeAmp());
        expectEquals(layout.serial.size(), standard.serial.size());

        beginTest("Plugins beside the chain, links and their repair");
        const auto synth = layout.add({ {}, "vst3", "Vital", plugin("Vital"), false, 0 }, -1);
        const auto tracker = layout.add({ {}, "vst3", "TONIQ", plugin("TONIQ"), false, 0 }, -1);
        expect(! layout.isSerial(synth));
        layout.addLink({ chain::input, tracker, false, 0.0f });
        layout.addLink({ tracker, synth, true, 0.0f });
        layout.addLink({ synth, chain::output, false, -6.0f });
        layout.addLink({ synth, chain::output, false, -3.0f }); // replaces the level
        layout.addLink({ "delay", "reverb", true, 0.0f });       // MIDI into a built-in: refused
        layout.addLink({ chain::keys, "drive", false, 0.0f });   // keyboards carry no audio: refused
        expectEquals(static_cast<int>(layout.links.size()), 3);
        expectWithinAbsoluteError(layout.links.back().gainDb, -3.0f, 0.001f);
        layout.moveTo(tracker, 2);
        expect(layout.isSerial(tracker));
        layout.moveToSide(tracker);
        expect(! layout.isSerial(tracker));
        layout.moveToSide("amp"); // built-ins stay in the chain
        expect(layout.isSerial("amp"));
        layout.remove("amp");
        expect(layout.find("amp") != nullptr);

        beginTest("Round trip through the preset format");
        layout.chainLevelDb = -60.0f;
        const auto restored = chain::Layout::fromValueTree(layout.toValueTree());
        expect(restored == layout);
        SoundState sound;
        sound.layout = layout;
        const auto viaSound = SoundState::fromValueTree(sound.toValueTree());
        expect(viaSound.has_value() && viaSound->layout == layout);

        beginTest("Removing a block removes its links");
        layout.remove(synth);
        expectEquals(static_cast<int>(layout.links.size()), 1); // input -> tracker
        expect(layout.find(synth) == nullptr);

        beginTest("Damaged layouts are repaired");
        auto tree = standard.toValueTree();
        auto serial = tree.getChildWithName("serial");
        serial.removeChild(serial.getChildWithProperty("id", "ir"), nullptr);
        juce::ValueTree unknown("node");
        unknown.setProperty("id", "x", nullptr);
        unknown.setProperty("kind", "flux-capacitor", nullptr);
        serial.addChild(unknown, -1, nullptr);
        const auto repaired = chain::Layout::fromValueTree(tree);
        expectEquals(order(repaired).joinIntoString(","), order(standard).joinIntoString(","));

        beginTest("Built-in blocks keep stereo while switched off");
        {
            bool on = false;
            chainnodes::Builtin block([](float* mono, int count) { juce::FloatVectorOperations::multiply(mono, 0.5f, count); },
                                      [&on] { return on; });
            juce::AudioBuffer<float> audio(2, 512);
            juce::MidiBuffer midi;
            const auto run = [&]
            {
                for (int pass = 0; pass < 4; ++pass) // past the side ramp
                {
                    juce::FloatVectorOperations::fill(audio.getWritePointer(0), 1.0f, 512);
                    juce::FloatVectorOperations::fill(audio.getWritePointer(1), -0.5f, 512);
                    block.processBlock(audio, midi);
                }
            };
            run();
            // Mid 0.25 * 0.5 = 0.125, side 0.75 passes.
            expectWithinAbsoluteError(audio.getSample(0, 511), 0.875f, 1.0e-5f);
            expectWithinAbsoluteError(audio.getSample(1, 511), -0.625f, 1.0e-5f);
            on = true;
            run();
            // Switched on: the mono effect feeds both sides.
            expectWithinAbsoluteError(audio.getSample(0, 511), 0.125f, 1.0e-5f);
            expectWithinAbsoluteError(audio.getSample(1, 511), 0.125f, 1.0e-5f);
        }

        beginTest("Plugin catalog: scans merge per file, hidden plugins stay out of the menus");
        {
            const auto plugin = [](const juce::String& name, const juce::String& file, bool instrument)
            {
                juce::PluginDescription description;
                description.name = name;
                description.pluginFormatName = "VST3";
                description.fileOrIdentifier = file;
                description.uniqueId = name.hashCode();
                description.isInstrument = instrument;
                return description;
            };
            PluginCatalog catalog;
            catalog.merge({ "/a/Vital.vst3", "/a/Toniq.vst3", "/a/Broken.vst3" },
                          { { plugin("Vital", "/a/Vital.vst3", true), plugin("TONIQ", "/a/Toniq.vst3", false) },
                            { { "/a/Broken.vst3", "crash" } } });
            expectEquals(catalog.plugins.size(), 2);
            expectEquals(static_cast<int>(catalog.failures.size()), 1);
            expectEquals(catalog.instruments().size(), 1);
            // Rescanning one file replaces only its entries.
            catalog.merge({ "/a/Broken.vst3" }, { { plugin("Fixed", "/a/Broken.vst3", false) }, {} });
            expectEquals(catalog.plugins.size(), 3);
            expect(catalog.failures.empty());
            catalog.setHidden(catalog.plugins[0], true);
            expectEquals(catalog.visible().size(), 2);
            catalog.files.add("/b/Imported.vst3");
            catalog.folders.add("/a");
            catalog.merge({ "/b/Imported.vst3" }, { { plugin("Imported", "/b/Imported.vst3", false) }, {} });

            juce::ValueTree uiState("uiState");
            uiState.appendChild(catalog.toValueTree(), nullptr);
            auto loaded = PluginCatalog::fromUiState(uiState);
            expectEquals(loaded.plugins.size(), 4);
            expectEquals(loaded.visible().size(), 3);
            expect(loaded.files.contains("/b/Imported.vst3") && loaded.folders.contains("/a"));
            loaded.removeFolder("/a");
            expectEquals(loaded.plugins.size(), 1); // only the imported file is left
            loaded.removeFile("/b/Imported.vst3");
            expect(loaded.plugins.isEmpty() && loaded.files.isEmpty());

            // The plain list of earlier versions is read too.
            juce::ValueTree old("uiState"), list("vst3Catalog");
            list.appendChild(juce::ValueTree::fromXml(*plugin("Vital", "/a/Vital.vst3", true).createXml()), nullptr);
            old.appendChild(list, nullptr);
            expectEquals(PluginCatalog::fromUiState(old).plugins.size(), 1);

            const auto bundle = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("catalog-test/Synth.vst3");
            const auto inner = bundle.getChildFile("Contents/x86_64-win/Synth.vst3");
            expect(inner.create().wasOk());
            expectEquals(PluginCatalog::bundleOf(inner).getFullPathName(), bundle.getFullPathName());
            bundle.getParentDirectory().deleteRecursively();
        }

        beginTest("Sounds saved with the fixed chain become a layout");
        auto old = SoundState().toValueTree();
        old.removeChild(old.getChildWithName("chainLayout"), nullptr);
        old.setProperty("compressorPreAmp", true, nullptr);
        juce::ValueTree pre("preVst3"), post("postVst3");
        pre.addChild(plugin("BYOD"), -1, nullptr);
        auto bypassedPost = plugin("Supermassive");
        bypassedPost.setProperty("bypassed", true, nullptr);
        post.addChild(bypassedPost, -1, nullptr);
        old.addChild(pre, -1, nullptr);
        old.addChild(post, -1, nullptr);
        const auto migrated = SoundState::fromValueTree(old);
        expect(migrated.has_value());
        if (migrated)
        {
            const auto& l = migrated->layout;
            expect(l.compressorBeforeAmp());
            expectEquals(static_cast<int>(l.serial.size()), 15);
            const auto byod = l.serial[static_cast<size_t>(l.serialIndex("pedal") - 1)];
            expectEquals(byod.kind, juce::String("vst3"));
            expectEquals(byod.name, juce::String("BYOD"));
            expect(! byod.bypassed);
            expectEquals(l.serial.back().name, juce::String("Supermassive"));
            expect(l.serial.back().bypassed);
        }
    }
};

ChainLayoutTests chainLayoutTests;
} // namespace
