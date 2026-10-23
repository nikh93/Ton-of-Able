#include "TrackFxComponent.h"

#include <algorithm>

#include "audio/RigChain.h"
#include "ui/rig/ChainBlocks.h"

#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui
{
using Theme = AmberTheme;
using daw::text;

TrackFxComponent::TrackFxComponent(daw::ArrangementController& c, const juce::Uuid& id, Sources s)
    : controller(c), trackId(id), sources(std::move(s))
{
    insertMode.setButtonText(text("Insert (nachträglich)"));
    printMode.setButtonText("Print (bei Aufnahme)");
    fromLiveRig.setButtonText(text("Live-Rig übernehmen"));
    insertMode.setTooltip(text("Die Spur wird trocken aufgenommen; die Kette läuft bei der Wiedergabe und bleibt jederzeit änderbar."));
    printMode.setTooltip(text("Die Kette wird beim Aufnehmen in die Datei eingerechnet (wie ein Amp vor dem Mikro). Abhören läuft ebenfalls über die Kette."));
    fromLiveRig.setTooltip(text("Kopiert Amp, IR, Pedal, EQs, Effekte und VST3-Zustände vom Live-Rig in diese Spur"));
    for (auto* button : { &insertMode, &printMode })
        button->setRadioGroupId(8123);
    insertMode.setConnectedEdges(juce::Button::ConnectedOnRight);
    printMode.setConnectedEdges(juce::Button::ConnectedOnLeft);

    title.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, Theme::text());
    for (auto* label : { &modeInfo })
    {
        label->setColour(juce::Label::textColourId, Theme::subduedText());
        label->setFont(juce::FontOptions(12.0f));
    }

    fill(amp, sources.amps, "Kein NAM-Amp");
    fill(ir, sources.irs, "Kein IR");
    fill(pedal, sources.pedals, "Kein NAM-Pedal");

    active.onClick = [this] { enabled = active.getToggleState(); publish(); };
    insertMode.onClick = [this] { print = false; publish(); };
    printMode.onClick = [this] { print = true; publish(); };
    fromLiveRig.onClick = [this]
    {
        if (! sources.captureLiveRig)
            return;
        sound = sources.captureLiveRig();
        // Tracks have no MIDI keyboards.
        auto& links = sound.layout.links;
        links.erase(std::remove_if(links.begin(), links.end(), [](const chain::Link& l) { return l.from == chain::keys; }), links.end());
        enabled = true;
        showState();
        publish();
    };
    // The signal chain, as in the live rig (without MIDI keyboards).
    chainStrip.offerKeyboards = false;
    chainStrip.describe = [this](const chain::Node& node)
    {
        chainblocks::Context context;
        context.ampName = nameOf(sources.amps, sound.namId);
        context.irName = nameOf(sources.irs, sound.irId);
        context.pedalLoaded = sound.namPedalPath.isNotEmpty();
        context.pedalOn = context.pedalLoaded && ! sound.namPedalBypassed;
        context.pedalName = juce::File(sound.namPedalPath).getFileNameWithoutExtension();
        context.rig = trackChain();
        return chainblocks::describe(node, sound, context);
    };
    chainStrip.onToggle = [this](const juce::String& id)
    {
        if (const auto* node = sound.layout.find(id); node != nullptr && ! node->isBuiltin())
        {
            if (auto* rig = trackChain())
                chainblocks::togglePluginBlock(sound, *rig, id);
        }
        else
            chainblocks::toggleBuiltin(sound, id);
        enabled = true;
        showState();
        publish();
    };
    chainStrip.onOpen = [this](const juce::String& id)
    {
        const std::pair<const char*, int> cards[] { { "drive", 0 }, { "autoWah", 1 }, { "chorus", 2 }, { "delay", 3 },
                                                    { "reverb", 4 }, { "compressor", 5 }, { "wah", 6 } };
        for (const auto& [card, index] : cards)
            if (id == card)
                return effects.showCard(index);
        if (id == "amp") return amp.showPopup();
        if (id == "ir") return ir.showPopup();
        if (id == "pedal") return pedal.showPopup();
        const auto* node = sound.layout.find(id);
        auto* rig = trackChain();
        if (node == nullptr || node->kind != "vst3" || rig == nullptr)
            return;
        if (const auto existing = pluginWindows.find(id); existing != pluginWindows.end())
        {
            existing->second->setVisible(true);
            existing->second->toFront(true);
            return;
        }
        juce::String error;
        if (auto window = chainblocks::openPluginWindow(*rig, id, error))
            pluginWindows[id] = std::move(window);
        else
            modeInfo.setText(error, juce::dontSendNotification);
    };
    chainStrip.onLayoutChanged = [this](const chain::Layout& layout)
    {
        sound.layout = layout;
        for (auto it = pluginWindows.begin(); it != pluginWindows.end();) // editors go before their plugins
            it = layout.find(it->first) == nullptr ? pluginWindows.erase(it) : std::next(it);
        enabled = true;
        publish();
        resized();
    };
    chainStrip.plugins = [this] { return sources.plugins ? sources.plugins() : juce::Array<juce::PluginDescription>(); };
    chainStrip.rescanPlugins = [this](std::function<void()> done) { if (sources.rescanPlugins) sources.rescanPlugins(std::move(done)); };
    chainStrip.managePlugins = [this] { if (sources.managePlugins) sources.managePlugins(); };
    chainViewport.setViewedComponent(&chainStrip, false);
    chainViewport.setScrollBarsShown(false, true);
    chainViewport.setScrollBarThickness(8);
    amp.onChange = [this]
    {
        const auto index = amp.getSelectedItemIndex() - 1;
        sound.namId = index >= 0 ? sources.amps[static_cast<size_t>(index)].id : juce::String();
        sound.namBypassed = sound.namId.isEmpty();
        namOn.setToggleState(! sound.namBypassed, juce::dontSendNotification);
        enabled = true;
        publish();
    };
    ir.onChange = [this]
    {
        const auto index = ir.getSelectedItemIndex() - 1;
        sound.irId = index >= 0 ? sources.irs[static_cast<size_t>(index)].id : juce::String();
        sound.irBypassed = sound.irId.isEmpty();
        irOn.setToggleState(! sound.irBypassed, juce::dontSendNotification);
        enabled = true;
        publish();
    };
    pedal.onChange = [this]
    {
        const auto index = pedal.getSelectedItemIndex() - 1;
        sound.namPedalPath = index >= 0 ? sources.pedals[static_cast<size_t>(index)].id : juce::String();
        sound.namPedalBypassed = sound.namPedalPath.isEmpty();
        publish();
    };
    namOn.onClick = [this] { sound.namBypassed = ! namOn.getToggleState(); publish(); };
    irOn.onClick = [this] { sound.irBypassed = ! irOn.getToggleState(); publish(); };

    const auto setupKnob = [this](RotaryParameterControl& knob, float SoundState::* field)
    {
        knob.setRange(-24.0, 24.0, 0.1);
        knob.setValueFormatter([](double v) { return juce::String(v, 1) + " dB"; });
        knob.onValueChange = [this, field](double value) { sound.*field = static_cast<float>(value); publish(); };
    };
    setupKnob(inputGain, &SoundState::inputGainDb);
    setupKnob(bass, &SoundState::bassDb);
    setupKnob(mid, &SoundState::midDb);
    setupKnob(treble, &SoundState::trebleDb);
    setupKnob(outputGain, &SoundState::outputGainDb);

    pitch.onChange = [this](const PitchShiftComponent::Settings& settings)
    {
        sound.pitchBypassed = settings.bypassed;
        sound.pitchSemitones = settings.semitones;
        sound.pitchMix = settings.mix;
        sound.pitchFormantPreserved = settings.formantPreserved;
        sound.pitchHighQuality = settings.highQuality;
        sound.pitchAnalogOctave = settings.analogOctave;
        if (! settings.bypassed)
            enabled = true;
        publish();
    };
    graphicEq.onChange = [this](bool bypassed, const GraphicEqComponent::Bands& bands)
    {
        sound.graphicEqBypassed = bypassed;
        sound.graphicEqDb = bands;
        publish();
    };
    effects.onStateChanged = [this](const SoundState& edited)
    {
        // The panel owns only the effect rack; everything else stays as set here.
        auto next = edited;
        next.retainAmpFrom(sound);
        // The chain stays; only the compressor position comes from the panel.
        auto layout = sound.layout;
        if (edited.layout.compressorBeforeAmp() != layout.compressorBeforeAmp())
            layout.setCompressorBeforeAmp(edited.layout.compressorBeforeAmp());
        next.layout = layout;
        sound = next;
        publish();
    };

    for (auto* component : std::initializer_list<juce::Component*> { &title, &active, &insertMode, &printMode, &fromLiveRig, &amp, &ir, &pedal,
                                                                      &namOn, &irOn, &modeInfo, &chainViewport, &inputGain, &bass, &mid, &treble, &outputGain,
                                                                      &pitch, &graphicEq, &effects })
        addAndMakeVisible(*component);

    loadFromTrack();
    setSize(1080, 900);
}

