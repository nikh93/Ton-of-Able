#pragma once

#include "audio/plugins/PluginInstrument.h"

#include <array>

// Device-free check of a VST3 drum instrument (e.g. JDrummer) as a drum-track
// sound source: loading, General MIDI drum notes, sample-accurate timing in
// export-sized blocks, state and the offline copy used for exports.
// Run in a disposable child process: native plugin crashes cannot be caught.
//   "Ton of Able.exe" --drum-instrument-test <plugin.vst3> <report.xml>
inline int runDrumInstrumentChecks(juce::File pluginPath, juce::File reportPath)
{
    juce::XmlElement report("DrumInstrumentChecks");
    report.setAttribute("pluginPath", pluginPath.getFullPathName());
    int failures = 0;
    const auto check = [&](const juce::String& name, bool ok, const juce::String& detail = {})
    {
        auto* e = report.createNewChildElement("check");
        e->setAttribute("name", name);
        e->setAttribute("pass", ok);
        e->setAttribute("detail", detail);
        failures += ok ? 0 : 1;
    };

    const auto descriptions = PluginInstrument::scanFile(pluginPath);
    check("scan finds an instrument", ! descriptions.isEmpty(), juce::String(descriptions.size()));
    if (descriptions.isEmpty())
    {
        report.writeTo(reportPath);
        return 1;
    }
    juce::String error;
    auto instrument = PluginInstrument::fromSettings(PluginInstrument::settingsFor(descriptions[0]), error);
    check("load " + descriptions[0].name, instrument != nullptr, error);
    if (instrument == nullptr)
    {
        report.writeTo(reportPath);
        return 1;
    }

    constexpr int block = 8192; // the export block size
    instrument->prepare(48000.0, block);
    juce::AudioBuffer<float> out(2, block);
    const auto renderOnset = [&](int note, int offset, float& peak)
    {
        // Release everything (some SoundFont hits sustain until note-off) and let it decay.
        std::array<daw::DrumEvent, daw::numDrumLanes> offs {};
        for (size_t index = 0; index < offs.size(); ++index)
            offs[index] = { 0, static_cast<juce::uint8>(daw::drumLanes()[index].note), 0 };
        instrument->render(offs.data(), static_cast<int>(offs.size()), out.getWritePointer(0), out.getWritePointer(1), block);
        // SoundFont releases can be long (an open hi-hat may ring ~20 s): wait for silence.
        for (int i = 0; i < 400; ++i)
        {
            instrument->render(nullptr, 0, out.getWritePointer(0), out.getWritePointer(1), block);
            if (out.getMagnitude(0, block) < 1.0e-6f)
                break;
        }
        const daw::DrumEvent events[] { { offset, static_cast<juce::uint8>(note), 110 } };
        instrument->render(events, 1, out.getWritePointer(0), out.getWritePointer(1), block);
        int onset = -1;
        peak = 0.0f;
        for (int sample = 0; sample < block; ++sample)
        {
            const auto value = std::max(std::abs(out.getSample(0, sample)), std::abs(out.getSample(1, sample)));
            peak = std::max(peak, value);
            if (onset < 0 && value > 1.0e-4f)
                onset = sample;
        }
        return onset;
    };

    for (const auto& lane : daw::drumLanes())
    {
        float peak = 0.0f;
        const auto leadIn = renderOnset(lane.note, 0, peak);  // silence at the start of the sample itself
        const auto onset = renderOnset(lane.note, 5021, peak);
        check(juce::String(lane.name) + " (note " + juce::String(lane.note) + ") sounds on time",
              leadIn >= 0 && onset - leadIn == 5021 && peak > 0.001f,
              "lead-in " + juce::String(leadIn) + ", onset " + juce::String(onset) + ", peak " + juce::String(peak, 4));
    }

    const auto settings = instrument->currentSettings();
    check("state is saved", settings.getProperty("stateBase64").toString().isNotEmpty());
    check("offline copy for export", instrument->createOfflineCopy() != nullptr);
    report.setAttribute("failures", failures);
    report.writeTo(reportPath);
    return failures == 0 ? 0 : 1;
}
