#pragma once

#include <atomic>

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
/**
 * Visualises a peak value supplied by an audio-thread-owned atomic. The GUI polls
 * the atomic at a display rate; no GUI calls are made from the audio callback.
 */
class AudioMeter final : public juce::Component, private juce::Timer
{
public:
    explicit AudioMeter(const std::atomic<float>* peakSource = nullptr);
    ~AudioMeter() override;

    void setPeakSource(const std::atomic<float>* peakSource) noexcept;
    void setLabel(juce::String label);
    void setDecayRate(float decibelsPerSecond) noexcept;
    /** Clears the latched overload indicator on the message thread. */
    void clearClipIndicator() noexcept;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    std::atomic<const std::atomic<float>*> source { nullptr };
    juce::String label;
    float displayedPeak = 0.0f;
    float heldPeak = 0.0f;
    float decayPerTick = 0.06f;
    bool clipping = false;
    int clipHoldTicks = 0;
    juce::Rectangle<int> meterBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioMeter)
};
} // namespace nam::ui
