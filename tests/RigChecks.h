#pragma once

#include "library/RigLibrary.h"
#include <iostream>

// CLI entry: catalog is resources/rig-models/catalog.json. No DSP or device.
inline int runRigChecks(juce::File catalog, juce::File reportFile)
{
    juce::XmlElement report("RigChecks");
    int passed = 0, errors = 0;
    const auto check = [&](const juce::String& label, bool ok, const juce::String& detail = {})
    {
        auto* e = report.createNewChildElement("check");
        e->setAttribute("name", label); e->setAttribute("pass", ok); e->setAttribute("detail", detail);
        if (ok) ++passed; else ++errors;
    };
    RigLibrary library;
    const auto resources = catalog.getParentDirectory().getParentDirectory();
    juce::String error;
    const bool loaded = library.load(catalog, resources, &error);
    check("catalog validates", loaded, error);
    report.setAttribute("catalog", catalog.getFullPathName());
    report.setAttribute("rigCount", library.rigs().size());
    SoundState current;
    current.inputGainDb = 3.0f; current.outputGainDb = -7.0f;
    current.bassDb = 5.0f; current.midDb = -4.0f; current.trebleDb = 6.0f;
    current.namInputTrimDb = 9.0f;
    current.namPedalInputDb = 8.0f; current.namPedalOutputDb = -9.0f;
    current.namPedalPath = "old-pedal";
    current.driveBypassed = current.autoWahBypassed = current.chorusBypassed = current.delayBypassed = current.reverbBypassed = false;
    juce::ValueTree pluginState("Vst3Slot");
    pluginState.setProperty("version", 1, nullptr);
    pluginState.setProperty("bypassed", false, nullptr);
    pluginState.setProperty("descriptionXml", "opaque description", nullptr);
    pluginState.setProperty("stateBase64", "opaque state", nullptr);
    juce::ValueTree child("opaqueChild");
    child.setProperty("value", "keep", nullptr);
    pluginState.addChild(child, -1, nullptr);
    const auto pre = current.layout.add({ {}, "vst3", "Pre", pluginState, false, 0 }, current.layout.serialIndex("pedal"));
    const auto post = current.layout.add({ {}, "vst3", "Post", pluginState.createCopy(), false, 0 }, -1);
    const auto beforeLayout = current.layout.toValueTree().toXmlString();
    const auto verify = [&](const RigEntry& entry)
    {
        auto sound = RigLibrary::makeSound(entry, current);
        check(entry.id + ": gains retained and amp EQ neutral", sound.inputGainDb == 3.0f && sound.outputGainDb == -7.0f
            && sound.bassDb == 0.0f && sound.midDb == 0.0f && sound.trebleDb == 0.0f && sound.namInputTrimDb == 0.0f);
        check(entry.id + ": routing avoids double cabinet", !sound.namBypassed && sound.irBypassed == (entry.routing == "full-rig")
            && sound.namPedalBypassed == (entry.routing != "pedal-amp-cab")
            && sound.namPedalPath == (entry.routing == "pedal-amp-cab" ? entry.pedalFile.getFullPathName() : juce::String{})
            && sound.namPedalInputDb == 0.0f && sound.namPedalOutputDb == 0.0f);
        check(entry.id + ": extras disabled", sound.driveBypassed && sound.autoWahBypassed && sound.chorusBypassed
            && sound.delayBypassed && sound.reverbBypassed && sound.layout.find(pre) != nullptr && sound.layout.find(pre)->bypassed
            && sound.layout.find(post) != nullptr && sound.layout.find(post)->bypassed);
        const auto* preNode = sound.layout.find(pre);
        check(entry.id + ": opaque plugin data preserved", preNode != nullptr && preNode->plugin["descriptionXml"].toString() == "opaque description"
            && preNode->plugin["stateBase64"].toString() == "opaque state"
            && preNode->plugin.getChild(0)["value"].toString() == "keep");
        if (auto* node = sound.layout.find(pre))
            node->plugin.getChild(0).setProperty("value", "changed copy", nullptr);
        check(entry.id + ": current plugin trees are not shared", current.layout.toValueTree().toXmlString() == beforeLayout);
        check(entry.id + ": rig identity and deferred asset mapping", sound.rigId == entry.id && sound.namId.isEmpty() && sound.irId.isEmpty());
    };
    if (loaded)
        for (const auto& entry : library.rigs())
        {
            check(entry.id + ": find returns entry", library.find(entry.id) == &entry);
            check(entry.id + ": amp exists", entry.ampFile.existsAsFile());
            verify(entry);
        }
    // Cover all routing branches even when the production catalog lacks one.
    for (const auto* routing : { "full-rig", "amp-cab", "pedal-amp-cab" })
    {
        RigEntry entry;
        entry.id = juce::String("mapping-") + routing; entry.routing = routing;
        entry.pedalFile = resources.getChildFile("test-pedal.nam");
        entry.embeddedEffects.add("Delay"); // Must NOT turn on the external delay.
        verify(entry);
    }
    if (loaded && !library.rigs().isEmpty())
    {
        const auto count = library.rigs().size();
        const auto retainedId = library.rigs()[0].id;
        // Malformed full-rig mapping uses a real existing asset, so rejection
        // must be routing validation, not a missing-file side effect.
        juce::DynamicObject::Ptr entry = new juce::DynamicObject();
        entry->setProperty("id", "invalid-double-cab"); entry->setProperty("name", "Test");
        entry->setProperty("description", "Test"); entry->setProperty("provenance", "Curated");
        entry->setProperty("routing", "full-rig"); entry->setProperty("pedalFile", "");
        entry->setProperty("ampFile", library.rigs()[0].ampRelativePath);
        entry->setProperty("cabFile", library.rigs()[0].ampRelativePath);
        entry->setProperty("embeddedEffects", juce::var(juce::Array<juce::var>{}));
        juce::Array<juce::var> entries; entries.add(juce::var(entry.get()));
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        root->setProperty("rigs", juce::var(entries));
        juce::TemporaryFile fixture(".json");
        if (fixture.getFile().replaceWithText(juce::JSON::toString(juce::var(root.get()))))
        {
            check("catalog forbids full-rig external cabinet", !library.load(fixture.getFile(), resources, &error)
                && error.contains("full-rig forbids"), error);
            check("failed catalog load is transactional", library.rigs().size() == count && library.find(retainedId) != nullptr);
        }
        else check("write temporary validation fixture", false);
    }
    report.setAttribute("passes", passed); report.setAttribute("errors", errors); report.setAttribute("pass", errors == 0);
    if (reportFile.getParentDirectory().createDirectory().failed() || !report.writeTo(reportFile)) return 2;
    std::cout << "RigChecks: " << passed << " passed, " << errors << " errors\n";
    return errors == 0 ? 0 : 1;
}
