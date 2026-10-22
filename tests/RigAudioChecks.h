#pragma once

#include "library/RigLibrary.h"
#include "audio/NamPedalProcessor.h"
#include "audio/IRProcessor.h"
#include "NAM/get_dsp.h"
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <type_traits>

// CLI: runRigAudioChecks(resources/rig-models/catalog.json, report.xml).
// Link RigLibrary.cpp, NamPedalProcessor.cpp, IRProcessor.cpp, NAM core and
// JUCE DSP; compile all NAM translation units with NAM_SAMPLE_FLOAT.
// Offline only. No devices, VSTs, UI, network, WAV writes, or listening claims.
inline int runRigAudioChecks(juce::File catalog, juce::File reportFile)
{
    static_assert(std::is_same_v<NAM_SAMPLE, float>);
    constexpr double rate = 48000.0;
    constexpr int blockSize = 128;
    juce::XmlElement report("RigAudioChecks");
    report.setAttribute("catalog", catalog.getFullPathName());
    report.setAttribute("sampleRate", rate);
    report.setAttribute("maximumBlock", blockSize);
    report.setAttribute("inputReferencePeak", 0.12);
    report.setAttribute("ratePolicy", "Known NAM rates must match 48000; unknown legacy rates are run at 48000. No invented dBu calibration.");
    report.setAttribute("scope", "Actual pedal NAM -> amp NAM -> cabinet, as mapped; all additional effects absent. Technical checks only; no listening assessment.");
    int passes = 0, failures = 0;
    const auto check = [&](juce::XmlElement& parent, const juce::String& label, bool ok, const juce::String& detail = {})
    {
        auto* e = parent.createNewChildElement("check");
        e->setAttribute("name", label); e->setAttribute("pass", ok); e->setAttribute("detail", detail);
        if (ok) ++passes; else ++failures;
    };
    struct Stats
    {
        double sum = 0.0, squares = 0.0, peak = 0.0;
        int samples = 0, nonfinite = 0;
        void add(double x)
        {
            if (!std::isfinite(x)) { ++nonfinite; return; }
            sum += x; squares += x * x; peak = juce::jmax(peak, std::abs(x)); ++samples;
        }
        double rms() const { return std::sqrt(squares / juce::jmax(1, samples)); }
        double acRms() const
        {
            const double mean = sum / juce::jmax(1, samples);
            return std::sqrt(juce::jmax(0.0, squares / juce::jmax(1, samples) - mean * mean));
        }
        void write(juce::XmlElement& e) const
        {
            e.setAttribute("samples", samples); e.setAttribute("nonfinite", nonfinite);
            e.setAttribute("peak", peak); e.setAttribute("rms", rms()); e.setAttribute("acRms", acRms());
        }
    };
    RigLibrary library;
    juce::String error;
    const bool loaded = library.load(catalog, catalog.getParentDirectory().getParentDirectory(), &error);
    check(report, "catalog loads", loaded, error);
    check(report, "catalog contains rigs", loaded && !library.rigs().isEmpty());
    report.setAttribute("rigCount", library.rigs().size());
    if (loaded)
        for (const auto& rig : library.rigs())
        {
            auto* entry = report.createNewChildElement("rig");
            entry->setAttribute("id", rig.id); entry->setAttribute("name", rig.name);
            entry->setAttribute("routing", rig.routing);
            entry->setAttribute("ampFile", rig.ampFile.getFullPathName());
            entry->setAttribute("pedalFile", rig.pedalPath); entry->setAttribute("cabFile", rig.cabPath);
            try
            {
                const auto sound = RigLibrary::makeSound(rig, SoundState{});
                const bool pedalEnabled = rig.routing == "pedal-amp-cab";
                const bool cabEnabled = rig.routing != "full-rig";
                check(*entry, "exact mapped stages / no double cabinet",
                      sound.namPedalBypassed == !pedalEnabled && sound.irBypassed == !cabEnabled
                      && !sound.namBypassed && (cabEnabled || (rig.cabPath.isEmpty() && rig.pedalPath.isEmpty())));
                check(*entry, "no added host effects", sound.driveBypassed && sound.autoWahBypassed
                      && sound.chorusBypassed && sound.delayBypassed && sound.reverbBypassed);

                NamPedalProcessor pedal;
                pedal.prepare(rate, blockSize);
                if (pedalEnabled)
                {
                    pedal.set(rig.pedalFile.getFullPathName(), false, 0.0f, 0.0f);
                    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 15000.0;
                    while (pedal.name().isEmpty() && juce::Time::getMillisecondCounterHiRes() < deadline)
                    {
                        const auto status = pedal.lastError();
                        if (status.isNotEmpty() && !status.startsWith("Loading"))
                            throw std::runtime_error(status.toStdString());
                        juce::Thread::sleep(10);
                    }
                    if (pedal.name() != rig.pedalFile.getFileNameWithoutExtension())
                        throw std::runtime_error("Pedal load did not complete for requested model");
                }
                nam::DspLoadOptions options;
                options.prewarm = false;
                auto amp = nam::get_dsp(std::filesystem::path(rig.ampFile.getFullPathName().toWideCharPointer()), options);
                if (amp->NumInputChannels() != 1 || amp->NumOutputChannels() != 1)
                    throw std::runtime_error("Rig amp must be mono");
                const double expected = amp->GetExpectedSampleRate();
                entry->setAttribute("ampExpectedRate", expected);
                if (expected > 0.0 && std::abs(expected - rate) > .5)
                    throw std::runtime_error("Rig amp sample rate does not match 48000");
                amp->SetPrewarmOnReset(false);
                amp->Reset(rate, blockSize);
                amp->prewarm();
                IRProcessor cabinet;
                const juce::dsp::ProcessSpec spec { rate, blockSize, 1 };
                if (cabEnabled)
                {
                    cabinet.prepare(spec); // IRProcessor requires prepare before load.
                    const auto result = cabinet.loadAsync(rig.cabFile);
                    if (result.failed()) throw std::runtime_error(result.getErrorMessage().toStdString());
                    // JUCE's subsequent prepare guarantees the queued IR is ready.
                    cabinet.prepare(spec);
                    if (cabinet.currentIrSize() <= 0) throw std::runtime_error("Cabinet IR not ready");
                    entry->setAttribute("cabinetSamples", cabinet.currentIrSize());
                }
                entry->setAttribute("pedalStages", pedalEnabled ? 1 : 0);
                entry->setAttribute("ampStages", 1);
                entry->setAttribute("cabinetStages", cabEnabled ? 1 : 0);
                juce::AudioBuffer<float> storage(1, blockSize);
                Stats pedalStats, ampStats;
                const auto process = [&](int count, bool measure)
                {
                    auto* samples = storage.getWritePointer(0);
                    if (pedalEnabled) pedal.process(samples, count);
                    for (int i = 0; i < count; ++i)
                    {
                        if (!std::isfinite(samples[i])) throw std::runtime_error("Non-finite pedal output");
                        if (measure) pedalStats.add(samples[i]);
                    }
                    NAM_SAMPLE* channels[] { samples };
                    amp->process(channels, channels, count);
                    for (int i = 0; i < count; ++i)
                    {
                        if (!std::isfinite(samples[i])) throw std::runtime_error("Non-finite amp output");
                        if (measure) ampStats.add(samples[i]);
                    }
                    if (cabEnabled)
                    {
                        juce::AudioBuffer<float> view(&samples, 1, count);
                        cabinet.process(view);
                    }
                };
                // Settle both wet ramps on silence without resetting afterwards.
                for (int i = 0; i < 48; ++i)
                {
                    storage.clear(); process(blockSize, false);
                    for (int s = 0; s < blockSize; ++s)
                        if (!std::isfinite(storage.getSample(0, s))) throw std::runtime_error("Non-finite warmup output");
                }
                // 1 s steady sine; 4 s exponentially decaying note; 1 s silence.
                // The final seven samples exercise the partial-block chain.
                constexpr int frames = 6 * 48000 + 7;
                std::array<Stats, 3> windows;
                for (int offset = 0; offset < frames; offset += blockSize)
                {
                    const int count = juce::jmin(blockSize, frames - offset);
                    for (int i = 0; i < count; ++i)
                    {
                        const double t = (offset + i) / rate;
                        const double envelope = t < 1.0 ? 1.0 : t < 5.0 ? std::exp(-(t - 1.0) / 1.2) : 0.0;
                        storage.setSample(0, i, static_cast<float>(0.12 * envelope * std::sin(juce::MathConstants<double>::twoPi * 220.0 * t)));
                    }
                    process(count, true);
                    for (int i = 0; i < count; ++i)
                        windows[(offset + i) < 48000 ? 0 : (offset + i) < 240000 ? 1 : 2].add(storage.getSample(0, i));
                }
                pedalStats.write(*entry->createNewChildElement("pedalOutput"));
                ampStats.write(*entry->createNewChildElement("ampOutput"));
                const char* names[] { "steadySine", "fourSecondDecay", "silenceTail" };
                for (size_t i = 0; i < windows.size(); ++i)
                {
                    windows[i].write(*entry->createNewChildElement(names[i]));
                    check(*entry, juce::String(names[i]) + " finite", windows[i].nonfinite == 0);
                }
                check(*entry, "steady sine has nonzero AC output", windows[0].acRms() > 1.0e-8);
                check(*entry, "decaying note has nonzero AC output", windows[1].acRms() > 1.0e-8);
                check(*entry, "pedal remains current without fault", !pedalEnabled
                      || (pedal.name() == rig.pedalFile.getFileNameWithoutExtension() && pedal.lastError().isEmpty()), pedal.lastError());
                // Noise/DC and silence-tail energy are reported, not required to
                // vanish: NAM captures may contain noise or embedded effects.
            }
            catch (const std::exception& e) { check(*entry, "audio chain exception", false, e.what()); }
            catch (...) { check(*entry, "audio chain exception", false, "Unknown exception"); }
        }
    report.setAttribute("passes", passes); report.setAttribute("errors", failures); report.setAttribute("pass", failures == 0);
    if (reportFile.getParentDirectory().createDirectory().failed() || !report.writeTo(reportFile)) return 2;
    std::cout << "RigAudioChecks: " << passes << " passed, " << failures << " errors\n";
    return failures == 0 ? 0 : 1;
}
