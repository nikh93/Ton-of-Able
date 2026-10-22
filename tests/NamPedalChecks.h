#pragma once

#include "audio/NamPedalProcessor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

// CLI integration: runNamPedalChecks(model, report) from --nam-pedal-test.
// Requires NamPedalProcessor.cpp + NAM core (NAM_SAMPLE_FLOAT), audio_basics/core.
// No device or editor required; worker completion is bounded by a 15 s timeout.
inline int runNamPedalChecks(juce::File model, juce::File reportFile)
{
    juce::XmlElement report("NamPedalChecks");
    report.setAttribute("model", model.getFullPathName());
    int passed = 0, errors = 0;
    const auto check = [&](const juce::String& label, bool ok, const juce::String& detail = {})
    {
        auto* e = report.createNewChildElement("check");
        e->setAttribute("name", label); e->setAttribute("pass", ok); e->setAttribute("detail", detail);
        if (ok) ++passed; else ++errors;
    };
    try
    {
        NamPedalProcessor pedal;
        pedal.prepare(48000.0, 128);
        const auto awaitReady = [&]()
        {
            const auto deadline = juce::Time::getMillisecondCounterHiRes() + 15000.0;
            while (juce::Time::getMillisecondCounterHiRes() < deadline)
            {
                if (pedal.name().isNotEmpty()) return true;
                const auto error = pedal.lastError();
                if (error.isNotEmpty() && !error.startsWith("Loading")) return false;
                juce::Thread::sleep(10);
            }
            return false;
        };
        const auto run = [&](bool expectDry, int blockSize, int blocks)
        {
            double maxError = 0.0, peak = 0.0;
            bool finite = true;
            juce::AudioBuffer<float> audio(1, blockSize), original(1, blockSize);
            for (int n = 0; n < blocks; ++n)
            {
                const int count = n == blocks - 1 ? juce::jmin(7, blockSize) : blockSize;
                for (int i = 0; i < count; ++i)
                {
                    const float x = static_cast<float>(.12 * std::sin(juce::MathConstants<double>::twoPi * 220.0 * (n * blockSize + i) / 48000.0));
                    audio.setSample(0, i, x); original.setSample(0, i, x);
                }
                pedal.process(audio.getWritePointer(0), count);
                for (int i = 0; i < count; ++i)
                {
                    const double y = audio.getSample(0, i);
                    finite = finite && std::isfinite(y);
                    peak = juce::jmax(peak, std::abs(y));
                    maxError = juce::jmax(maxError, std::abs(y - original.getSample(0, i)));
                }
            }
            auto* metrics = report.createNewChildElement("audio");
            metrics->setAttribute("blockSize", blockSize); metrics->setAttribute("blocks", blocks);
            metrics->setAttribute("finalBlockSamples", juce::jmin(7, blockSize));
            metrics->setAttribute("finite", finite); metrics->setAttribute("peak", peak);
            metrics->setAttribute("expectDry", expectDry); metrics->setAttribute("maximumDryError", maxError);
            return finite && (expectDry ? maxError <= 1.0e-6 : maxError > 1.0e-5 && peak > 1.0e-8);
        };
        const auto settle = [&]()
        {
            juce::AudioBuffer<float> silence(1, 128);
            for (int i = 0; i < 50; ++i) { silence.clear(); pedal.process(silence.getWritePointer(0), 128); }
        };
        check("empty pedal is dry", run(true, 128, 4));
        pedal.set(model.getFullPathName(), false, 0.0f, 0.0f);
        const bool loaded = awaitReady();
        check("async model ready", loaded, pedal.lastError());
        if (!loaded) throw std::runtime_error("A compatible mono model is required for remaining checks");
        check("name belongs to requested model", pedal.name() == model.getFileNameWithoutExtension());
        check("ready has no error", pedal.lastError().isEmpty());
        check("wet finite including short block", run(false, 128, 50));
        const auto originalName = pedal.name();
        pedal.set(model.getFullPathName(), true, 6.0f, -2.0f);
        check("same-path control update does not reload", pedal.name() == originalName && pedal.lastError().isEmpty());
        settle();
        check("settled bypass excludes trims and equals dry", run(true, 128, 8));

        // Format generation invalidates the old prepared instance immediately.
        pedal.prepare(48000.0, 32);
        check("smaller format asynchronously prepared", awaitReady(), pedal.lastError());
        pedal.setControls(false, -3.0f, 3.0f);
        check("new format and partial blocks finite", run(false, 32, 80));
        pedal.prepare(48000.0, 512);
        check("larger format asynchronously prepared", awaitReady(), pedal.lastError());
        check("larger blocks finite", run(false, 512, 12));

        const auto missing = model.getParentDirectory().getNonexistentChildFile("missing-nam-pedal-check", ".nam").getFullPathName();
        pedal.set(missing, false, 24.0f, 24.0f);
        check("missing request immediately disables old model", pedal.name().isEmpty() && run(true, 128, 4));
        check("missing request fails with an error", !awaitReady() && pedal.lastError().isNotEmpty(), pedal.lastError());
        pedal.setControls(false, 12.0f, 12.0f);
        check("unbypass after failure cannot revive old model", run(true, 128, 4) && pedal.name().isEmpty());

        // Supersede a valid in-flight load; wait so any stale completion has time
        // to arrive, and verify that only the latest missing request is observed.
        pedal.set(model.getFullPathName(), false, 0.0f, 0.0f);
        pedal.set(missing, false, 0.0f, 0.0f);
        check("latest request wins over stale valid load", !awaitReady() && pedal.name().isEmpty() && run(true, 128, 4), pedal.lastError());
        pedal.set(model.getFullPathName(), false, 0.0f, 0.0f);
        check("valid reselection recovers", awaitReady(), pedal.lastError());
        pedal.prepare(96000.0,128);
        check("48k fixture rejects wrong device rate and remains dry", !awaitReady() && pedal.lastError().contains("sample rate") && run(true,128,4));
        pedal.prepare(48000.0,128);
        check("matching device rate recovers", awaitReady(),pedal.lastError());
        pedal.set({}, false, 24.0f, 24.0f);
        check("clear immediately dry without old name or error", pedal.name().isEmpty() && pedal.lastError().isEmpty() && run(true, 128, 4));
    }
    catch (const std::exception& e) { check("exception", false, e.what()); }
    catch (...) { check("exception", false, "Unknown exception"); }
    report.setAttribute("passes", passed); report.setAttribute("errors", errors); report.setAttribute("pass", errors == 0);
    if (reportFile.getParentDirectory().createDirectory().failed() || !report.writeTo(reportFile)) return 2;
    std::cout << "NamPedalChecks: " << passed << " passes, " << errors << " errors\n";
    return errors == 0 ? 0 : 1;
}
