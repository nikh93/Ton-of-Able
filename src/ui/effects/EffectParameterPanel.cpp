#include "EffectParameterPanel.h"

#include "../components/AmberTheme.h"

#include <array>
#include <cmath>

namespace nam::ui
{
namespace
{
juce::String unit(double value, const juce::String& suffix)
{
    return juce::String(value, value >= 100.0 ? 0 : 1) + suffix;
}

void configure(RotaryParameterControl& control, double minimum, double maximum,
               std::function<juce::String(double)> formatter)
{
    control.setRange(minimum, maximum, 0.01 * (maximum - minimum));
    control.setValueFormatter(std::move(formatter));
}
} // namespace

EffectParameterPanel::EffectParameterPanel()
{
    configureControls();
    for (int i=0; i<numCards; ++i)
    {
        cardContent.addAndMakeVisible(surfaces[i]);
        surfaces[i].addAndMakeVisible(reveals[i]);
        reveals[i].addAndMakeVisible(parameters[i]);
    }
    const juce::StringArray presetNames[] = {
        { "Edge of Breakup", "Warm Blues", "Singing Lead", "Vintage Fuzz" },
        { "Funky", "Vocal", "Slow Sweep", "Down Sweep" },
        { "Subtle", "Classic", "Wide", "Vibrato" },
        { "Slapback", "Analog", "Lead Echo", "Ambient" },
        { "Room", "Hall", "Cave", "Bright Plate" },
        { "Sanft", "Clean Sustain", "Funk Squash", "Parallel" },
        { "Cry Baby", "Vox", "Bass-Wah", "Fester Wah" }
    };
    for (int i = 0; i < numCards; ++i)
    {
        effectPresets[i].addItemList(presetNames[i], 1);
        effectPresets[i].setTextWhenNothingSelected("Custom");
        effectPresets[i].setTooltip("Effekt-Preset auswaehlen und aktivieren");
        surfaces[i].addAndMakeVisible(effectPresets[i]);
        effectPresets[i].onChange = [this, i] { if (effectPresets[i].getSelectedId() > 0) applyEffectPreset(i, effectPresets[i].getSelectedId()); };
        power[i].setButtonText("ON");
        surfaces[i].addAndMakeVisible(power[i]);
        power[i].onClick = [this, i]
        {
            bool* bypass[] = { &currentState.driveBypassed, &currentState.autoWahBypassed, &currentState.chorusBypassed, &currentState.delayBypassed, &currentState.reverbBypassed, &currentState.compressorBypassed, &currentState.pedalWahBypassed };
            *bypass[i] = !power[i].getToggleState();
            publish();
        };
    }
    cardViewport.setViewedComponent(&cardContent, false);
    cardViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(cardViewport);
    for (auto* control : { &driveAmount, &driveTone, &driveCharacter, &driveSustain, &driveLevel, &driveMix,
                           &wahSensitivity, &wahAttack, &wahRelease, &wahMin, &wahMax, &wahQ, &wahMix,
                           &chorusRate, &chorusDepth, &chorusDelay, &chorusFeedback, &chorusMix,
                           &delayTime, &delayFeedback, &delayMix, &delayLowCut, &delayHighCut,
                           &reverbSize, &reverbDecay, &reverbDamping, &reverbPreDelay, &reverbMix,
                           &reverbLowCut, &reverbHighCut, &compressorThreshold, &compressorRatio, &compressorAttack,
                           &compressorRelease, &compressorKnee, &compressorMakeup, &compressorMix,
                           &wahPosition, &wahHeel, &wahToe, &wahPedalQ, &wahPedalMix })
        cardContent.addAndMakeVisible(*control);
    wahMouse.setTooltip(juce::String(juce::CharPointer_UTF8("Die Maus ist das Pedal: oben = Toe (hell), unten = Heel (dunkel) – über den ganzen Bildschirm. Taste W schaltet um (frei belegbar).")));
    wahMouse.onClick = [this]
    {
        currentState.pedalWahMouse = wahMouse.getToggleState();
        if (currentState.pedalWahMouse)
            currentState.pedalWahBypassed = false;
        publish();
    };
    compressorPreAmp.setTooltip(juce::String(juce::CharPointer_UTF8("An: wie ein Kompressor-Pedal vor Drive und Amp (mehr Sustain). Aus: nach Amp und Box, wie im Studio.")));
    compressorPreAmp.onClick = [this]
    {
        currentState.compressorPreAmp = compressorPreAmp.getToggleState();
        publish();
    };
    cardContent.addAndMakeVisible(wahDirection);
    parameters[0].addAndMakeVisible(ampPush);
    ampPush.slider().setTooltip("Pegel vor NAM: 0 dB = unveraendert. Wirkt auch bei Drive OFF. OUTPUT regelt die Abhoerlautstaerke.");
    for (auto* card : { &driveCard, &wahCard, &chorusCard, &delayCard, &reverbCard, &compressorCard, &wahPedalCard })
    {
        card->setLookAndFeel(&headerLook);
        card->setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    }
    driveCard.onClick = [this] { animateTo(expandedCard == 0 ? -1 : 0); };
    wahCard.onClick = [this] { animateTo(expandedCard == 1 ? -1 : 1); };
    chorusCard.onClick = [this] { animateTo(expandedCard == 2 ? -1 : 2); };
    delayCard.onClick = [this] { animateTo(expandedCard == 3 ? -1 : 3); };
    reverbCard.onClick = [this] { animateTo(expandedCard == 4 ? -1 : 4); };
    compressorCard.onClick = [this] { animateTo(expandedCard == 5 ? -1 : 5); };
    wahPedalCard.onClick = [this] { animateTo(expandedCard == 6 ? -1 : 6); };
    wahDirection.onClick = [this]
    {
        currentState.autoWahUpward = wahDirection.getToggleState();
        publish();
    };
    driveType.addItem("Crunch", 1); driveType.addItem("Warm Overdrive", 2); driveType.addItem("Singing Lead", 3); driveType.addItem("Fuzz", 4);
    driveType.onChange = [this] { currentState.driveType = driveType.getSelectedId() - 1; driveCharacter.setValue(currentState.driveType == 1 ? currentState.driveWarmth : currentState.driveType == 2 ? currentState.drivePresence : currentState.driveBias, juce::dontSendNotification); publish(); resized(); };
    parameters[0].addAndMakeVisible(driveType);
    setState(currentState);
    updateCardLabels();
}

EffectParameterPanel::~EffectParameterPanel()
{
    stopTimer();
    for (auto* c : { &driveCard, &wahCard, &chorusCard, &delayCard, &reverbCard, &compressorCard, &wahPedalCard }) c->setLookAndFeel(nullptr);
}

void EffectParameterPanel::previewExpanded(int index)
{
    stopTimer();
    expandedCard = index;
    for (int i=0; i<numCards; ++i) openness[i] = i == index ? 1.f : 0.f;
    resized();
    cardViewport.setViewPosition(0, juce::jmax(0, index) * 92);
}

void EffectParameterPanel::animateTo(int index)
{
    expandedCard = index;
    for (int i=0; i<numCards; ++i) from[i] = openness[i];
    animationStart = juce::Time::getMillisecondCounterHiRes();
    updateCardLabels();
    startTimerHz(60);
}

void EffectParameterPanel::timerCallback()
{
    const float t = juce::jlimit(0.f, 1.f, float((juce::Time::getMillisecondCounterHiRes()-animationStart)/280.0));
    const float eased = t*t*(3.f-2.f*t);
    for (int i=0; i<numCards; ++i)
        openness[i] = from[i] + ((i == expandedCard ? 1.f : 0.f)-from[i])*eased;
    resized();
    if (t >= 1.f) stopTimer();
}

void GlassEffectSurface::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(1, 2);
    g.setColour(juce::Colours::black.withAlpha(.28f));
    g.fillRoundedRectangle(r.translated(0, 2), 12);
    juce::ColourGradient glass(juce::Colour(0xff788a87).withAlpha(.54f), r.getX(), r.getY(),
        juce::Colour(0xff172321).withAlpha(.85f), r.getRight(), r.getBottom(), false);
    glass.addColour(.35, juce::Colour(0xff3d514e).withAlpha(.65f));
    g.setGradientFill(glass); g.fillRoundedRectangle(r, 12);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(.22f), 0, r.getY(),
        juce::Colours::white.withAlpha(0.f), 0, r.getY()+48, false));
    g.fillRoundedRectangle(r.withHeight(juce::jmin(48.f,r.getHeight())), 12);
    g.setColour(juce::Colours::white.withAlpha(.22f)); g.drawRoundedRectangle(r.reduced(.5f), 12, 1);
    g.setColour(juce::Colours::white.withAlpha(.24f));
    g.drawHorizontalLine(juce::roundToInt(r.getY()+2), r.getX()+12, r.getRight()-12);
}