TrackFxComponent::~TrackFxComponent()
{
    pluginWindows.clear(); // before the chain could go away
}

RigChain* TrackFxComponent::trackChain() const
{
    if (const auto* track = controller.state().findTrack(trackId))
        return dynamic_cast<RigChain*>(track->fx->processor.get());
    return nullptr;
}

juce::String TrackFxComponent::nameOf(const std::vector<Choice>& choices, const juce::String& id)
{
    for (const auto& choice : choices)
        if (choice.id == id && id.isNotEmpty())
            return choice.name;
    return {};
}

void TrackFxComponent::fill(juce::ComboBox& box, const std::vector<Choice>& choices, const juce::String& none)
{
    box.clear(juce::dontSendNotification);
    box.addItem(none, 1);
    for (size_t index = 0; index < choices.size(); ++index)
        box.addItem(choices[index].name, static_cast<int>(index) + 2);
}

void TrackFxComponent::select(juce::ComboBox& box, const std::vector<Choice>& choices, const juce::String& id)
{
    box.setSelectedItemIndex(0, juce::dontSendNotification);
    for (size_t index = 0; index < choices.size(); ++index)
        if (choices[index].id == id && id.isNotEmpty())
            box.setSelectedItemIndex(static_cast<int>(index) + 1, juce::dontSendNotification);
}

