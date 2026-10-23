#pragma once

#include "audio/RigChain.h"
#include "presets/PresetManager.h"

// Routing through real VST3 plugins: the guitar feeds an audio-to-MIDI
// tracker beside the chain, its notes are transposed and play an instrument
// that goes to the output. Run in a child process (plugin crashes).
inline int runChainRoutingChecks(const juce::File& trackerPath, const juce::File& synthPath, const juce::File& reportPath)
{
    juce::XmlElement report("ChainRoutingChecks");
    int failures = 0;
    const auto check = [&](const juce::String& name, bool ok, const juce::String& detail = {})
    {
        auto* e = report.createNewChildElement("check");
        e->setAttribute("name", name);
        e->setAttribute("pass", ok);
        e->setAttribute("detail", detail);
        failures += ok ? 0 : 1;
    };
    const auto first = [](const juce::File& file, bool instrument)
    {
        for (const auto& description : Vst3Slot::scanFile(file))
            if (description.isInstrument == instrument)
                return std::optional<juce::PluginDescription>(description);
        return std::optional<juce::PluginDescription>();
    };
    const auto tracker = first(trackerPath, false);
    const auto synth = first(synthPath, true);
    check("scan finds the tracker and the instrument", tracker.has_value() && synth.has_value());
    if (! tracker || ! synth)
    {
        report.writeTo(reportPath);
        return 1;
    }
    const auto pluginState = [](const juce::PluginDescription& description)
    {
        juce::ValueTree state("Vst3Slot");
        state.setProperty("version", 1, nullptr);
        state.setProperty("descriptionXml", description.createXml()->toString(), nullptr);
        state.setProperty("stateBase64", juce::String(), nullptr);
        return state;
    };

    constexpr double rate = 48000.0;
    constexpr int block = 256;
    RigChain rig;
    rig.prepare(rate, block);
    SoundState sound;
    sound.namBypassed = sound.irBypassed = true;
    rig.applyEffectState(sound);
    auto layout = chain::Layout::standard();
    const auto trackerId = layout.add({ {}, "vst3", tracker->name, pluginState(*tracker), false, 0 }, -1);
    const auto transposeId = layout.add({ {}, "transpose", {}, {}, false, -24 }, -1);
    const auto synthId = layout.add({ {}, "vst3", synth->name, pluginState(*synth), false, 0 }, -1);
    layout.addLink({ chain::input, trackerId, false, 0.0f });
    layout.addLink({ trackerId, transposeId, true, 0.0f });
    layout.addLink({ transposeId, synthId, true, 0.0f });
    layout.addLink({ synthId, chain::output, false, 0.0f });
    layout.chainLevelDb = -60.0f; // synth only
    rig.setLayout(layout);
    report.setAttribute("loadErrors", rig.loadErrors().joinIntoString(" | "));
    check("tracker loaded", rig.plugin(trackerId) != nullptr && rig.plugin(trackerId)->name().isNotEmpty());
    check("instrument loaded", rig.plugin(synthId) != nullptr && rig.plugin(synthId)->name().isNotEmpty());

    std::vector<float> mono(block), left(block), right(block);
    const auto run = [&](float amplitude, int blocks)
    {
        float peak = 0.0f;
        double phase = 0.0;
        for (int index = 0; index < blocks; ++index)
        {
            for (auto& sample : mono)
            {
                sample = amplitude * static_cast<float>(std::sin(phase));
                phase += juce::MathConstants<double>::twoPi * 110.0 / rate;
            }
            rig.processWithInputPeak(mono.data(), left.data(), right.data(), block);
            for (int sample = 0; sample < block; ++sample)
                peak = std::max({ peak, std::abs(left[static_cast<size_t>(sample)]), std::abs(right[static_cast<size_t>(sample)]) });
        }
        return peak;
    };
    run(0.0f, 20);
    const auto playing = run(0.5f, 60);
    check("guitar notes play the instrument", playing > 0.01f, juce::String(playing));
    run(0.0f, 200);
    const auto silent = run(0.0f, 20);
    check("silence: the instrument stops, the guitar stays muted", silent < 0.01f, juce::String(silent));

    auto unplugged = layout;
    unplugged.removeLink({ trackerId, transposeId, true, 0.0f });
    rig.setLayout(unplugged);
    check("plugins survive a rewire without reloading", rig.plugin(synthId) != nullptr && rig.plugin(synthId)->name().isNotEmpty());
    const auto withoutLink = run(0.5f, 60);
    check("without the MIDI link the instrument stays silent", withoutLink < 0.01f, juce::String(withoutLink));

    auto guitarOnly = unplugged;
    guitarOnly.chainLevelDb = 0.0f;
    rig.setLayout(guitarOnly);
    const auto guitar = run(0.5f, 20);
    check("the chain level brings the guitar back", guitar > 0.2f, juce::String(guitar));

    report.setAttribute("failures", failures);
    report.writeTo(reportPath);
    return failures == 0 ? 0 : 1;
}