void GlassHeaderLook::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down)
{
    if (hover || down)
    {
        g.setColour(juce::Colours::white.withAlpha(down ? .14f : .07f));
        g.fillRoundedRectangle(b.getLocalBounds().toFloat().reduced(2), 10);
    }
}

void EffectParameterPanel::configureControls()
{
    configure(driveAmount, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(driveTone, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(driveCharacter, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(driveSustain, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(driveLevel, 0.0, 1.5, [](double v) { return v <= 0 ? juce::String("Mute") : juce::String(20.0 * std::log10(v), 1) + " dB"; });
    configure(ampPush, -18.0, 18.0, [](double v) { return unit(v, " dB"); });
    ampPush.setRange(-18.0, 18.0, .1);
    configure(driveMix, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(wahSensitivity, 0.0, 1.0, [](double v) { return juce::String(v, 2); });
    configure(wahAttack, 1.0, 100.0, [](double v) { return unit(v, " ms"); });
    configure(wahRelease, 20.0, 1000.0, [](double v) { return unit(v, " ms"); });
    configure(wahMin, 80.0, 1800.0, [](double v) { return unit(v, " Hz"); });
    configure(wahMax, 800.0, 8000.0, [](double v) { return unit(v, " Hz"); });
    configure(wahQ, 0.5, 12.0, [](double v) { return juce::String(v, 2); });
    configure(wahMix, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(chorusRate, 0.05, 8.0, [](double v) { return juce::String(v, 2) + " Hz"; });
    configure(chorusDepth, 0.0, 20.0, [](double v) { return unit(v, " ms"); });
    configure(chorusDelay, 1.0, 40.0, [](double v) { return unit(v, " ms"); });
    configure(chorusFeedback, -0.95, 0.95, [](double v) { return juce::String(v, 2); });
    configure(chorusMix, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(delayTime, 10.0, 2000.0, [](double v) { return unit(v, " ms"); });
    configure(delayFeedback, 0.0, 0.92, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(delayMix, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(delayLowCut, 20.0, 1000.0, [](double v) { return unit(v, " Hz"); });
    configure(delayHighCut, 1000.0, 20000.0, [](double v) { return unit(v, " Hz"); });
    configure(reverbSize, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(reverbDecay, 0.1, 12.0, [](double v) { return unit(v, " s"); });
    configure(reverbDamping, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(reverbPreDelay, 0.0, 250.0, [](double v) { return unit(v, " ms"); });
    configure(reverbMix, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(reverbLowCut, 20.0, 1000.0, [](double v) { return unit(v, " Hz"); });
    configure(reverbHighCut, 1000.0, 20000.0, [](double v) { return unit(v, " Hz"); });
    configure(compressorThreshold, -60.0, 0.0, [](double v) { return unit(v, " dB"); });
    compressorThreshold.setRange(-60.0, 0.0, 0.1);
    configure(compressorRatio, 1.0, 20.0, [](double v) { return juce::String(v, v < 10.0 ? 1 : 0) + ":1"; });
    compressorRatio.setRange(1.0, 20.0, 0.1);
    configure(compressorAttack, 0.1, 200.0, [](double v) { return juce::String(v, v < 10.0 ? 1 : 0) + " ms"; });
    configure(compressorRelease, 5.0, 2000.0, [](double v) { return unit(v, " ms"); });
    configure(compressorKnee, 0.0, 18.0, [](double v) { return unit(v, " dB"); });
    configure(compressorMakeup, 0.0, 24.0, [](double v) { return unit(v, " dB"); });
    configure(compressorMix, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(wahPosition, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    configure(wahHeel, 100.0, 1500.0, [](double v) { return unit(v, " Hz"); });
    configure(wahToe, 500.0, 6000.0, [](double v) { return unit(v, " Hz"); });
    configure(wahPedalQ, 0.7, 15.0, [](double v) { return juce::String(v, 1); });
    configure(wahPedalMix, 0.0, 1.0, [](double v) { return juce::String(v * 100.0, 0) + "%"; });
    compressorAttack.setRange(0.1, 200.0, 0.1);
    compressorRelease.setRange(5.0, 2000.0, 1.0);
    for (auto* control : { &compressorAttack, &compressorRelease })
        control->slider().setSkewFactorFromMidPoint(control == &compressorAttack ? 10.0 : 150.0);

    const auto bind = [this](RotaryParameterControl& control, float& value)
    {
        control.onValueChange = [this, &value](double next) { value = static_cast<float>(next); publish(); };
    };
    bind(driveAmount, currentState.driveAmount); bind(driveTone, currentState.driveTone); bind(driveLevel, currentState.driveLevel); bind(driveMix, currentState.driveMix);
    driveCharacter.onValueChange = [this](double value) { if (currentState.driveType == 1) currentState.driveWarmth = static_cast<float>(value); else if (currentState.driveType == 2) currentState.drivePresence = static_cast<float>(value); else currentState.driveBias = static_cast<float>(value); publish(); };
    bind(driveSustain, currentState.driveSustain);
    bind(ampPush, currentState.namInputTrimDb);
    bind(wahSensitivity, currentState.autoWahSensitivity); bind(wahAttack, currentState.autoWahAttackMs);
    bind(wahRelease, currentState.autoWahReleaseMs); bind(wahMin, currentState.autoWahMinimumHz);
    bind(wahMax, currentState.autoWahMaximumHz); bind(wahQ, currentState.autoWahQ); bind(wahMix, currentState.autoWahMix);
    bind(chorusRate, currentState.chorusRateHz); bind(chorusDepth, currentState.chorusDepthMs);
    bind(chorusDelay, currentState.chorusDelayMs); bind(chorusFeedback, currentState.chorusFeedback); bind(chorusMix, currentState.chorusMix);
    bind(delayTime, currentState.delayTimeMs); bind(delayFeedback, currentState.delayFeedback); bind(delayMix, currentState.delayMix);
    bind(delayLowCut, currentState.delayLowCutHz); bind(delayHighCut, currentState.delayHighCutHz);
    bind(reverbSize, currentState.reverbSize); bind(reverbDecay, currentState.reverbDecay); bind(reverbDamping, currentState.reverbDamping);
    bind(reverbPreDelay, currentState.reverbPreDelayMs); bind(reverbMix, currentState.reverbMix);
    bind(reverbLowCut, currentState.reverbLowCutHz); bind(reverbHighCut, currentState.reverbHighCutHz);
    bind(compressorThreshold, currentState.compressorThresholdDb); bind(compressorRatio, currentState.compressorRatio);
    bind(compressorAttack, currentState.compressorAttackMs); bind(compressorRelease, currentState.compressorReleaseMs);
    bind(compressorKnee, currentState.compressorKneeDb); bind(compressorMakeup, currentState.compressorMakeupDb);
    bind(compressorMix, currentState.compressorMix);
    bind(wahPosition, currentState.pedalWahPosition); bind(wahHeel, currentState.pedalWahHeelHz);
    bind(wahToe, currentState.pedalWahToeHz); bind(wahPedalQ, currentState.pedalWahQ); bind(wahPedalMix, currentState.pedalWahMix);
}

void EffectParameterPanel::showWahPosition(float position)
{
    currentState.pedalWahPosition = position;
    lastPublishedState.pedalWahPosition = position;
    wahPosition.setValue(position, juce::dontSendNotification);
}

void EffectParameterPanel::setState(const SoundState& state, const juce::NotificationType notification)
{
    currentState = state;
    ampPush.setValue(state.namInputTrimDb, notification);
    lastPublishedState = state;
    for (auto& preset : effectPresets) preset.setSelectedId(0, juce::dontSendNotification);
    driveType.setSelectedId(juce::jlimit(0, 3, state.driveType) + 1, juce::dontSendNotification);
    driveAmount.setValue(state.driveAmount, notification); driveTone.setValue(state.driveTone, notification); driveSustain.setValue(state.driveSustain, notification); driveLevel.setValue(state.driveLevel, notification); driveMix.setValue(state.driveMix, notification);
    driveCharacter.setValue(state.driveType == 1 ? state.driveWarmth : state.driveType == 2 ? state.drivePresence : state.driveBias, notification);
    wahSensitivity.setValue(state.autoWahSensitivity, notification); wahAttack.setValue(state.autoWahAttackMs, notification);
    wahRelease.setValue(state.autoWahReleaseMs, notification); wahMin.setValue(state.autoWahMinimumHz, notification);
    wahMax.setValue(state.autoWahMaximumHz, notification); wahQ.setValue(state.autoWahQ, notification); wahMix.setValue(state.autoWahMix, notification);
    chorusRate.setValue(state.chorusRateHz, notification); chorusDepth.setValue(state.chorusDepthMs, notification);
    chorusDelay.setValue(state.chorusDelayMs, notification); chorusFeedback.setValue(state.chorusFeedback, notification); chorusMix.setValue(state.chorusMix, notification);
    delayTime.setValue(state.delayTimeMs, notification); delayFeedback.setValue(state.delayFeedback, notification); delayMix.setValue(state.delayMix, notification);
    delayLowCut.setValue(state.delayLowCutHz, notification); delayHighCut.setValue(state.delayHighCutHz, notification);
    reverbSize.setValue(state.reverbSize, notification); reverbDecay.setValue(state.reverbDecay, notification); reverbDamping.setValue(state.reverbDamping, notification);
    reverbPreDelay.setValue(state.reverbPreDelayMs, notification); reverbMix.setValue(state.reverbMix, notification);
    reverbLowCut.setValue(state.reverbLowCutHz, notification); reverbHighCut.setValue(state.reverbHighCutHz, notification);
    compressorThreshold.setValue(state.compressorThresholdDb, notification); compressorRatio.setValue(state.compressorRatio, notification);
    compressorAttack.setValue(state.compressorAttackMs, notification); compressorRelease.setValue(state.compressorReleaseMs, notification);
    compressorKnee.setValue(state.compressorKneeDb, notification); compressorMakeup.setValue(state.compressorMakeupDb, notification);
    compressorMix.setValue(state.compressorMix, notification);
    compressorPreAmp.setToggleState(state.compressorPreAmp, juce::dontSendNotification);
    wahPosition.setValue(state.pedalWahPosition, notification); wahHeel.setValue(state.pedalWahHeelHz, notification);
    wahToe.setValue(state.pedalWahToeHz, notification); wahPedalQ.setValue(state.pedalWahQ, notification);
    wahPedalMix.setValue(state.pedalWahMix, notification);
    wahMouse.setToggleState(state.pedalWahMouse, juce::dontSendNotification);
    wahDirection.setToggleState(state.autoWahUpward, juce::dontSendNotification);
    updateCardLabels();
    resized();
}

void EffectParameterPanel::updateCardLabels()
{
    bool bypass[] = { currentState.driveBypassed, currentState.autoWahBypassed, currentState.chorusBypassed, currentState.delayBypassed, currentState.reverbBypassed, currentState.compressorBypassed, currentState.pedalWahBypassed };
    for (int i=0; i<numCards; ++i) power[i].setToggleState(!bypass[i], juce::dontSendNotification);
    const juce::String driveNames[] { "CRUNCH", "WARM OVERDRIVE", "SINGING LEAD", "FUZZ" };
    const auto driveName = driveNames[juce::jlimit(0, 3, currentState.driveType)];
    driveCard.setButtonText("DRIVE / " + driveName + "  " + (currentState.driveBypassed ? "OFF" : "ON") + "  " + (expandedCard == 0 ? "-" : "+"));
    wahCard.setButtonText("AUTO-WAH  " + juce::String(currentState.autoWahBypassed ? "OFF" : "ON") + "  " + (expandedCard == 1 ? "-" : "+"));
    chorusCard.setButtonText("CHORUS  " + juce::String(currentState.chorusBypassed ? "OFF" : "ON") + "  " + (expandedCard == 2 ? "-" : "+"));
    delayCard.setButtonText("DELAY  " + juce::String(currentState.delayBypassed ? "OFF" : "ON") + "  " + (expandedCard == 3 ? "-" : "+"));
    reverbCard.setButtonText("REVERB  " + juce::String(currentState.reverbBypassed ? "OFF" : "ON") + "  " + (expandedCard == 4 ? "-" : "+"));
    compressorCard.setButtonText("KOMPRESSOR " + juce::String(currentState.compressorPreAmp ? "(VOR AMP)" : "(NACH AMP)") + "  "
                                 + (currentState.compressorBypassed ? "OFF" : "ON") + "  " + (expandedCard == 5 ? "-" : "+"));
    wahPedalCard.setButtonText("WAH-PEDAL " + juce::String(currentState.pedalWahMouse ? "(MAUS)" : "") + "  "
                               + (currentState.pedalWahBypassed ? "OFF" : "ON") + "  " + (expandedCard == 6 ? "-" : "+"));
    driveAmount.setName(currentState.driveType == 2 ? "GAIN" : currentState.driveType == 3 ? "FUZZ" : "DRIVE");
    driveCharacter.setName(currentState.driveType == 1 ? "WARMTH" : currentState.driveType == 2 ? "PRESENCE" : "BIAS");
}

void EffectParameterPanel::applyEffectPreset(int effect, int preset)
{
    const int p = juce::jlimit(0, 3, preset - 1);
    std::array<int, numCards> selections;
    for (int i = 0; i < numCards; ++i) selections[i] = effectPresets[i].getSelectedId();
    auto state = currentState;
    switch (effect)
    {
        case 0:
            state.driveBypassed = false; state.driveType = p;
            state.driveAmount = std::array<float,4>{ .22f, .48f, .52f, .72f }[p];
            state.driveTone = p == 2 ? .38f : .45f; state.driveWarmth = .7f; state.drivePresence = .4f;
            state.driveSustain = .65f; state.driveBias = .4f; state.driveLevel = 1.f; state.driveMix = 1.f;
            state.namInputTrimDb = 0.f;
            break;
        case 1:
            state.autoWahBypassed = false;
            state.autoWahSensitivity = std::array<float,4>{ .65f, .55f, .7f, .65f }[p];
            state.autoWahAttackMs = std::array<float,4>{ 4.f, 12.f, 65.f, 8.f }[p];
            state.autoWahReleaseMs = std::array<float,4>{ 120.f, 250.f, 650.f, 220.f }[p];
            state.autoWahMinimumHz = 180.f; state.autoWahMaximumHz = p == 1 ? 2200.f : 3200.f;
            state.autoWahQ = p == 1 ? 5.f : 3.5f; state.autoWahMix = 1.f; state.autoWahUpward = p != 3;
            break;
        case 2:
            state.chorusBypassed = false;
            state.chorusRateHz = std::array<float,4>{ .45f, .8f, .35f, 4.f }[p];
            state.chorusDepthMs = std::array<float,4>{ 1.2f, 3.f, 6.f, 2.f }[p];
            state.chorusDelayMs = 12.f; state.chorusFeedback = p == 2 ? .15f : 0.f;
            state.chorusMix = std::array<float,4>{ .18f, .35f, .5f, 1.f }[p];
            break;
        case 3:
            state.delayBypassed = false;
            state.delayTimeMs = std::array<float,4>{ 95.f, 320.f, 430.f, 720.f }[p];
            state.delayFeedback = std::array<float,4>{ .12f, .38f, .32f, .62f }[p];
            state.delayMix = std::array<float,4>{ .18f, .27f, .24f, .38f }[p];
            state.delayLowCutHz = 120.f; state.delayHighCutHz = p == 1 ? 3200.f : 6500.f;
            break;
        case 4:
            state.reverbBypassed = false;
            state.reverbSize = std::array<float,4>{ .25f, .7f, 1.f, .55f }[p];
            state.reverbDecay = std::array<float,4>{ .7f, 2.8f, 7.f, 1.8f }[p];
            state.reverbDamping = std::array<float,4>{ .55f, .4f, .6f, .2f }[p];
            state.reverbPreDelayMs = std::array<float,4>{ 5.f, 25.f, 65.f, 12.f }[p];
            state.reverbMix = std::array<float,4>{ .18f, .26f, .4f, .22f }[p];
            state.reverbLowCutHz = 160.f; state.reverbHighCutHz = p == 3 ? 12000.f : 7500.f;
            break;
        case 5:
            // Gentle levelling, a pedal-style sustainer, a hard funk squash and parallel compression.
            state.compressorBypassed = false;
            state.compressorPreAmp = p == 1 || p == 2;
            state.compressorThresholdDb = std::array<float,4>{ -16.f, -30.f, -36.f, -32.f }[p];
            state.compressorRatio = std::array<float,4>{ 2.5f, 6.f, 12.f, 8.f }[p];
            state.compressorAttackMs = std::array<float,4>{ 15.f, 8.f, 1.f, 3.f }[p];
            state.compressorReleaseMs = std::array<float,4>{ 180.f, 300.f, 90.f, 120.f }[p];
            state.compressorKneeDb = std::array<float,4>{ 8.f, 6.f, 2.f, 4.f }[p];
            state.compressorMakeupDb = std::array<float,4>{ 3.f, 10.f, 14.f, 12.f }[p];
            state.compressorMix = p == 3 ? .45f : 1.f;
            break;
        case 6:
            // Dunlop Cry Baby, Vox V847, a deeper bass wah and a fixed "cocked" wah without mouse.
            state.pedalWahBypassed = false;
            state.pedalWahHeelHz = std::array<float,4>{ 350.f, 450.f, 180.f, 350.f }[p];
            state.pedalWahToeHz = std::array<float,4>{ 2200.f, 1800.f, 1100.f, 2200.f }[p];
            state.pedalWahQ = std::array<float,4>{ 6.f, 4.5f, 5.f, 7.f }[p];
            state.pedalWahMix = 1.f;
            if (p == 3) { state.pedalWahMouse = false; state.pedalWahPosition = .62f; }
            break;
        default: return;
    }
    setState(state);
    publish();
    for (int i = 0; i < numCards; ++i) effectPresets[i].setSelectedId(selections[i], juce::dontSendNotification);
    effectPresets[effect].setSelectedId(preset, juce::dontSendNotification);
}

void EffectParameterPanel::publish()
{
    const auto before = lastPublishedState.toValueTree();
    const auto after = currentState.toValueTree();
    const juce::String prefixes[] { "drive", "autoWah", "chorus", "delay", "reverb", "compressor", "pedalWah" };
    for (int property = 0; property < after.getNumProperties(); ++property)
    {
        const auto key = after.getPropertyName(property);
        if (key.toString().endsWith("Bypassed") || before[key] == after[key]) continue;
        if (key.toString() == "namInputTrimDb") effectPresets[0].setSelectedId(0, juce::dontSendNotification);
        for (int effect = 0; effect < numCards; ++effect)
            if (key.toString().startsWith(prefixes[effect])) effectPresets[effect].setSelectedId(0, juce::dontSendNotification);
    }
    lastPublishedState = currentState;
    updateCardLabels();
    if (onStateChanged)
        onStateChanged(currentState);
}

void EffectParameterPanel::layoutEffect(juce::Rectangle<int> area,
                                        std::initializer_list<RotaryParameterControl*> controls)
{
    area.removeFromTop(20);
    constexpr int columns = 3;
    const auto rows = static_cast<int>((controls.size() + columns - 1) / columns);
    const auto cellWidth = area.getWidth() / columns;
    const auto cellHeight = rows > 0 ? area.getHeight() / rows : area.getHeight();
    int index = 0;
    for (auto* control : controls)
    {
        const auto row = index / columns;
        const auto column = index % columns;
        control->setBounds(area.getX() + column * cellWidth, area.getY() + row * cellHeight,
                           cellWidth, cellHeight);
        ++index;
    }
}

void EffectParameterPanel::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff41483f), 0, 0,
        juce::Colour(0xff202620), float(getWidth()), float(getHeight()), false));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    g.setColour(AmberTheme::amber().withAlpha(0.35f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
    g.setColour(AmberTheme::amber());
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("OPTIONALE EFFEKTE", 18, 13, 210, 18, juce::Justification::left);
}

void EffectParameterPanel::resized()
{
    updateCardLabels();
    cardViewport.setBounds(getLocalBounds().reduced(14).withTrimmedTop(30));
    float totalOpen = 0.0f;
    for (const auto open : openness)
        totalOpen += open;
    cardContent.setSize(juce::jmax(240, cardViewport.getWidth()-16), numCards*92 + juce::roundToInt(350*totalOpen));
    auto area = cardContent.getLocalBounds();
    juce::TextButton* cards[numCards] = { &driveCard, &wahCard, &chorusCard, &delayCard, &reverbCard, &compressorCard, &wahPedalCard };
    std::initializer_list<RotaryParameterControl*> groups[numCards - 1] = {
        { &wahSensitivity, &wahAttack, &wahRelease, &wahMin, &wahMax, &wahQ, &wahMix },
        { &chorusRate, &chorusDepth, &chorusDelay, &chorusFeedback, &chorusMix },
        { &delayTime, &delayFeedback, &delayMix, &delayLowCut, &delayHighCut },
        { &reverbSize, &reverbDecay, &reverbDamping, &reverbPreDelay, &reverbMix, &reverbLowCut, &reverbHighCut },
        { &compressorThreshold, &compressorRatio, &compressorAttack, &compressorRelease, &compressorKnee, &compressorMakeup, &compressorMix },
        { &wahPosition, &wahHeel, &wahToe, &wahPedalQ, &wahPedalMix }
    };
    for (auto* control : { &driveAmount, &driveTone, &driveCharacter, &driveSustain, &driveLevel, &driveMix,
                           &wahSensitivity, &wahAttack, &wahRelease, &wahMin, &wahMax, &wahQ, &wahMix,
                           &chorusRate, &chorusDepth, &chorusDelay, &chorusFeedback, &chorusMix,
                           &delayTime, &delayFeedback, &delayMix, &delayLowCut, &delayHighCut,
                           &reverbSize, &reverbDecay, &reverbDamping, &reverbPreDelay, &reverbMix, &reverbLowCut, &reverbHighCut,
                           &compressorThreshold, &compressorRatio, &compressorAttack, &compressorRelease, &compressorKnee, &compressorMakeup, &compressorMix,
                           &wahPosition, &wahHeel, &wahToe, &wahPedalQ, &wahPedalMix })
        control->setVisible(false);
    driveType.setVisible(false); wahDirection.setVisible(false); compressorPreAmp.setVisible(false); wahMouse.setVisible(false);
    parameters[6].addAndMakeVisible(wahMouse);
    parameters[1].addAndMakeVisible(wahDirection);
    parameters[5].addAndMakeVisible(compressorPreAmp);
    for (int i=0; i<numCards; ++i)
    {
        const int height = juce::roundToInt(350*openness[i]);
        surfaces[i].setBounds(area.removeFromTop(82+height));
        auto header = surfaces[i].getLocalBounds().removeFromTop(48).reduced(8,0);
        surfaces[i].addAndMakeVisible(*cards[i]);
        power[i].setBounds(header.removeFromRight(60));
        cards[i]->setBounds(header);
        cards[i]->setColour(juce::TextButton::textColourOffId, power[i].getToggleState() ? AmberTheme::amberBright() : AmberTheme::text());
        effectPresets[i].setBounds(12, 46, surfaces[i].getWidth()-24, 28);
        reveals[i].setBounds(8,82, surfaces[i].getWidth()-16,height);
        reveals[i].setVisible(height>0);
        parameters[i].setBounds(0,0,reveals[i].getWidth(),350);
        parameters[i].setAlpha(.25f+.75f*openness[i]);
        auto content = parameters[i].getLocalBounds();
        if (i == 0)
        {
            driveType.setVisible(height > 0); driveType.setBounds(content.removeFromTop(30).reduced(4, 1)); content.removeFromTop(4);
            const auto show = [this, height](RotaryParameterControl& control) { control.setVisible(height > 0); parameters[0].addAndMakeVisible(control); };
            show(driveAmount); show(driveTone); show(driveLevel); show(driveMix); show(ampPush);
            if (currentState.driveType != 0) show(driveCharacter);
            if (currentState.driveType == 2) show(driveSustain);
            if (currentState.driveType == 0) layoutEffect(content, { &driveAmount, &driveTone, &driveLevel, &driveMix, &ampPush });
            else if (currentState.driveType == 1) layoutEffect(content, { &driveAmount, &driveTone, &driveCharacter, &driveLevel, &driveMix, &ampPush });
            else if (currentState.driveType == 2) layoutEffect(content, { &driveAmount, &driveTone, &driveCharacter, &driveSustain, &driveLevel, &driveMix, &ampPush });
            else layoutEffect(content, { &driveAmount, &driveCharacter, &driveTone, &driveLevel, &driveMix, &ampPush });
        }
        else
        {
            for (auto* c : groups[i - 1]) { parameters[i].addAndMakeVisible(*c); c->setVisible(height > 0); }
            auto direction = content.removeFromBottom(26);
            layoutEffect(content, groups[i - 1]);
            if (i == 1) { wahDirection.setVisible(height > 0); wahDirection.setBounds(direction.removeFromRight(70)); }
            if (i == 5) { compressorPreAmp.setVisible(height > 0); compressorPreAmp.setBounds(direction.removeFromRight(110)); }
            if (i == 6) { wahMouse.setVisible(height > 0 && mouseWahAvailable); wahMouse.setBounds(direction.removeFromRight(90)); }
        }
        area.removeFromTop(10);
    }
}
} // namespace nam::ui
