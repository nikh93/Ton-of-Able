// "Ton Tuner": the tuner of Ton of Able as a VST3 plugin, so it sits in any
// signal chain (and in other hosts). The audio passes through, or is muted
// while tuning; the pitch is detected on the message thread from a copy of
// the most recent input.

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PitchDetector.h"

namespace
{
const juce::Colour background { 0xff17130f }, panel { 0xff221c17 }, amber { 0xffe3a447 }, textColour { 0xfff1dfc4 },
    subdued { 0xffbcaa91 }, inTune { 0xff6fbf73 }, offColour { 0xffdd735c };

class TunerProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int ringSize = 8192;

    TunerProcessor()
        : AudioProcessor(BusesProperties().withInput("In", juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Out", juce::AudioChannelSet::stereo(), true))
    {
        addParameter(reference = new juce::AudioParameterFloat(juce::ParameterID { "reference", 1 }, "Kammerton A",
                                                               juce::NormalisableRange<float>(430.0f, 450.0f, 0.5f), 440.0f));
        addParameter(mute = new juce::AudioParameterBool(juce::ParameterID { "mute", 1 }, "Stumm", false));
    }

    const juce::String getName() const override { return "Ton Tuner"; }
    void prepareToPlay(double sampleRate, int) override { rate.store(sampleRate); }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
        return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
    }

    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) override
    {
        const auto channels = audio.getNumChannels();
        const auto count = audio.getNumSamples();
        auto write = writeIndex.load(std::memory_order_relaxed);
        for (int sample = 0; sample < count; ++sample)
        {
            float sum = 0.0f;
            for (int channel = 0; channel < channels; ++channel)
                sum += audio.getSample(channel, sample);
            ring[static_cast<size_t>(write)] = channels > 0 ? sum / static_cast<float>(channels) : 0.0f;
            write = (write + 1) % ringSize;
        }
        writeIndex.store(write, std::memory_order_release);
        if (mute->get())
            audio.clear();
    }

    /** Message thread: the latest count samples, oldest first. A torn read only costs one reading. */
    void copyLatest(float* out, int count) const
    {
        const auto end = writeIndex.load(std::memory_order_acquire);
        for (int index = 0; index < count; ++index)
            out[index] = ring[static_cast<size_t>((end - count + index + ringSize) % ringSize)];
    }
    [[nodiscard]] double sampleRate() const { return rate.load(); }
    [[nodiscard]] float referencePitch() const { return reference->get(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& data) override
    {
        juce::ValueTree state("TonTuner");
        state.setProperty("reference", reference->get(), nullptr);
        state.setProperty("mute", mute->get(), nullptr);
        juce::MemoryOutputStream stream(data, false);
        state.writeToStream(stream);
    }
    void setStateInformation(const void* data, int size) override
    {
        const auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(size));
        if (! state.hasType("TonTuner"))
            return;
        *reference = static_cast<float>(state.getProperty("reference", 440.0f));
        *mute = static_cast<bool>(state.getProperty("mute", false));
    }

    juce::AudioParameterFloat* reference = nullptr;
    juce::AudioParameterBool* mute = nullptr;

private:
    std::array<float, ringSize> ring {};
    std::atomic<int> writeIndex { 0 };
    std::atomic<double> rate { 48000.0 };
};

class TunerEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TunerEditor(TunerProcessor& p) : AudioProcessorEditor(p), processor(p)
    {
        referenceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        referenceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        referenceSlider.setRange(430.0, 450.0, 0.5);
        referenceSlider.setTextValueSuffix(" Hz");
        referenceSlider.setValue(processor.referencePitch(), juce::dontSendNotification);
        referenceSlider.onValueChange = [this] { *processor.reference = static_cast<float>(referenceSlider.getValue()); };
        referenceSlider.setColour(juce::Slider::thumbColourId, amber);
        referenceSlider.setColour(juce::Slider::trackColourId, amber.withAlpha(0.5f));
        referenceSlider.setColour(juce::Slider::textBoxTextColourId, textColour);
        referenceSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        referenceLabel.setText(juce::String(juce::CharPointer_UTF8("Kammerton A")), juce::dontSendNotification);
        referenceLabel.setColour(juce::Label::textColourId, subdued);
        muteButton.setButtonText(juce::String(juce::CharPointer_UTF8("Stumm beim Stimmen")));
        muteButton.setColour(juce::ToggleButton::textColourId, textColour);
        muteButton.setColour(juce::ToggleButton::tickColourId, amber);
        muteButton.setToggleState(processor.mute->get(), juce::dontSendNotification);
        muteButton.onClick = [this] { *processor.mute = muteButton.getToggleState(); };
        for (auto* child : std::initializer_list<juce::Component*> { &referenceSlider, &referenceLabel, &muteButton })
            addAndMakeVisible(*child);
        samples.resize(4096);
        setSize(420, 260);
        startTimerHz(25);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(background);
        auto area = getLocalBounds().reduced(14);
        auto display = area.removeFromTop(170).toFloat();
        g.setColour(panel);
        g.fillRoundedRectangle(display, 8.0f);

        const auto active = held > 0;
        const auto good = active && std::abs(cents) < 3.0;
        const auto colour = ! active ? subdued.withAlpha(0.5f) : good ? inTune : std::abs(cents) < 15.0 ? amber : offColour;

        // Note name and octave.
        g.setColour(colour);
        g.setFont(juce::FontOptions(64.0f, juce::Font::bold));
        g.drawText(active ? note.name : juce::String("-"), display.removeFromTop(78.0f).toNearestInt(), juce::Justification::centred);
        g.setFont(juce::FontOptions(14.0f));
        g.setColour(subdued);
        const auto detail = active ? note.name + juce::String(note.octave) + "   " + juce::String(frequency, 1) + " Hz   "
                                         + (cents >= 0.0 ? "+" : "") + juce::String(cents, 1) + " ct"
                                   : juce::String(juce::CharPointer_UTF8("Saite anschlagen …"));
        g.drawText(detail, display.removeFromTop(22.0f).toNearestInt(), juce::Justification::centred);

        // Scale from -50 to +50 cents with the needle.
        auto scale = display.reduced(24.0f, 18.0f);
        const auto centre = scale.getCentreX();
        g.setColour(subdued.withAlpha(0.35f));
        for (int mark = -50; mark <= 50; mark += 10)
        {
            const auto x = centre + static_cast<float>(mark) / 50.0f * scale.getWidth() * 0.5f;
            g.drawLine(x, scale.getY() + (mark == 0 ? 0.0f : 8.0f), x, scale.getBottom(), mark == 0 ? 2.0f : 1.0f);
        }
        g.setColour(inTune.withAlpha(0.25f));
        g.fillRect(juce::Rectangle<float>(centre - scale.getWidth() * 0.03f, scale.getY(), scale.getWidth() * 0.06f, scale.getHeight()));
        if (active)
        {
            const auto x = centre + static_cast<float>(juce::jlimit(-50.0, 50.0, cents)) / 50.0f * scale.getWidth() * 0.5f;
            g.setColour(colour);
            g.fillRoundedRectangle(juce::Rectangle<float>(x - 3.0f, scale.getY() - 4.0f, 6.0f, scale.getHeight() + 8.0f), 3.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14);
        area.removeFromTop(180);
        auto row = area.removeFromTop(26);
        referenceLabel.setBounds(row.removeFromLeft(100));
        referenceSlider.setBounds(row);
        area.removeFromTop(6);
        muteButton.setBounds(area.removeFromTop(24));
    }

private:
    void timerCallback() override
    {
        processor.copyLatest(samples.data(), static_cast<int>(samples.size()));
        const auto result = detector.detect(samples.data(), static_cast<int>(samples.size()), processor.sampleRate());
        if (result.frequency > 0.0 && result.clarity > 0.8)
        {
            const auto next = tuner::noteOf(result.frequency, processor.referencePitch());
            // Smooth the reading while it stays on one note; jump on a new one.
            if (held > 0 && next.midi == note.midi)
            {
                cents += 0.35 * (next.cents - cents);
                frequency += 0.35 * (result.frequency - frequency);
            }
            else
            {
                cents = next.cents;
                frequency = result.frequency;
            }
            note = next;
            held = 12; // keep showing the last note for half a second
        }
        else if (held > 0)
            --held;
        repaint();
    }

    TunerProcessor& processor;
    tuner::PitchDetector detector;
    std::vector<float> samples;
    tuner::Note note;
    double cents = 0.0, frequency = 0.0;
    int held = 0;
    juce::Slider referenceSlider;
    juce::Label referenceLabel;
    juce::ToggleButton muteButton;
};

juce::AudioProcessorEditor* TunerProcessor::createEditor() { return new TunerEditor(*this); }
} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TunerProcessor(); }
