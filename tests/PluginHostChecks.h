#pragma once

#include "audio/plugins/Vst3Slot.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

// Run synchronously from JUCEApplication::initialise, before creating the normal
// window. No nested event loop, network, preset downloads, or visible editor.
// Run in a disposable child process: native plugin crashes cannot be caught here.
inline int runPluginHostChecks(juce::File pluginPath, juce::File reportPath)
{
    juce::XmlElement report("PluginHostChecks");
    report.setAttribute("pluginPath", pluginPath.getFullPathName());
    report.setAttribute("timeUTC", juce::Time::getCurrentTime().toISO8601(true));
    int passes = 0, failures = 0, skips = 0;
    const auto check = [&](juce::XmlElement& parent, const juce::String& label, bool ok,
                           const juce::String& detail = {})
    {
        auto* e = parent.createNewChildElement("check");
        e->setAttribute("name", label);
        e->setAttribute("pass", ok);
        e->setAttribute("detail", detail);
        if (ok) ++passes; else ++failures;
    };
    const auto skip = [&](juce::XmlElement& parent, const juce::String& label, const juce::String& why)
    {
        auto* e = parent.createNewChildElement("skip");
        e->setAttribute("name", label); e->setAttribute("reason", why); ++skips;
    };
    const auto pump = [](Vst3Slot& slot, int channels, int blockSize, double rate, int frames,
                         bool silence, bool compareDry, juce::XmlElement& metrics)
    {
        double sum = 0.0, peak = 0.0, error = 0.0;
        int nonfinite = 0, samples = 0, shortBlocks = 0;
        juce::AudioBuffer<float> storage(channels, blockSize), original(channels, blockSize);
        for (int offset = 0; offset < frames; offset += blockSize)
        {
            const int count = juce::jmin(blockSize, frames - offset);
            if (count < blockSize) ++shortBlocks;
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < count; ++i)
                {
                    const float x = silence ? 0.0f : static_cast<float>(0.12 * std::sin(
                        juce::MathConstants<double>::twoPi * (ch == 0 ? 220.0 : 337.0) * (offset + i) / rate));
                    storage.setSample(ch, i, x); original.setSample(ch, i, x);
                }
            juce::AudioBuffer<float> block(storage.getArrayOfWritePointers(), channels, count);
            slot.process(block);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < count; ++i)
                {
                    const double y = block.getSample(ch, i);
                    if (!std::isfinite(y)) { ++nonfinite; continue; }
                    peak = juce::jmax(peak, std::abs(y)); sum += y * y; ++samples;
                    if (compareDry) error = juce::jmax(error, std::abs(y - original.getSample(ch, i)));
                }
        }
        metrics.setAttribute("frames", frames); metrics.setAttribute("shortBlocks", shortBlocks);
        metrics.setAttribute("nonfinite", nonfinite); metrics.setAttribute("peak", peak);
        metrics.setAttribute("rms", std::sqrt(sum / juce::jmax(1, samples)));
        metrics.setAttribute("maximumDryError", error);
        metrics.setAttribute("latencySamples", slot.latencySamples());
        return nonfinite == 0 && (!compareDry || error <= 1.0e-6);
    };
    // One continuous source spans priming and measurement. This avoids resetting
    // phase at a block/test boundary while the slot still contains delayed audio.
    const auto probe = [](Vst3Slot& slot, int channels, int blockSize, double rate,
                          bool bypass, juce::XmlElement& metrics, bool directPlugin = false)
    {
        slot.setBypassed(bypass);
        const int latency = slot.latencySamples();
        const int priming = latency + static_cast<int>(std::ceil(0.1 * rate));
        const int measuredFrames = (static_cast<int>(rate / 10) / blockSize + 1) * blockSize + 7;
        // Align the measurement start, but deliberately end with a short block.
        const int start = ((priming + blockSize - 1) / blockSize) * blockSize;
        const int total = start + measuredFrames;
        const auto source = [rate](int ch, int frame)
        {
            if (frame < 0) return 0.0f;
            const double t = frame / rate;
            const double f = ch == 0 ? 220.0 : 337.0;
            const double phase = juce::MathConstants<double>::twoPi;
            // Deterministic modulated two-tone source; peak bounded by 0.12.
            return static_cast<float>((0.8 + 0.2 * std::sin(phase * 3.7 * t))
                * (0.09 * std::sin(phase * f * t) + 0.03 * std::sin(phase * (f * 1.619) * t)));
        };
        juce::AudioBuffer<float> storage(channels, blockSize);
        juce::MidiBuffer directMidi;
        double outputSum = 0.0, referenceSum = 0.0, delayedErrorSum = 0.0, immediateErrorSum = 0.0;
        double maximumError = 0.0, peak = 0.0;
        int samples = 0, nonfinite = 0, shortBlocks = 0;
        bool stableLatency = true;
        for (int offset = 0; offset < total; offset += blockSize)
        {
            const int count = juce::jmin(blockSize, total - offset);
            if (count < blockSize) ++shortBlocks;
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < count; ++i) storage.setSample(ch, i, source(ch, offset + i));
            juce::AudioBuffer<float> block(storage.getArrayOfWritePointers(), channels, count);
            if (directPlugin)
            {
                directMidi.clear();
                slot.instance()->processBlock(block, directMidi);
            }
            else slot.process(block);
            stableLatency = stableLatency && slot.latencySamples() == latency;
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < count; ++i)
                {
                    const double y = block.getSample(ch, i);
                    if (!std::isfinite(y)) { ++nonfinite; continue; }
                    if (offset + i < start) continue;
                    const double delayed = source(ch, offset + i - latency);
                    const double immediate = source(ch, offset + i);
                    const double error = y - delayed;
                    outputSum += y * y; referenceSum += delayed * delayed;
                    delayedErrorSum += error * error;
                    immediateErrorSum += (y - immediate) * (y - immediate);
                    maximumError = juce::jmax(maximumError, std::abs(error));
                    peak = juce::jmax(peak, std::abs(y)); ++samples;
                }
        }
        const double denominator = juce::jmax(1, samples);
        const double outputRms = std::sqrt(outputSum / denominator);
        const double referenceRms = std::sqrt(referenceSum / denominator);
        const double differenceRms = std::sqrt(delayedErrorSum / denominator);
        const double immediateDifferenceRms = std::sqrt(immediateErrorSum / denominator);
        const double threshold = juce::jmax(1.0e-5, referenceRms * 1.0e-3);
        metrics.setAttribute("latencySamples", latency);
        metrics.setAttribute("directPlugin", directPlugin);
        metrics.setAttribute("stableLatency", stableLatency);
        metrics.setAttribute("primingFrames", start);
        metrics.setAttribute("measuredFrames", measuredFrames);
        metrics.setAttribute("shortBlocks", shortBlocks);
        metrics.setAttribute("nonfinite", nonfinite);
        metrics.setAttribute("outputPeak", peak);
        metrics.setAttribute("outputRms", outputRms);
        metrics.setAttribute("delayedDryRms", referenceRms);
        metrics.setAttribute("delayedDryDifferenceRms", differenceRms);
        metrics.setAttribute("immediateDryDifferenceRms", immediateDifferenceRms);
        metrics.setAttribute("maximumDelayedDryError", maximumError);
        metrics.setAttribute("nonDryThresholdRms", threshold);
        metrics.setAttribute("bypassTolerance", 1.0e-6);
        // Reject both immediate fault/lock fallback and latency-only pass-through.
        // Require audible-path energy as well, so a silent/muted plugin fails.
        return nonfinite == 0 && stableLatency && samples > 0
            && (bypass ? maximumError <= 1.0e-6
                       : outputRms > 1.0e-6 && differenceRms > threshold && immediateDifferenceRms > threshold);
    };
    try
    {
        const auto* mm = juce::MessageManager::getInstanceWithoutCreating();
        if (mm == nullptr || !mm->isThisTheMessageThread())
            throw std::runtime_error("Checks require the JUCE message thread");
        const auto descriptions = Vst3Slot::scanFile(pluginPath);
        check(report, "scan finds audio effects", !descriptions.isEmpty());
        report.setAttribute("typeCount", descriptions.size());
        for (const auto& desc : descriptions)
        {
            auto* type = report.createNewChildElement("plugin");
            type->addChildElement(desc.createXml().release());
            try
            {
                Vst3Slot slot;
                const auto loaded = slot.load(desc);
                check(*type, "load", loaded.wasOk(), loaded.getErrorMessage());
                if (loaded.failed()) continue;
                auto* p = slot.instance();
                check(*type, "instance present", p != nullptr);
                if (p == nullptr) continue;
                auto* programs = type->createNewChildElement("programs");
                programs->setAttribute("count", p->getNumPrograms());
                programs->setAttribute("current", p->getCurrentProgram());
                int usefulProgram = -1;
                int kingOfToneProgram = -1;
                for (int i = 0; i < p->getNumPrograms(); ++i)
                {
                    const auto programName = p->getProgramName(i);
                    auto* item = programs->createNewChildElement("program");
                    item->setAttribute("index", i); item->setAttribute("name", programName);
                    const auto lower = programName.toLowerCase();
                    if (usefulProgram < 0 && (lower.contains("mayer") || lower.contains("clapton")
                        || lower.contains("blues") || lower.contains("zendrive") || lower.contains("king of tone")
                        || lower.contains("tube screamer") || lower.contains("lead"))) usefulProgram = i;
                    if (programName.equalsIgnoreCase("John Mayer")) usefulProgram = i;
                    if (programName.equalsIgnoreCase("King Of Tone")) kingOfToneProgram = i;
                }
                if (usefulProgram >= 0)
                {
                    p->setCurrentProgram(usefulProgram);
                    programs->setAttribute("selectedCandidate", p->getProgramName(usefulProgram));
                    check(*programs, "select useful program candidate", p->getCurrentProgram() == usefulProgram);
                }
                else skip(*programs, "useful factory program selection", "No matching named program exposed; use plugin preset browser then save slot state. Default may be an empty chain.");
                auto* route = type->createNewChildElement("presetRoute");
                route->setAttribute("runtimeSelected", usefulProgram >= 0);
                route->setAttribute("guidance", "Prefer an exposed named program; otherwise select a preset in the native editor and persist Vst3Slot::saveState. Host parameters alone may not construct the processor graph.");
                if (desc.name.containsIgnoreCase("BYOD"))
                {
                    route->setAttribute("source", "https://github.com/Chowdhury-DSP/BYOD/tree/main/res/presets");
                    route->setAttribute("candidates", "John Mayer; Clapton; King Of Tone; Tube Screamer; ZenDrive (names from upstream source; installed availability and tone unverified)");
                    route->setAttribute("stateRoute", "Upstream BYOD serialises StateManager XML with copyXmlToBinary; .chowpreset contains a proc_chain, not a complete VST3 host state. Do not pass preset XML directly to Vst3Slot::restoreState or invent parameter mappings.");
                }
                for (double rate : { 44100.0, 48000.0, 96000.0 })
                    for (int block : { 32, 128, 512 })
                        for (int channels : { 1, 2 })
                        {
                            auto* test = type->createNewChildElement("audio");
                            test->setAttribute("rate", rate); test->setAttribute("block", block);
                            test->setAttribute("channels", channels);
                            slot.prepare(rate, block); slot.setBypassed(false);
                            // 50 ms settles bypass fade, then 11 blocks + 7 tests partial lengths.
                            juce::XmlElement settling("settling");
                            pump(slot, channels, block, rate, static_cast<int>(rate / 20), true, false, settling);
                            auto* sine = test->createNewChildElement("sine");
                            check(*test, "sine finite", pump(slot, channels, block, rate, block * 11 + 7, false, false, *sine));
                            auto* silence = test->createNewChildElement("silence");
                            check(*test, "silence/tail finite", pump(slot, channels, block, rate, block * 11 + 7, true, false, *silence));
                            if (usefulProgram >= 0)
                            {
                                auto* wet = test->createNewChildElement("wetProcessing");
                                wet->setAttribute("program", slot.instance()->getProgramName(usefulProgram));
                                check(*test, "selected program measurably differs from immediate and delayed dry",
                                      probe(slot, channels, block, rate, false, *wet));
                            }
                            else skip(*test, "non-dry processing", "No useful named program selected; default may intentionally pass through");
                            auto* bypass = test->createNewChildElement("bypass");
                            check(*test, "bypass equals latency-delayed continuous dry after fade",
                                  probe(slot, channels, block, rate, true, *bypass));
                        }
                if (desc.name.containsIgnoreCase("Supermassive"))
                {
                    auto* tail = type->createNewChildElement("supermassiveImpulseTail");
                    tail->setAttribute("rate", 48000);
                    tail->setAttribute("block", 128);
                    tail->setAttribute("seconds", 4);
                    tail->setAttribute("impulseLeft", 0.1);
                    tail->setAttribute("impulseRight", 0.1);
                    tail->setAttribute("lateWindowSeconds", "1.0 through 4.0");
                    tail->setAttribute("parameters", "fresh plugin defaults, unchanged");
                    Vst3Slot reverb;
                    reverb.prepare(48000.0, 128);
                    const auto result = reverb.load(desc);
                    check(*tail, "fresh 48k load", result.wasOk(), result.getErrorMessage());
                    if (result.wasOk())
                    {
                        reverb.setBypassed(false);
                        juce::AudioBuffer<float> block(2, 128);
                        int nonfinite = 0;
                        // 128 ms of silence settles the slot's 10 ms wet fade.
                        // Do not reset the slot or plugin after this priming.
                        for (int n = 0; n < 48; ++n)
                        {
                            block.clear(); reverb.process(block);
                            for (int ch = 0; ch < 2; ++ch)
                                for (int i = 0; i < 128; ++i)
                                    if (!std::isfinite(block.getSample(ch, i))) ++nonfinite;
                        }
                        double peak = 0.0, lateSum = 0.0, stereoSum = 0.0;
                        int lateFrames = 0;
                        for (int offset = 0; offset < 192000; offset += 128)
                        {
                            block.clear();
                            if (offset == 0)
                            {
                                block.setSample(0, 0, 0.1f);
                                block.setSample(1, 0, 0.1f);
                            }
                            reverb.process(block);
                            for (int i = 0; i < 128; ++i)
                            {
                                const double l = block.getSample(0, i), r = block.getSample(1, i);
                                if (!std::isfinite(l) || !std::isfinite(r)) { ++nonfinite; continue; }
                                peak = juce::jmax(peak, juce::jmax(std::abs(l), std::abs(r)));
                                if (offset + i >= 48000)
                                {
                                    lateSum += 0.5 * (l * l + r * r);
                                    stereoSum += (l - r) * (l - r);
                                    ++lateFrames;
                                }
                            }
                        }
                        const double lateRms = std::sqrt(lateSum / juce::jmax(1, lateFrames));
                        const double stereoRms = std::sqrt(stereoSum / juce::jmax(1, lateFrames));
                        tail->setAttribute("nonfinite", nonfinite);
                        tail->setAttribute("peak", peak);
                        tail->setAttribute("lateRms", lateRms);
                        tail->setAttribute("lateStereoDifferenceRms", stereoRms);
                        tail->setAttribute("minimumRms", 1.0e-8);
                        tail->setAttribute("latencySamples", reverb.latencySamples());
                        check(*tail, "four-second impulse response finite", nonfinite == 0);
                        check(*tail, "nonzero late tail (1-4 seconds)", lateRms > 1.0e-8);
                        check(*tail, "stereo difference in late tail", stereoRms > 1.0e-8);
                    }
                }
                slot.prepare(48000.0, 128); slot.setBypassed(false);
                // Keep the canonical matrix above unchanged. Separate fresh and
                // rate-transition probes isolate initialisation from reprepare.
                if (usefulProgram >= 0)
                {
                    auto* diagnostics = type->createNewChildElement("rateDiagnostics");
                    const int diagnosticProgram = kingOfToneProgram >= 0 ? kingOfToneProgram : usefulProgram;
                    diagnostics->setAttribute("program", p->getProgramName(diagnosticProgram));
                    diagnostics->setAttribute("purpose", "Retain explicit King Of Tone 96k regression failures independently of the John Mayer main matrix");
                    const auto initialise = [&](Vst3Slot& target, double rate)
                    {
                        target.prepare(rate, 128);
                        const auto result = target.load(desc);
                        check(*diagnostics, "diagnostic load at " + juce::String(rate), result.wasOk(), result.getErrorMessage());
                        if (result.failed()) return false;
                        target.instance()->setCurrentProgram(diagnosticProgram);
                        target.prepare(rate, 128);
                        return true;
                    };
                    Vst3Slot fresh;
                    if (initialise(fresh, 96000.0))
                    {
                        auto* e = diagnostics->createNewChildElement("fresh96000");
                        check(*diagnostics, "fresh 96k selected program non-dry", probe(fresh, 2, 128, 96000.0, false, *e));
                    }
                    Vst3Slot direct;
                    if (initialise(direct, 96000.0))
                    {
                        auto* e = diagnostics->createNewChildElement("direct96000");
                        check(*diagnostics, "direct plugin 96k selected program non-dry", probe(direct, 2, 128, 96000.0, false, *e, true));
                    }
                    Vst3Slot mayer;
                    mayer.prepare(96000.0, 128);
                    const auto mayerLoad = mayer.load(desc);
                    if (mayerLoad.wasOk())
                        for (int i = 0; i < mayer.instance()->getNumPrograms(); ++i)
                            if (mayer.instance()->getProgramName(i).equalsIgnoreCase("John Mayer"))
                            {
                                mayer.instance()->setCurrentProgram(i);
                                mayer.prepare(96000.0, 128);
                                auto* e = diagnostics->createNewChildElement("johnMayer96000");
                                check(*diagnostics, "John Mayer fresh 96k non-dry", probe(mayer, 2, 128, 96000.0, false, *e));
                                break;
                            }
                    Vst3Slot transition;
                    if (initialise(transition, 48000.0))
                        for (const double rate : { 48000.0, 96000.0, 48000.0 })
                        {
                            transition.prepare(rate, 128);
                            auto* e = diagnostics->createNewChildElement("transition");
                            e->setAttribute("rate", rate);
                            check(*diagnostics, "48-96-48 transition non-dry at " + juce::String(rate),
                                  probe(transition, 2, 128, rate, false, *e));
                        }
                }
                p = slot.instance();
                auto* params = type->createNewChildElement("parameters");
                int chosen = -1;
                for (int i = 0; i < p->getParameters().size(); ++i)
                {
                    auto* param = p->getParameters()[i];
                    const auto n = param->getName(128);
                    auto* item = params->createNewChildElement("parameter");
                    item->setAttribute("index", i); item->setAttribute("name", n);
                    item->setAttribute("value", static_cast<double>(param->getValue()));
                    item->setAttribute("automatable", param->isAutomatable());
                    if (chosen < 0 && param->isAutomatable() && !param->isMetaParameter()
                        && !n.trim().isEmpty() && !n.containsIgnoreCase("bypass")
                        && !n.containsIgnoreCase("unused") && !n.containsIgnoreCase("program")) chosen = i;
                }
                const auto state = slot.saveState();
                check(*type, "save valid state", state.isValid() && state.hasProperty("descriptionXml") && state.hasProperty("stateBase64"));
                type->setAttribute("encodedStateCharacters", state["stateBase64"].toString().length());
                float before = 0.0f;
                juce::String parameterName;
                if (chosen >= 0)
                {
                    auto* param = p->getParameters()[chosen];
                    parameterName = param->getName(128); before = param->getValue();
                    param->beginChangeGesture(); param->setValueNotifyingHost(before < .5f ? .8f : .2f); param->endChangeGesture();
                    check(*params, "parameter tweak takes effect", std::abs(param->getValue() - before) > 1.0e-4f, parameterName);
                }
                else skip(*params, "parameter roundtrip", "No meaningful automatable parameter exposed by this graph");
                slot.setBypassed(true);
                const auto restored = slot.restoreState(state);
                check(*type, "restore state", restored.wasOk(), restored.getErrorMessage());
                p = slot.instance();
                check(*type, "restored plugin present and bypass restored", p != nullptr && !slot.isBypassed());
                if (chosen >= 0 && restored.wasOk() && p != nullptr)
                    check(*params, "parameter restored", chosen < p->getParameters().size()
                        && p->getParameters()[chosen]->getName(128) == parameterName
                        && std::abs(p->getParameters()[chosen]->getValue() - before) <= 1.0e-4f, parameterName);
                if (restored.wasOk())
                {
                    slot.clear();
                    check(*type, "clear removes instance", slot.instance() == nullptr);
                    const auto fresh = slot.restoreState(state);
                    check(*type, "restore into empty slot", fresh.wasOk() && slot.instance() != nullptr, fresh.getErrorMessage());
                }
                auto missing = desc;
                missing.fileOrIdentifier = pluginPath.getParentDirectory().getNonexistentChildFile("__missing_plugin_host_check", ".vst3").getFullPathName();
                auto* old = slot.instance(); const auto oldName = slot.name(); const auto oldBypass = slot.isBypassed();
                const auto missingResult = slot.load(missing);
                check(*type, "missing plugin retains old instance", missingResult.failed() && old != nullptr
                    && slot.instance() == old && slot.name() == oldName && slot.isBypassed() == oldBypass, missingResult.getErrorMessage());
                p = slot.instance();
                if (p != nullptr && p->hasEditor())
                {
                    { std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditorIfNeeded());
                      check(*type, "native editor creation (not shown)", editor != nullptr); }
                    check(*type, "editor destroyed", p->getActiveEditor() == nullptr);
                }
                else skip(*type, "native editor", "Plugin reports no editor");
                slot.clear();
            }
            catch (const std::exception& e) { check(*type, "exception", false, e.what()); }
            catch (...) { check(*type, "exception", false, "Unknown plugin exception"); }
        }
    }
    catch (const std::exception& e) { check(report, "exception", false, e.what()); }
    catch (...) { check(report, "exception", false, "Unknown exception"); }
    report.setAttribute("passes", passes); report.setAttribute("errors", failures);
    report.setAttribute("skips", skips); report.setAttribute("pass", failures == 0);
    report.setAttribute("scope", "Host smoke checks including continuous delayed bypass and non-dry selected-program output. Not a sound-quality, allocation, threading, or crash-safety certification. No listening performed.");
    if (reportPath.getParentDirectory().createDirectory().failed() || !report.writeTo(reportPath))
    { std::cerr << "Cannot write plugin check report: " << reportPath.getFullPathName() << '\n'; return 2; }
    std::cout << "PluginHostChecks: " << passes << " passed, " << failures << " errors, " << skips
              << " skipped; " << reportPath.getFullPathName() << '\n';
    return failures == 0 ? 0 : 1;
}
