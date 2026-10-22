#include "PitchShiftComponent.h"

#include "audio/effects/LivePitchShifter.h"
#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using Theme = AmberTheme;
using daw::text;

PitchShiftComponent::PitchShiftComponent()
{
    power.setButtonText("PITCH (Rubber Band)");
    power.setTooltip(text("Transponiert das Live-Signal vor Drive und NAM-Amp, z. B. eine Oktave nach unten"));
    power.onClick = [this] { current.bypassed = ! power.getToggleState(); publish(); };
    addAndMakeVisible(power);

    semitones.setRange(-24.0, 24.0, 1.0);
    semitones.setTextValueSuffix(" st");
    semitones.setDoubleClickReturnValue(true, -12.0);
    semitones.onValueChange = [this]
    {
        current.semitones = static_cast<float>(semitones.getValue());
        current.bypassed = false;
        publish();
    };
    mix.setRange(0.0, 100.0, 1.0);
    mix.setTextValueSuffix(" %");
    mix.setDoubleClickReturnValue(true, 100.0);
    mix.onValueChange = [this] { current.mix = static_cast<float>(mix.getValue() / 100.0); publish(); };
    for (auto* slider : { &semitones, &mix })
    {
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 22);
        addAndMakeVisible(*slider);
    }
    semitoneLabel.setText(text("Halbtöne"), juce::dontSendNotification);
    mixLabel.setText(text("Mix (Original ↔ transponiert)"), juce::dontSendNotification);
    for (auto* label : { &semitoneLabel, &mixLabel, &info })
    {
        label->setColour(juce::Label::textColourId, Theme::subduedText());
        label->setFont(juce::FontOptions(12.0f));
        addAndMakeVisible(*label);
    }

    for (const auto value : { -24, -12, -7, -5, 5, 7, 12 })
    {
        auto* button = quick.add(new juce::TextButton((value > 0 ? "+" : "") + juce::String(value)));
        button->setTooltip(value == -12 ? text("Eine Oktave tiefer – latenzfrei wie ein analoges Oktavpedal (Flip-Flop), einstimmig") : value == 12 ? text("Eine Oktave höher") : juce::String());
        button->onClick = [this, value] { semitones.setValue(value, juce::sendNotificationSync); };
        addAndMakeVisible(*button);
    }

    formant.setButtonText("Formanten erhalten");
    formant.setTooltip(text("Hält den Klangcharakter beim Transponieren natürlicher (vor allem bei Stimme)"));
    formant.onClick = [this] { current.formantPreserved = formant.getToggleState(); publish(); };
    addAndMakeVisible(formant);

    mode.addItem(text("Niedrige Latenz (R2, ca. 30–45 ms)"), 1);
    mode.addItem(text("Hohe Qualität (R3 Live, ca. 90 ms)"), 2);
    mode.onChange = [this] { current.highQuality = mode.getSelectedId() == 2; publish(); };
    addAndMakeVisible(mode);

    refresh();
    setSize(520, 210);
    startTimerHz(4);
}

void PitchShiftComponent::setSettings(const Settings& settings)
{
    current = settings;
    refresh();
}

void PitchShiftComponent::refresh()
{
    power.setToggleState(! current.bypassed, juce::dontSendNotification);
    semitones.setValue(current.semitones, juce::dontSendNotification);
    mix.setValue(current.mix * 100.0, juce::dontSendNotification);
    formant.setToggleState(current.formantPreserved, juce::dontSendNotification);
    mode.setSelectedId(current.highQuality ? 2 : 1, juce::dontSendNotification);
    timerCallback();
}

void PitchShiftComponent::publish()
{
    power.setToggleState(! current.bypassed, juce::dontSendNotification);
    if (onChange)
        onChange(current);
}

void PitchShiftComponent::timerCallback()
{
    const auto analog = nam::effects::LivePitchShifter::usesAnalogOctave(current.semitones);
    mode.setEnabled(! analog);
    juce::String line = text("Dry und transponiertes Signal laufen zeitgleich; Aufnahmen werden ausgeglichen.");
    if (analog)
        line = text("−12: analoger Oktaver (Flip-Flop-Teiler) – keine Verzögerung. Einstimmig spielen, Akkorde verzerren.");
    else if (latencyMs && ! current.bypassed)
        if (const auto ms = latencyMs(); ms > 0.0)
            line = text("Aktuelle Verzögerung: ") + juce::String(ms, 1) + " ms. " + line;
    info.setText(line, juce::dontSendNotification);
}

void PitchShiftComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff1d1814));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
}

void PitchShiftComponent::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto top = area.removeFromTop(24);
    mode.setBounds(top.removeFromRight(260));
    power.setBounds(top);
    area.removeFromTop(8);
    auto row = area.removeFromTop(24);
    semitoneLabel.setBounds(row.removeFromLeft(140));
    semitones.setBounds(row);
    area.removeFromTop(4);
    auto buttons = area.removeFromTop(26);
    const auto width = buttons.getWidth() / quick.size();
    for (auto* button : quick)
        button->setBounds(buttons.removeFromLeft(width).reduced(2, 0));
    area.removeFromTop(6);
    row = area.removeFromTop(24);
    mixLabel.setBounds(row.removeFromLeft(140));
    mix.setBounds(row);
    area.removeFromTop(4);
    formant.setBounds(area.removeFromTop(24).removeFromLeft(220));
    info.setBounds(area);
}
} // namespace nam::ui
