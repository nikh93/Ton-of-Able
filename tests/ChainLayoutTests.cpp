#include <juce_core/juce_core.h>

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
