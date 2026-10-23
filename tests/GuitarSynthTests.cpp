#include <juce_audio_processors/juce_audio_processors.h>

#include "audio/plugins/GuitarSynth.h"

namespace
{
// Stand-ins for real plugins: a tracker that sends note 40 while the guitar
// is loud and a synth that writes a constant per held note, so the routing
// can be checked sample by sample.
class FakePlugin : public juce::AudioPluginInstance
{
public:
    explicit FakePlugin(bool isSynth)
        : AudioPluginInstance(isSynth ? BusesProperties().withOutput("Out", juce::AudioChannelSet::stereo())
                                      : BusesProperties().withInput("In", juce::AudioChannelSet::mono())
                                                         .withOutput("Out", juce::AudioChannelSet::mono())),
          synth(isSynth)
    {
    }
    void fillInPluginDescription(juce::PluginDescription& d) const override { d.name = getName(); d.isInstrument = synth; }
    const juce::String getName() const override { return synth ? "FakeSynth" : "FakeTracker"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override
    {
        if (! synth)
        {
            const auto loud = audio.getMagnitude(0, 0, audio.getNumSamples()) > 0.1f;
            midi.clear();
            if (loud && ! sounding)
                midi.addEvent(juce::MidiMessage::noteOn(1, 40, static_cast<juce::uint8>(100)), 0);
            if (! loud && sounding)
                midi.addEvent(juce::MidiMessage::noteOff(1, 40), 0);
            sounding = loud;
            return;
        }
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            if (message.isNoteOn())
            {
                held.insert(message.getNoteNumber());
                lastNote = message.getNoteNumber();
            }
            else if (message.isNoteOff())
                held.erase(message.getNoteNumber());
        }
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            juce::FloatVectorOperations::fill(audio.getWritePointer(channel), held.empty() ? 0.0f : 0.5f, audio.getNumSamples());
    }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return synth; }
    bool producesMidi() const override { return ! synth; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    const bool synth;
    bool sounding = false;
    std::set<int> held;
    int lastNote = -1;
};

class GuitarSynthTests final : public juce::UnitTest
{
public:
    GuitarSynthTests() : UnitTest("Guitar to MIDI synth", "Plugins") {}

    void runTest() override
    {
        constexpr int block = 256;
        GuitarSynth host;
        host.prepare(48000.0, block);
        auto trackerPlugin = std::make_unique<FakePlugin>(false);
        auto synthPlugin = std::make_unique<FakePlugin>(true);
        auto* synthView = synthPlugin.get();
        juce::PluginDescription trackerDescription, synthDescription;
        trackerPlugin->fillInPluginDescription(trackerDescription);
        synthPlugin->fillInPluginDescription(synthDescription);
        expect(host.adopt(GuitarSynth::Slot::tracker, std::move(trackerPlugin), trackerDescription).wasOk());
        expect(host.adopt(GuitarSynth::Slot::synth, std::move(synthPlugin), synthDescription).wasOk());

        std::vector<float> dry(block), left(block), right(block);
        const auto run = [&](float guitar, float processed)
        {
            std::fill(dry.begin(), dry.end(), guitar);
            std::fill(left.begin(), left.end(), processed);
            std::fill(right.begin(), right.end(), processed);
            host.process(dry.data(), left.data(), right.data(), block);
        };

        beginTest("Disabled: the guitar passes untouched");
        run(0.5f, 0.3f);
        expectEquals(left[100], 0.3f);
        expect(synthView->held.empty());

        beginTest("Detected notes play the synth, mixed with the guitar");
        host.setEnabled(true);
        for (int index = 0; index < 20; ++index) // let the level smoothing settle
            run(0.5f, 0.3f);
        expectEquals(synthView->lastNote, 40);
        expectEquals(host.lastNote(), 40);
        expectWithinAbsoluteError(left[block - 1], 0.3f + 0.5f, 0.001f);
        expectWithinAbsoluteError(right[block - 1], 0.3f + 0.5f, 0.001f);

        beginTest("Synth only, quieter synth");
        host.setGuitarLevelDb(-60.0f);
        host.setSynthLevelDb(-6.0f);
        for (int index = 0; index < 20; ++index)
            run(0.5f, 0.3f);
        expectWithinAbsoluteError(left[block - 1], 0.5f * juce::Decibels::decibelsToGain(-6.0f), 0.001f);

        beginTest("Silence releases the note");
        run(0.0f, 0.0f);
        expect(synthView->held.empty());
        expectEquals(host.heldNotes(), 0);

        beginTest("Transpose by an octave");
        host.setTranspose(12);
        run(0.5f, 0.0f);
        expectEquals(synthView->lastNote, 52);
        run(0.0f, 0.0f);
        expect(synthView->held.empty(), "note off is transposed too");

        beginTest("MIDI keyboard notes reach the synth");
        host.setTranspose(0);
        juce::MidiInputCallback& input = host;
        input.handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(90)).withTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001));
        for (int index = 0; index < 4; ++index)
            run(0.0f, 0.0f);
        expectEquals(synthView->lastNote, 64);

        beginTest("State keeps the settings");
        const auto state = host.saveState();
        expectEquals(static_cast<int>(state["transpose"]), 0);
        expect(static_cast<bool>(state["enabled"]));
        expectWithinAbsoluteError(static_cast<float>(state["synthDb"]), -6.0f, 0.001f);
        host.clear(GuitarSynth::Slot::synth);
        expect(! host.hasPlugin(GuitarSynth::Slot::synth));
        run(0.5f, 0.3f); // no synth: only the guitar level applies, nothing crashes
    }
};

GuitarSynthTests guitarSynthTests;
} // namespace
