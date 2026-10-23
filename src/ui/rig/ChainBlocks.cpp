#include "ChainBlocks.h"

#include "audio/RigChain.h"
#include "audio/effects/LivePitchShifter.h"
#include "daw/DawText.h"
#include "ui/components/AmberTheme.h"

namespace nam::ui::chainblocks
{
using daw::text;

SignalChainStrip::Block describe(const chain::Node& node, const SoundState& sound, const Context& context)
{
    const auto percent = [](float value) { return juce::String(juce::roundToInt(value * 100.0f)) + " %"; };
    const auto signedSemitones = [](float value) { return (value > 0.0f ? "+" : "") + juce::String(value, 0); };
    const auto& kind = node.kind;
    if (kind == "vst3")
    {
        auto* slot = context.rig != nullptr ? context.rig->plugin(node.id) : nullptr;
        const auto loaded = slot != nullptr && slot->name().isNotEmpty();
        bool instrument = false;
        if (const auto xml = juce::parseXML(node.plugin.getProperty("descriptionXml").toString()))
            instrument = xml->getBoolAttribute("isInstrument");
        return { instrument ? "SYNTH" : "VST3", loaded ? slot->name() : node.name + text(" (nicht geladen)"),
                 loaded && ! node.bypassed, true, ! loaded,
                 instrument ? text("VST3-Instrument: spielt MIDI aus den Verbindungen (Keyboards, Audio → MIDI)")
                            : text("VST3-Plugin") };
    }
    if (kind == "transpose")
        return { "MIDI", signedSemitones(static_cast<float>(node.semitones)) + text(" Halbtöne"), ! node.bypassed, true, false,
                 text("Verschiebt MIDI-Noten (Rechtsklick: Halbtöne wählen)") };
    if (kind == "pitch")
        return { "PITCH", signedSemitones(sound.pitchSemitones) + text(" · ")
                     + (nam::effects::LivePitchShifter::usesAnalogOctave(sound.pitchAnalogOctave, sound.pitchSemitones) ? juce::String("Analog")
                        : sound.pitchHighQuality ? "R3" : "R2"),
                 ! sound.pitchBypassed, true, false, text("Live-Transponieren (Rubber Band oder Analog-Oktaver)") };
    if (kind == "compressor")
        return { "KOMP", juce::String(sound.compressorRatio, 1) + ":1 · " + juce::String(sound.compressorThresholdDb, 0) + " dB",
                 ! sound.compressorBypassed, true, false,
                 text("Kompressor ") + (sound.layout.compressorBeforeAmp() ? text("vor dem Amp (Pedal)") : text("nach dem Amp (Studio)")) };
    if (kind == "wah")
        return { "WAH", sound.pedalWahMouse ? text("Maus · ") + percent(sound.pedalWahPosition) : percent(sound.pedalWahPosition),
                 ! sound.pedalWahBypassed, true, false, text("Wah-Pedal (Taste W: Maussteuerung)") };
    if (kind == "drive")
    {
        const juce::String types[] { "Crunch", "Overdrive", "Lead", "Fuzz" };
        return { "DRIVE", types[juce::jlimit(0, 3, sound.driveType)], ! sound.driveBypassed, true, false, text("Verzerrer") };
    }
    if (kind == "pedal")
        return { "NAM-PEDAL", context.pedalLoaded ? context.pedalName : text("leer"), context.pedalOn, true, ! context.pedalLoaded,
                 text("NAM-Capture eines Pedals") };
    if (kind == "amp")
        return { "AMP", context.ampName.isNotEmpty() ? context.ampName : text("kein Modell"), ! sound.namBypassed && context.ampName.isNotEmpty(),
                 true, context.ampName.isEmpty(), text("NAM-Amp") };
    if (kind == "ir")
        return { "BOX / IR", context.irName.isNotEmpty() ? context.irName : text("kein IR"), ! sound.irBypassed && context.irName.isNotEmpty(),
                 true, context.irName.isEmpty(), text("Cabinet-Impulsantwort (bei Full-Rig-Modellen aus)") };
    if (kind == "ampEq")
        return { "AMP-EQ", "B " + juce::String(sound.bassDb, 0) + text(" · M ") + juce::String(sound.midDb, 0) + text(" · T ") + juce::String(sound.trebleDb, 0),
                 true, false, false, text("Bass, Mid, Treble des Amps") };
    if (kind == "geq")
        return { "GRAPHIC EQ", text("10 Bänder"), ! sound.graphicEqBypassed, true, false, text("10-Band-Equalizer") };
    if (kind == "autoWah")
        return { "AUTO-WAH", juce::String(sound.autoWahMinimumHz, 0) + text("–") + juce::String(sound.autoWahMaximumHz, 0) + " Hz",
                 ! sound.autoWahBypassed, true, false, text("Hüllkurvengesteuertes Wah") };
    if (kind == "chorus")
        return { "CHORUS", juce::String(sound.chorusRateHz, 1) + text(" Hz · ") + percent(sound.chorusMix), ! sound.chorusBypassed, true, false, "Chorus" };
    if (kind == "delay")
        return { "DELAY", juce::String(sound.delayTimeMs, 0) + text(" ms · ") + percent(sound.delayMix), ! sound.delayBypassed, true, false, "Delay" };
    if (kind == "reverb")
        return { "REVERB", juce::String(sound.reverbDecay, 1) + text(" s · ") + percent(sound.reverbMix), ! sound.reverbBypassed, true, false, "Hall" };
    return { kind.toUpperCase(), {}, false, false, false, {} };
}

bool toggleBuiltin(SoundState& sound, const juce::String& kind)
{
    bool* field = kind == "pitch" ? &sound.pitchBypassed
                : kind == "compressor" ? &sound.compressorBypassed
                : kind == "wah" ? &sound.pedalWahBypassed
                : kind == "drive" ? &sound.driveBypassed
                : kind == "pedal" ? &sound.namPedalBypassed
                : kind == "amp" ? &sound.namBypassed
                : kind == "ir" ? &sound.irBypassed
                : kind == "geq" ? &sound.graphicEqBypassed
                : kind == "autoWah" ? &sound.autoWahBypassed
                : kind == "chorus" ? &sound.chorusBypassed
                : kind == "delay" ? &sound.delayBypassed
                : kind == "reverb" ? &sound.reverbBypassed
                : nullptr;
    if (field == nullptr)
        return false;
    *field = ! *field;
    return true;
}

namespace
{
class PluginWindow final : public juce::DocumentWindow
{
public:
    PluginWindow(const juce::String& name, juce::AudioProcessorEditor* editor)
        : DocumentWindow(name, AmberTheme::surface(), closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(editor, true); // deleting the editor detaches it from the plugin
        setResizable(editor->isResizable(), false);
        centreWithSize(getWidth(), getHeight());
    }
    void closeButtonPressed() override { setVisible(false); }
};
} // namespace

std::unique_ptr<juce::DocumentWindow> openPluginWindow(RigChain& rig, const juce::String& id, juce::String& error)
{
    auto* slot = rig.plugin(id);
    auto* instance = slot != nullptr ? slot->instance() : nullptr;
    if (instance == nullptr)
    {
        error = text("Das Plugin ist nicht geladen – fehlt die Datei? (Rechtsklick: entfernen)");
        return {};
    }
    auto* editor = instance->createEditorIfNeeded();
    if (editor == nullptr)
    {
        error = slot->name() + text(" hat keine eigene Oberfläche.");
        return {};
    }
    auto window = std::make_unique<PluginWindow>(slot->name(), editor);
    window->setVisible(true);
    return window;
}

void togglePluginBlock(SoundState& sound, RigChain& rig, const juce::String& id)
{
    if (auto* node = sound.layout.find(id); node != nullptr && ! node->isBuiltin())
    {
        node->bypassed = ! node->bypassed;
        rig.setBlockBypassed(id, node->bypassed);
    }
}
} // namespace nam::ui::chainblocks