void TrackFxComponent::loadFromTrack()
{
    const auto* track = controller.state().findTrack(trackId);
    if (track == nullptr)
        return;
    title.setText(text("Spur-Effekte: ") + track->name, juce::dontSendNotification);
    enabled = track->fx->enabled.load();
    print = track->fx->printWhileRecording.load();
    if (const auto stored = SoundState::fromValueTree(track->fx->settings))
        sound = *stored;
    else
    {
        // A new chain starts neutral: no amp, no IR, all effects off.
        sound = SoundState::defaults();
        sound.namBypassed = true;
        sound.irBypassed = true;
    }
    printMode.setEnabled(track->inputChannel >= 0 && ! track->isReturn());
    insertMode.setEnabled(! track->isReturn());
    showState();
}

void TrackFxComponent::showState()
{
    active.setToggleState(enabled, juce::dontSendNotification);
    insertMode.setToggleState(! print, juce::dontSendNotification);
    printMode.setToggleState(print, juce::dontSendNotification);
    select(amp, sources.amps, sound.namId);
    select(ir, sources.irs, sound.irId);
    select(pedal, sources.pedals, sound.namPedalBypassed ? juce::String() : sound.namPedalPath);
    namOn.setToggleState(! sound.namBypassed, juce::dontSendNotification);
    irOn.setToggleState(! sound.irBypassed, juce::dontSendNotification);
    inputGain.setValue(sound.inputGainDb, juce::dontSendNotification);
    bass.setValue(sound.bassDb, juce::dontSendNotification);
    mid.setValue(sound.midDb, juce::dontSendNotification);
    treble.setValue(sound.trebleDb, juce::dontSendNotification);
    outputGain.setValue(sound.outputGainDb, juce::dontSendNotification);
    graphicEq.setValues(sound.graphicEqBypassed, sound.graphicEqDb);
    pitch.setSettings({ sound.pitchBypassed, sound.pitchSemitones, sound.pitchMix, sound.pitchFormantPreserved, sound.pitchHighQuality, sound.pitchAnalogOctave });
    effects.setState(sound);
    chainStrip.setLayout(sound.layout);
    const auto* track = controller.state().findTrack(trackId);
    modeInfo.setText(track != nullptr && track->isReturn()
                         ? text("Return-Spur: die Kette bearbeitet, was die Spuren per SEND schicken – Hall/Delay hier auf 100 % Mix lassen.")
                         : track != nullptr && track->inputChannel < 0
                         ? text("Rig-Spur: aufgenommen wird das Live-Rig; diese Kette wirkt zusätzlich als Insert bei der Wiedergabe.")
                         : print ? text("Print: Effekte werden in die Aufnahme eingerechnet.")
                                 : text("Insert: trocken aufnehmen, Effekte bei der Wiedergabe – nachträglich änderbar."),
                     juce::dontSendNotification);
}

