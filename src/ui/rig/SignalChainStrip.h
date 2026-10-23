#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace nam::ui
{
/** The live rig's signal chain as a row of blocks in processing order
    (pitch, wah, drive, VST3, NAM pedal, amp, cabinet, EQ, effects ...).

    Each block shows whether it is on (LED) and what it is set to. Clicking
    the LED switches it on or off, clicking the block opens its editor. It is
    the rig's central control surface; the owner supplies the blocks and
    reacts to the two callbacks. */
class SignalChainStrip final : public juce::Component, public juce::TooltipClient
{
public:
    struct Block
    {
        juce::String id;          // stable key for the callbacks, e.g. "drive"
        juce::String label;       // short name, e.g. "DRIVE"
        juce::String detail;      // current setting, e.g. "Crunch" or the model name
        bool active = false;      // processing (LED lit)
        bool toggleable = true;   // has an on/off switch
        bool empty = false;       // nothing loaded yet (VST3 slot, pedal): drawn hollow
        juce::String tooltip;
    };

    void setBlocks(std::vector<Block>);
    [[nodiscard]] const std::vector<Block>& blocks() const noexcept { return items; }
    /** Screen area of a block, e.g. to anchor a call-out to it. */
    [[nodiscard]] juce::Rectangle<int> screenBoundsOf(const juce::String& id) const;
    void setSelected(const juce::String& id);

    std::function<void(const juce::String& id)> onToggle;
    std::function<void(const juce::String& id)> onOpen;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    juce::String getTooltip() override;

private:
    [[nodiscard]] juce::Rectangle<float> blockBounds(size_t index) const;
    [[nodiscard]] juce::Rectangle<float> ledBounds(size_t index) const;
    [[nodiscard]] int blockAt(juce::Point<float>) const;

    std::vector<Block> items;
    juce::String selected;
    int hovered = -1;
    bool hoveredLed = false;
};
} // namespace nam::ui
