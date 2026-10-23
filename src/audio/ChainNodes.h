#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>

#include "plugins/Vst3Slot.h"

/** Processors for the nodes of the signal graph (see RigChain). Every node
    is stereo in and out, so any output can feed any input. */
namespace chainnodes
{
class Base : public juce::AudioProcessor
{
public:
    Base() : AudioProcessor(BusesProperties().withInput("In", juce::AudioChannelSet::stereo(), true)
                                             .withOutput("Out", juce::AudioChannelSet::stereo(), true)) {}
    const juce::String getName() const override { return "Node"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

/** A built-in effect. The effects are mono: a stereo input is summed, the
    result goes to both outputs. */
class Builtin final : public Base
{
public:
    using Process = std::function<void(float* mono, int numSamples)>;
    explicit Builtin(Process p) : process(std::move(p)) {}
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) override
    {
        const auto count = audio.getNumSamples();
        auto* left = audio.getWritePointer(0);
        if (audio.getNumChannels() > 1)
        {
            auto* right = audio.getWritePointer(1);
            for (int i = 0; i < count; ++i)
                left[i] = 0.5f * (left[i] + right[i]);
            process(left, count);
            juce::FloatVectorOperations::copy(right, left, count);
        }
        else
            process(left, count);
    }

private:
    Process process;
};

/** A VST3 plugin, hosted by Vst3Slot (crash-safe, latency-compensated bypass). */
class Plugin final : public Base
{
public:
    Vst3Slot slot;
    const juce::String getName() const override { return slot.name(); }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    void prepareToPlay(double rate, int block) override { slot.prepare(rate, block); }
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override { slot.process(audio, &midi); }
};

/** Shifts MIDI notes; audio passes unchanged. */
class Transpose final : public Base
{
public:
    std::atomic<int> semitones { 0 };
    std::atomic<bool> bypassed { false };
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    void prepareToPlay(double, int) override { shifted.ensureSize(4096); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer& midi) override
    {
        const auto shift = bypassed.load(std::memory_order_relaxed) ? 0 : semitones.load(std::memory_order_relaxed);
        if (shift == 0)
            return;
        shifted.clear();
        for (const auto metadata : midi)
        {
            auto message = metadata.getMessage();
            if (message.isNoteOnOrOff() || message.isAftertouch())
            {
                const auto note = message.getNoteNumber() + shift;
                if (note < 0 || note > 127)
                    continue;
                message.setNoteNumber(note);
            }
            shifted.addEvent(message, metadata.samplePosition);
        }
        midi.swapWith(shifted);
    }

private:
    juce::MidiBuffer shifted;
};

/** The level of an extra audio connection. */
class Gain final : public Base
{
public:
    explicit Gain(float db) { setDb(db); }
    void setDb(float db) noexcept { target.store(db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain(db), std::memory_order_relaxed); }
    void prepareToPlay(double rate, int) override { gain.reset(rate, 0.02); gain.setCurrentAndTargetValue(target.load()); }
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override
    {
        midi.clear();
        gain.setTargetValue(target.load(std::memory_order_relaxed));
        if (! gain.isSmoothing())
        {
            audio.applyGain(gain.getTargetValue());
            return;
        }
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const auto g = gain.getNextValue();
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                audio.getWritePointer(channel)[i] *= g;
        }
    }

private:
    std::atomic<float> target { 1.0f };
    juce::SmoothedValue<float> gain { 1.0f };
};
} // namespace chainnodes