void TrackFxComponent::publish()
{
    // Keep what was edited in the plugin windows.
    if (auto* rig = trackChain())
        sound.layout = rig->captureLayout(sound.layout);
    active.setToggleState(enabled, juce::dontSendNotification);
    controller.setTrackFx(trackId, sound.toValueTree(), enabled, print);
    showState();
}

void TrackFxComponent::paint(juce::Graphics& g)
{
    g.fillAll(Theme::background());
}

void TrackFxComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    auto top = area.removeFromTop(30);
    title.setBounds(top.removeFromLeft(320));
    fromLiveRig.setBounds(top.removeFromRight(170));
    top.removeFromRight(10);
    printMode.setBounds(top.removeFromRight(160));
    insertMode.setBounds(top.removeFromRight(160));
    top.removeFromRight(10);
    active.setBounds(top);
    area.removeFromTop(4);
    modeInfo.setBounds(area.removeFromTop(20));
    area.removeFromTop(8);

    auto selectors = area.removeFromTop(28);
    const auto third = (selectors.getWidth() - 120) / 3;
    pedal.setBounds(selectors.removeFromLeft(third).withTrimmedRight(6));
    namOn.setBounds(selectors.removeFromLeft(60));
    amp.setBounds(selectors.removeFromLeft(third).withTrimmedRight(6));
    irOn.setBounds(selectors.removeFromLeft(60));
    ir.setBounds(selectors);
    area.removeFromTop(8);
    {
        const auto stripWidth = chainStrip.preferredWidth(area.getWidth());
        chainViewport.setBounds(area.removeFromTop(chainStrip.preferredHeight() + (stripWidth > area.getWidth() ? 10 : 0)));
        chainStrip.setSize(stripWidth, chainStrip.preferredHeight());
    }
    area.removeFromTop(8);

    auto knobs = area.removeFromTop(118);
    const auto cell = knobs.getWidth() / 5;
    for (auto* knob : { &inputGain, &bass, &mid, &treble, &outputGain })
        knob->setBounds(knobs.removeFromLeft(cell).reduced(4));
    area.removeFromTop(8);
    auto left = area.removeFromLeft(area.getWidth() / 2).withTrimmedRight(8);
    pitch.setBounds(left.removeFromTop(200));
    left.removeFromTop(8);
    graphicEq.setBounds(left);
    effects.setBounds(area.withTrimmedLeft(8));
}
} // namespace nam::ui
