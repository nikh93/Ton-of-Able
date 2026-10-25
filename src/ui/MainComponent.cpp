#include "MainComponent.h"
#include "daw/SampleKit.h"
#include "ui/rig/ChainBlocks.h"

namespace
{
using Theme = nam::ui::AmberTheme;

// MSVC can interpret plain narrow literals using the active Windows code page.
// Make every German UI string unambiguously UTF-8 before handing it to JUCE.
juce::String uiText(const char* utf8Text)
{
    return juce::String(juce::CharPointer_UTF8(utf8Text));
}
}

namespace
{
// A plain DocumentWindow like the app's other windows: juce::DialogWindow
// brings its own desktop scale handling, and with Windows display scaling
// its clicks landed vertically offset from the drawn controls.
class AudioSettingsDialog final : public juce::DocumentWindow
{
public:
    AudioSettingsDialog(juce::AudioDeviceManager& manager, const juce::File& logFile, std::function<void()> closed)
        : juce::DocumentWindow("Audio-Einstellungen", Theme::surface(), closeButton), onClosed(std::move(closed))
    {
        setUsingNativeTitleBar(true);
        content = std::make_unique<Content>(manager, logFile);
        // The window keeps its own size; the device panel grows inside a scroll area
        // instead of resizing the window from within whenever devices change.
        setContentNonOwned(content.get(), false);
        setResizable(true, false);
        setResizeLimits(620, 420, 1000, 900);
    }

    void closeButtonPressed() override
    {
        if (onClosed) onClosed();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            closeButtonPressed();
            return true;
        }
        return DocumentWindow::keyPressed(key);
    }

private:
    struct Content final : juce::Component
    {
        Content(juce::AudioDeviceManager& manager, const juce::File& logFile) : selector(manager, 1, 16, 2, 2, false, false, false, false)
        {
            viewport.setViewedComponent(&selector, false);
            viewport.setScrollBarsShown(true, false);
            addAndMakeVisible(viewport);
            openLog.setButtonText(uiText("Protokoll-Ordner öffnen"));
            openLog.setTooltip(logFile.getFullPathName());
            openLog.onClick = [logFile] { logFile.revealToUser(); };
            addAndMakeVisible(openLog);
        }
        void resized() override
        {
            auto bounds = getLocalBounds();
            openLog.setBounds(bounds.removeFromBottom(36).reduced(8, 5).removeFromRight(200));
            viewport.setBounds(bounds);
            selector.setSize(viewport.getMaximumVisibleWidth(), std::max(selector.getHeight(), bounds.getHeight()));
        }
        juce::AudioDeviceSelectorComponent selector;
        juce::Viewport viewport;
        juce::TextButton openLog;
    };
    std::unique_ptr<Content> content;
    std::function<void()> onClosed;
};
// A track's own chain remembers which assets it has loaded, so knob edits
// never reload NAM models, IRs or VST3 plugins.
struct TrackRig final : RigChain
{
    juce::String namId, irId;
};

// Shows a drum track's VST3 instrument GUI. Keeps the instrument alive until
// the editor is gone, because plugins must outlive their editors.
class InstrumentEditorWindow final : public juce::DocumentWindow
{
public:
    InstrumentEditorWindow(std::shared_ptr<PluginInstrument> owner, juce::AudioProcessorEditor* editor)
        : DocumentWindow(owner->name(), Theme::surface(), closeButton), instrument(std::move(owner))
    {
        setUsingNativeTitleBar(true);
        setContentOwned(editor, true);
        setResizable(editor->isResizable(), false);
        centreWithSize(getWidth(), getHeight());
    }
    ~InstrumentEditorWindow() override { clearContentComponent(); }
    void closeButtonPressed() override { setVisible(false); }

private:
    std::shared_ptr<PluginInstrument> instrument;
};

class ShortcutWindow final : public juce::DocumentWindow
{
public:
    explicit ShortcutWindow(juce::KeyPressMappingSet& mappings)
        : DocumentWindow(uiText("Tastenkürzel"), Theme::surface(), closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new nam::ui::ShortcutEditor(mappings), true);
        setResizable(true, false);
        setResizeLimits(520, 480, 1200, 1400);
        centreWithSize(getWidth(), getHeight());
    }
    void closeButtonPressed() override { setVisible(false); }
};

class PedalWindow final : public juce::DocumentWindow
{
public:
    explicit PedalWindow(NamPedalPanel& panel) : DocumentWindow("NAM-Pedal", Theme::surface(), closeButton)
    {
        setUsingNativeTitleBar(true); setContentNonOwned(&panel,true);
        setResizable(true,false); setResizeLimits(560,300,1200,700); centreWithSize(640,340);
    }
    void closeButtonPressed() override { setVisible(false); }
};

}

MainComponent::MainComponent()
{
    Theme::applyTo(lookAndFeel); setLookAndFeel(&lookAndFeel);
    // Separate windows (track effects, plugin rack, call-outs) share the theme.
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
    openWav.setButtonText(uiText("Datei öffnen"));
    chooseWavFolder.setButtonText(uiText("Ordner öffnen"));
    record.setIconColour(nam::ui::AmberTheme::record());
    liveMode.setConnectedEdges(juce::Button::ConnectedOnRight);
    wavMode.setConnectedEdges(juce::Button::ConnectedOnLeft);
    configureAudio.setButtonText(uiText("Einstellungen"));
    configureAudio.setTooltip(uiText("Audio-Interface, Ein-/Ausgänge, Samplerate und Puffergröße"));
    audioSummaryLabel.onClick = [this] { showAudioSettings(); };
    audioSummaryLabel.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    audioSummaryLabel.setTooltip(uiText("Aktuelles Audiogerät – Klick öffnet die Einstellungen"));
    liveHint.setJustificationType(juce::Justification::topLeft);
    liveHint.setMinimumHorizontalScale(1.0f);
    addChildComponent(liveHint);
    liveMode.setTooltip(uiText("Live-Eingang des Interfaces durch das Rig"));
    wavMode.setTooltip(uiText("WAV-Datei statt Gitarre durch das Rig spielen – zum Vergleichen von Amps und Presets"));
    record.setTooltip(uiText("Den Rig-Ausgang als WAV aufnehmen (für Mehrspuraufnahmen das Arrangement nutzen)"));
    diagnosticsToggle.setTooltip(uiText("Latenz, Auslastung, Aussetzer und Protokoll"));
    librariesToggle.setTooltip(uiText("NAM- und IR-Bibliothek: umbenennen, Tags, fehlende Dateien neu verknüpfen"));
    selectNam.setTooltip(uiText("NAM-Modelle importieren (mehrere Dateien oder ganze Ordner, auch per Drag & Drop)"));
    selectIr.setTooltip(uiText("Cabinet-IRs (.wav) importieren"));
    namEnabled.setTooltip(uiText("NAM-Amp an/aus"));
    irEnabled.setTooltip(uiText("Cabinet-IR an/aus (bei Full-Rig-Modellen automatisch aus)"));
    selectNam.setButtonText(uiText("NAM importieren"));
    selectIr.setButtonText(uiText("IR importieren"));
    juce::String stateError;
    namLibrary.loadValueTree(audioEngine.appSettings().loadNamLibrary(), &stateError);
    irLibrary.loadValueTree(audioEngine.appSettings().loadIrLibrary(), &stateError);
    presetManager.loadValueTree(audioEngine.appSettings().loadPresetLibrary(), &stateError);
    curatedAmpLibrary.load(defaultAssetsDirectory().getChildFile("curated_catalog.json"), &stateError);
    const auto personalCatalog=defaultAssetsDirectory().getSiblingFile("lead-models").getChildFile("catalog.json");
    if (personalCatalog.existsAsFile()) curatedAmpLibrary.load(personalCatalog,&stateError,true);
    registerBundledDefaults();
    modelFolderRoot = juce::File(audioEngine.appSettings().loadUiState().getProperty("modelFolder").toString());
    if (! juce::File::isAbsolutePath(modelFolderRoot.getFullPathName()) || modelFolderRoot == juce::File())
        modelFolderRoot = ModelFolder::defaultLocation();
    syncModelFolder();
    pedalPanel.addBundledModels(defaultAssetsDirectory().getSiblingFile("pedal-models"));
    pedalPanel.setSize(640, 340);
    configureRigs();
    title.setText("TON OF ABLE", juce::dontSendNotification); title.setFont(juce::FontOptions(26.0f, juce::Font::bold)); title.setColour(juce::Label::textColourId, Theme::text()); addAndMakeVisible(title);
    status.setText(uiText("Bereit"), juce::dontSendNotification); status.setColour(juce::Label::textColourId, Theme::subduedText()); addAndMakeVisible(status);
    latency.setColour(juce::Label::textColourId, Theme::amber()); latency.setJustificationType(juce::Justification::centredRight); addAndMakeVisible(latency);
    audioSummaryLabel.setColour(juce::Label::textColourId, Theme::subduedText()); audioSummaryLabel.setJustificationType(juce::Justification::centredRight); addAndMakeVisible(audioSummaryLabel);
    for (juce::TextButton* b : std::initializer_list<juce::TextButton*> { &configureAudio, &selectNam, &selectIr, &liveMode, &wavMode, &openWav, &chooseWavFolder, &playWav, &pauseWav, &stopWav, &record, &savePreset, &toggleAB }) addAndMakeVisible(*b);
    ampSelector.setTextWhenNothingSelected("AMP");
    ampSelector.setTextWhenNoChoicesAvailable("AMP");
    configurationSelector.setTextWhenNothingSelected("KONFIGURATION");
    configurationSelector.setTextWhenNoChoicesAvailable("KONFIGURATION");
    addAndMakeVisible(ampSelector); addAndMakeVisible(configurationSelector);
    addAndMakeVisible(presetBrowser);
    addAndMakeVisible(effectParameters);
    addAndMakeVisible(wavBrowser); wavBrowser.setVisible(false);
    addAndMakeVisible(namBrowser); addAndMakeVisible(irBrowser);
    namBrowser.setVisible(false); irBrowser.setVisible(false);
    addAndMakeVisible(loopWav); addAndMakeVisible(namEnabled); addAndMakeVisible(irEnabled); configureMainControls();
    wavTimeline.setSliderStyle(juce::Slider::LinearHorizontal);
    wavTimeline.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    wavTimeline.setRange(0.0, 1.0, 0.001);
    wavTimeline.onValueChange = [this] { audioEngine.player().setPositionNormalized(static_cast<float>(wavTimeline.getValue())); };
    addAndMakeVisible(wavTimeline);
    for (auto* toggle : { &wahEnabled, &chorusEnabled, &delayEnabled, &reverbEnabled }) addAndMakeVisible(*toggle);
    effectParameters.onStateChanged = [this](const SoundState& state)
    {
        const auto ampState = captureSoundState(false);
        // The panel moves only the compressor (pedal or studio position); the
        // rest of the chain stays as edited in the signal chain.
        auto layout = currentSound.layout;
        currentSound = state;
        currentSound.retainAmpFrom(ampState);
        if (state.layout.compressorBeforeAmp() != layout.compressorBeforeAmp())
            layout.setCompressorBeforeAmp(state.layout.compressorBeforeAmp());
        currentSound.layout = layout;
        audioEngine.rigChain().setLayout(layout);
        wahEnabled.setToggleState(!state.autoWahBypassed, juce::dontSendNotification);
        chorusEnabled.setToggleState(!state.chorusBypassed, juce::dontSendNotification);
        delayEnabled.setToggleState(!state.delayBypassed, juce::dontSendNotification);
        reverbEnabled.setToggleState(!state.reverbBypassed, juce::dontSendNotification);
        audioEngine.applyEffectState(currentSound);
        updateChainStrip();
    };
    addAndMakeVisible(diagnosticsToggle);
    addAndMakeVisible(librariesToggle);
    addAndMakeVisible(diagnosticsPanel);
    diagnosticsPanel.setVisible(false);
    for (auto* c : { &inputGain, &bass, &mid, &treble, &outputGain }) addAndMakeVisible(*c);
    inputMeter.setLabel("INPUT"); outputMeter.setLabel("OUTPUT"); addAndMakeVisible(inputMeter); addAndMakeVisible(outputMeter);
    configureAudio.onClick = [this] { showAudioSettings(); };
    selectNam.onClick = [this] { chooseNamFile(); }; selectIr.onClick = [this] { chooseIrFile(); };
    liveMode.onClick = [this] { audioEngine.setSourceMode(AudioEngine::SourceMode::live); wavBrowser.setVisible(false); status.setText(uiText("Live-Eingang aktiv"), juce::dontSendNotification); resized(); };
    wavMode.onClick = [this] { audioEngine.setSourceMode(AudioEngine::SourceMode::wav); wavBrowser.setVisible(true); status.setText("WAV-Testmodus aktiv", juce::dontSendNotification); if (! wavBrowser.directory().isDirectory()) chooseWavDirectory(); resized(); };
    openWav.onClick = [this] { chooseWavFile(); }; chooseWavFolder.onClick = [this] { chooseWavDirectory(); }; playWav.onClick = [this] { audioEngine.player().play(); }; pauseWav.onClick = [this] { audioEngine.player().pause(); }; stopWav.onClick = [this] { audioEngine.player().stop(); };
    loopWav.onClick = [this] { audioEngine.player().setLooping(loopWav.getToggleState()); };
    wavBrowser.onFileSelected = [this](const juce::File& file) { loadAndPlayWav(file); };
    namEnabled.setToggleState(true, juce::dontSendNotification); irEnabled.setToggleState(true, juce::dontSendNotification);
    namEnabled.onClick = [this] { currentSound.namBypassed = !namEnabled.getToggleState(); audioEngine.setNamBypassed(currentSound.namBypassed); };
    irEnabled.onClick = [this] { currentSound.irBypassed = !irEnabled.getToggleState(); audioEngine.setIrBypassed(currentSound.irBypassed); };
    record.onClick = [this] { if (audioEngine.isRecording()) { audioEngine.stopRecording(); record.setButtonText("REC"); record.setIcon(nam::ui::Icon::record); status.setText(uiText("Aufnahme beendet"), juce::dontSendNotification); } else chooseRecordingTarget(); };
    wahEnabled.onClick = [this] { audioEngine.setAutoWahBypassed(!wahEnabled.getToggleState()); currentSound.autoWahBypassed = !wahEnabled.getToggleState(); };
    chorusEnabled.onClick = [this] { audioEngine.setChorusBypassed(!chorusEnabled.getToggleState()); currentSound.chorusBypassed = !chorusEnabled.getToggleState(); };
    delayEnabled.onClick = [this] { audioEngine.setDelayBypassed(!delayEnabled.getToggleState()); currentSound.delayBypassed = !delayEnabled.getToggleState(); };
    reverbEnabled.onClick = [this] { audioEngine.setReverbBypassed(!reverbEnabled.getToggleState()); currentSound.reverbBypassed = !reverbEnabled.getToggleState(); };
    diagnosticsToggle.onClick = [this]
    {
        diagnosticsPanel.setVisible(diagnosticsToggle.getToggleState());
        if (diagnosticsPanel.isVisible())
            diagnosticsPanel.toFront(false);
        resized();
    };
    librariesToggle.onClick = [this]
    {
        const auto visible = librariesToggle.getToggleState();
        namBrowser.setVisible(visible); irBrowser.setVisible(visible);
        if (visible) { namBrowser.toFront(false); irBrowser.toFront(false); }
        resized();
    };
    auto configureLibraryBrowser = [this](nam::ui::LibraryBrowser& browser, LibraryManager& library, bool nam)
    {
        nam::ui::LibraryBrowser::Callbacks callbacks;
        callbacks.onSelect = [this, &library, nam](const LibraryItem& item)
        {
            if (nam) { currentSound.namId = item.id.toString(); currentSound.ampId = "user-models"; currentSound.configurationId = item.displayName; audioEngine.loadNamAsync(item.linkedFile); }
            else { currentSound.irId = item.id.toString(); audioEngine.loadIr(item.linkedFile); }
        };
        callbacks.onImport = [this, &library, nam](const juce::File& file)
        {
            for (const auto& item : library.items())
                if (item.linkedFile == file)
                {
                    if (nam) { currentSound.namId = item.id.toString(); currentSound.ampId = "user-models"; currentSound.configurationId = item.displayName; audioEngine.loadNamAsync(file); }
                    else { currentSound.irId = item.id.toString(); audioEngine.loadIr(file); }
                    break;
                }
            auto& settings = audioEngine.appSettings();
            if (nam) settings.saveNamLibrary(namLibrary.toValueTree()); else settings.saveIrLibrary(irLibrary.toValueTree());
            settings.flush();
        };
        callbacks.onRelink = [this, nam](const LibraryItem& item, const juce::File& file)
        {
            if (nam && currentSound.namId == item.id.toString()) audioEngine.loadNamAsync(file);
            if (!nam && currentSound.irId == item.id.toString()) audioEngine.loadIr(file);
        };
        callbacks.onRename = [this, nam](const LibraryItem&, const juce::String&)
        { if (nam) audioEngine.appSettings().saveNamLibrary(namLibrary.toValueTree()); else audioEngine.appSettings().saveIrLibrary(irLibrary.toValueTree()); };
        callbacks.onTagsEdited = [this, nam](const LibraryItem&, const juce::StringArray&)
        {
            if (nam) audioEngine.appSettings().saveNamLibrary(namLibrary.toValueTree());
            else audioEngine.appSettings().saveIrLibrary(irLibrary.toValueTree());
        };
        callbacks.onDelete = [this, nam](const LibraryItem&)
        {
            if (nam) audioEngine.appSettings().saveNamLibrary(namLibrary.toValueTree());
            else audioEngine.appSettings().saveIrLibrary(irLibrary.toValueTree());
        };
        callbacks.onError = [this](const juce::String& error) { status.setText(error, juce::dontSendNotification); };
        callbacks.onImportFiles = [this, nam](const juce::Array<juce::File>& files) { importModels(files, nam); };
        callbacks.extendImportMenu = [this](juce::PopupMenu& menu) { addModelFolderItems(menu); };
        browser.setCallbacks(std::move(callbacks));
        browser.refresh();
    };
    configureLibraryBrowser(namBrowser, namLibrary, true);
    configureLibraryBrowser(irBrowser, irLibrary, false);
    savePreset.onClick = [this]
    {
        const auto name = "Preset " + juce::Time::getCurrentTime().formatted("%H:%M:%S");
        if (const auto id = presetManager.create(name, captureSoundState()); id.has_value())
        {
            setLoadedPreset(*id);
            persistPresetState();
            presetBrowser.setSelectedPreset(*id);
            status.setText("Preset gespeichert: " + name, juce::dontSendNotification);
        }
    };
    toggleAB.onClick = [this]
    {
        switchAB(! abState.usingA);
    };
    configurePresetBrowser();
    configureCuratedSelectors();
    const auto savedRack = audioEngine.appSettings().loadUiState().getChildWithName("vst3Rack");
    if (const auto lastPreset = audioEngine.appSettings().loadLastPreset())
        if (const auto* preset = presetManager.find(*lastPreset))
        {
            setLoadedPreset(*lastPreset);
            presetBrowser.setSelectedPreset(*lastPreset);
            applySoundState(preset->sound);
        }
        else
            audioEngine.appSettings().clearLastPreset();
    pluginCatalog = PluginCatalog::fromUiState(audioEngine.appSettings().loadUiState());
    if (savedRack.isValid())
    {
        // Settings from before the editable chain: the pedal, the known plugins
        // and the two VST3 slots, which become blocks of the chain.
        if (const auto pedal = savedRack.getChildWithName("namPedal"); pedal.isValid())
            pedalPanel.restoreSaved(pedal);
        for (const auto& entry : savedRack.getChildWithName("knownVst3"))
            if (const auto xml = entry.createXml())
            {
                juce::PluginDescription description;
                if (description.loadFromXml(*xml))
                    pluginCatalog.plugins.add(description);
            }
        auto layout = currentSound.layout;
        const auto addSlot = [&layout](const juce::ValueTree& state, int position)
        {
            if (! state.hasProperty("descriptionXml"))
                return;
            chain::Node node;
            node.kind = "vst3";
            node.plugin = state.createCopy();
            node.bypassed = state.getProperty("bypassed", false);
            if (const auto xml = juce::parseXML(state.getProperty("descriptionXml").toString()))
                node.name = xml->getStringAttribute("name");
            layout.add(node, position);
        };
        addSlot(savedRack.getChildWithName("pre").getChild(0), layout.serialIndex("pedal"));
        addSlot(savedRack.getChildWithName("post").getChild(0), static_cast<int>(layout.serial.size()));
        applyLayout(layout);
    }
    if (!audioEngine.appSettings().loadLastPreset())
        if (const auto saved=audioEngine.appSettings().loadUiState().getChildWithName("lastRigSound").getChild(0); saved.isValid())
            if (auto sound=SoundState::fromValueTree(saved); sound && (sound->rigId.isEmpty() || rigLibrary.find(sound->rigId)))
                applySoundState(*sound);
    configureArrangement();
    setUpCommands();
    wahMouseTimer.startTimerHz(60);
    startTimerHz(4);
    audioSummaryLabel.setText(audioSummary(), juce::dontSendNotification);
    setSize(1280, 800);
    if (pluginCatalog.plugins.isEmpty())
        scanPlugins({}, {});
}

MainComponent::~MainComponent()
{
    stopTimer();
    wahMouseTimer.stopTimer();
    shortcutWindow.reset();
    commandManager.getKeyMappings()->removeChangeListener(this);
    removeKeyListener(commandManager.getKeyMappings());
    instrumentEditors.clear();
    pluginScanPool.removeAllJobs(true, 5000);
    pedalWindow.reset();
    auto& settings = audioEngine.appSettings();
    auto uiState = settings.loadUiState();
    if (!uiState.isValid()) uiState=juce::ValueTree("uiState");
    uiState.removeChild(uiState.getChildWithName("vst3Rack"),nullptr); // migrated into the chain
    arrangementView.autosave();
    uiState.setProperty("modelFolder", modelFolderRoot.getFullPathName(), nullptr);
    saveKeyMappings(uiState);
    uiState.removeChild(uiState.getChildWithName("arrangementView"),nullptr);
    auto arrangementState = arrangementView.saveState();
    arrangementState.setProperty("visible", arrangementVisible, nullptr);
    uiState.addChild(arrangementState,-1,nullptr);
    uiState.removeChild(uiState.getChildWithName("lastRigSound"),nullptr);
    {
        // The live sound, chain and plugins included, comes back on the next start.
        juce::ValueTree lastRig("lastRigSound");
        lastRig.addChild(captureSoundState().toValueTree(),-1,nullptr);
        uiState.addChild(lastRig,-1,nullptr);
    }
    pluginEditors.clear(); // plugin windows close before the plugins go away with the engine
    settings.saveUiState(uiState);
    settings.saveNamLibrary(namLibrary.toValueTree());
    settings.saveIrLibrary(irLibrary.toValueTree());
    settings.savePresetLibrary(presetManager.toValueTree());
    settings.flush();
    setLookAndFeel(nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void MainComponent::configurePresetBrowser()
{
    presetBrowser.setPresetManager(&presetManager);
    presetBrowser.onSave = [this] (const juce::Uuid& id)
    {
        if (! presetManager.replace(id, captureSoundState()))
            return;
        setLoadedPreset(id);
        persistPresetState();
        presetBrowser.refresh();
        status.setText("Preset aktualisiert", juce::dontSendNotification);
    };
    presetBrowser.onSaveAs = [this] (juce::String name)
    {
        if (const auto id = presetManager.create(std::move(name), captureSoundState()))
        {
            setLoadedPreset(*id);
            persistPresetState();
            presetBrowser.setSelectedPreset(*id);
            if (const auto* preset = presetManager.find(*id))
                status.setText("Preset gespeichert: " + preset->name, juce::dontSendNotification);
        }
    };
    presetBrowser.onLoad = [this] (const juce::Uuid& id)
    {
        if (const auto* preset = presetManager.find(id))
        {
            applySoundState(preset->sound);
            setLoadedPreset(id);
            persistPresetState();
            status.setText("Preset geladen: " + preset->name, juce::dontSendNotification);
        }
    };
    presetBrowser.onRename = [this] (const juce::Uuid& id, juce::String name)
    {
        if (presetManager.rename(id, std::move(name)))
        {
            persistPresetState();
            presetBrowser.refresh();
            status.setText("Preset umbenannt", juce::dontSendNotification);
        }
    };
    presetBrowser.onDelete = [this] (const juce::Uuid& id)
    {
        const bool wasLoaded = loadedPreset == id;
        if (! presetManager.erase(id))
            return;
        if (wasLoaded)
        {
            loadedPreset = {};
            audioEngine.appSettings().clearLastPreset();
        }
        persistPresetState();
        presetBrowser.setSelectedPreset({});
        status.setText(uiText("Preset gelöscht"), juce::dontSendNotification);
    };
    presetBrowser.onABSelectionChanged = [this] (bool useA) { switchAB(useA); };
    presetBrowser.refresh();
}

void MainComponent::persistPresetState()
{
    auto& settings = audioEngine.appSettings();
    settings.savePresetLibrary(presetManager.toValueTree());
    settings.flush();
}

void MainComponent::setLoadedPreset(const juce::Uuid& id)
{
    loadedPreset = id;
    if (id.isNull())
        audioEngine.appSettings().clearLastPreset();
    else
        audioEngine.appSettings().saveLastPreset(id);
}

void MainComponent::switchAB(bool useA)
{
    if (! abInitialised)
    {
        abState.a = captureSoundState();
        abState.b = abState.a;
        abState.usingA = true;
        abInitialised = true;
    }

    if (useA == abState.usingA)
        return;

    if (abState.usingA)
    {
        abState.a = captureSoundState();
        applySoundState(abState.b);
    }
    else
    {
        abState.b = captureSoundState();
        applySoundState(abState.a);
    }

    abState.usingA = useA;
    toggleAB.setButtonText(useA ? "A" : "B");
    presetBrowser.setUsingA(useA);
}

void MainComponent::timerCallback()
{
    const bool wav = audioEngine.getSourceMode() == AudioEngine::SourceMode::wav;
    liveMode.setToggleState(!wav, juce::dontSendNotification);
    wavMode.setToggleState(wav, juce::dontSendNotification);
    wavBrowser.setVisible(wav && view == View::rig);
    graphicEqButton.setToggleState(! currentSound.graphicEqBypassed, juce::dontSendNotification);
    pitchButton.setToggleState(! currentSound.pitchBypassed, juce::dontSendNotification);
    pitchButton.setButtonText(currentSound.pitchBypassed ? juce::String("PITCH") : "PITCH " + juce::String(currentSound.pitchSemitones > 0 ? "+" : "") + juce::String(currentSound.pitchSemitones, 0));
    playWav.setEnabled(wav); pauseWav.setEnabled(wav); stopWav.setEnabled(wav);
    loopWav.setEnabled(wav); wavTimeline.setEnabled(wav);
    const auto latencyMs = audioEngine.estimatedLatencyMilliseconds();
    const auto performance = audioEngine.performanceSnapshot();
    if (! wavTimeline.isMouseButtonDown())
        wavTimeline.setValue(audioEngine.player().positionNormalized(), juce::dontSendNotification);
    latency.setText(latencyMs >= 0.0
        ? uiText("Latenz ") + juce::String(latencyMs, 1) + uiText(" ms · Last ") + juce::String(performance.realtimeLoad * 100.0, 0) + uiText(" % · Aussetzer ") + juce::String(performance.xruns)
        : uiText("Kein Audiogerät aktiv"), juce::dontSendNotification);
    updateChainStrip();
    audioSummaryLabel.setText(audioSummary(), juce::dontSendNotification);
    audioSummaryLabel.setColour(juce::Label::textColourId,
                                audioEngine.deviceManager().getCurrentAudioDevice() == nullptr ? Theme::amber() : Theme::subduedText());
    // The hint names the rig's input, which re-amping replaces during playback.
    const auto* reamp = arrangementController.state().reampTrack();
    liveHint.setText(reamp != nullptr
        ? uiText("Rig-Eingang: Re-Amp „") + reamp->name + uiText("“ – bei der Wiedergabe spielt diese Spur statt der Gitarre durch das Rig; eine scharfgeschaltete Rig-Spur nimmt das Ergebnis auf. Ausschalten: Rechtsklick auf die Spur oder RE-AMP im Spurkopf.")
        : uiText("Live: Die Gitarre spielt durch das Rig. Für Testdateien auf WAV umschalten, für Mehrspuraufnahmen ins Arrangement (Tab)."),
        juce::dontSendNotification);
    liveHint.setColour(juce::Label::textColourId, reamp != nullptr ? Theme::amber() : Theme::subduedText());
    // LIVE shows a hint instead of the WAV player controls.
    const auto showWav = wav && view == View::rig;
    if (playWav.isVisible() != showWav)
        resized();
}

juce::String MainComponent::audioSummary() const
{
    auto* device = audioEngine.deviceManager().getCurrentAudioDevice();
    if (device == nullptr)
        return uiText("Kein Audiogerät – hier klicken zum Einrichten");
    const auto rate = device->getCurrentSampleRate();
    const auto buffer = device->getCurrentBufferSizeSamples();
    return device->getTypeName() + uiText(" · ") + device->getName() + uiText(" · ") + juce::String(rate / 1000.0, 1) + uiText(" kHz · ")
         + juce::String(buffer) + uiText(" Samples (") + juce::String(rate > 0.0 ? 1000.0 * buffer / rate : 0.0, 1) + " ms)";
}

void MainComponent::showAudioSettings()
{
    if (audioSettingsWindow != nullptr)
    {
        audioEngine.diagnosticLog().log(Logger::Level::info, "[UI] audio settings shown again");
        audioSettingsWindow->setVisible(true);
        audioSettingsWindow->toFront(true);
        return;
    }

    audioEngine.diagnosticLog().log(Logger::Level::info, "[UI] audio settings opened");
    audioSettingsWindow.reset(new AudioSettingsDialog(audioEngine.deviceManager(), audioEngine.diagnosticLog().file(), [this]
    {
        audioSettingsWindow->setVisible(false);
        audioSummaryLabel.setText(audioSummary(), juce::dontSendNotification);
    }));
    audioSettingsWindow->centreWithSize(700, 560);
    audioSettingsWindow->setVisible(true);
}

SoundState MainComponent::captureSoundState(bool includePlugins)
{
    auto state = currentSound;
    pedalPanel.capture(state);
    state.inputGainDb = static_cast<float>(inputGain.getValue()); state.bassDb = static_cast<float>(bass.getValue()); state.midDb = static_cast<float>(mid.getValue()); state.trebleDb = static_cast<float>(treble.getValue()); state.outputGainDb = static_cast<float>(outputGain.getValue());
    state.autoWahBypassed = !wahEnabled.getToggleState(); state.chorusBypassed = !chorusEnabled.getToggleState(); state.delayBypassed = !delayEnabled.getToggleState(); state.reverbBypassed = !reverbEnabled.getToggleState();
    state.namBypassed = !namEnabled.getToggleState(); state.irBypassed = !irEnabled.getToggleState();
    if (includePlugins)
        state.layout = audioEngine.rigChain().captureLayout(currentSound.layout);
    return state;
}

void MainComponent::applySoundState(const SoundState& state)
{
    currentSound = state;
    closePluginEditors(true); // plugins may be replaced
    audioEngine.rigChain().setLayout(state.layout);
    pedalPanel.restore(state);
    if (const auto* rig=rigLibrary.find(state.rigId))
    {
        for (int n=0;n<rigLibrary.rigs().size();++n)
            if(rigLibrary.rigs()[n].id==rig->id) rigSelector.setSelectedId(n+1,juce::sendNotificationSync);
        ampSelector.setSelectedId(0,juce::dontSendNotification);
        ampSelector.setText(rig->ampFile.getFileNameWithoutExtension(),juce::dontSendNotification);
        configurationSelector.setSelectedId(0,juce::dontSendNotification);
        configurationSelector.setText("Rig-Vorlage: "+rig->name,juce::dontSendNotification);
    }
    if (const auto* model = curatedAmpLibrary.find(state.ampId, state.configurationId))
    {
        const auto ampIndex = curatedAmpIds.indexOf(model->ampId);
        if (ampIndex >= 0)
        {
            ampSelector.setSelectedId(ampIndex + 1, juce::dontSendNotification);
            configurationSelector.clear(juce::dontSendNotification);
            curatedConfigurationIds.clear();
            for (const auto& candidate : curatedAmpLibrary.models())
                if (candidate.ampId == model->ampId)
                {
                    curatedConfigurationIds.add(candidate.configurationId);
                    configurationSelector.addItem(candidate.configurationName, curatedConfigurationIds.size());
                }
            const auto configIndex = curatedConfigurationIds.indexOf(model->configurationId);
            if (configIndex >= 0)
                configurationSelector.setSelectedId(configIndex + 1, juce::dontSendNotification);
        }
    }
    effectParameters.setState(state);
    inputGain.setValue(state.inputGainDb, juce::dontSendNotification); bass.setValue(state.bassDb, juce::dontSendNotification); mid.setValue(state.midDb, juce::dontSendNotification); treble.setValue(state.trebleDb, juce::dontSendNotification); outputGain.setValue(state.outputGainDb, juce::dontSendNotification);
    audioEngine.applyEffectState(state);
    namEnabled.setToggleState(!state.namBypassed, juce::dontSendNotification); irEnabled.setToggleState(!state.irBypassed, juce::dontSendNotification);
    wahEnabled.setToggleState(!state.autoWahBypassed, juce::dontSendNotification); chorusEnabled.setToggleState(!state.chorusBypassed, juce::dontSendNotification); delayEnabled.setToggleState(!state.delayBypassed, juce::dontSendNotification); reverbEnabled.setToggleState(!state.reverbBypassed, juce::dontSendNotification);
    if (const auto id = juce::Uuid(state.namId); !id.isNull()) if (const auto* item = namLibrary.find(id); item != nullptr && !item->isMissing()) audioEngine.loadNamAsync(item->linkedFile);
    if (const auto id = juce::Uuid(state.irId); !id.isNull()) if (const auto* item = irLibrary.find(id); item != nullptr && !item->isMissing()) audioEngine.loadIr(item->linkedFile);
}

void MainComponent::configureMainControls()
{
    const auto setup = [](nam::ui::RotaryParameterControl& c) { c.setRange(-24.0, 24.0, 0.1); c.setValue(0.0, juce::dontSendNotification); c.setValueFormatter([](double v) { return juce::String(v, 1) + " dB"; }); };
    setup(inputGain); setup(bass); setup(mid); setup(treble); setup(outputGain);
    inputGain.onValueChange = [this](double v) { audioEngine.setInputGainDb((float) v); }; bass.onValueChange = [this](double v) { audioEngine.setBassDb((float) v); }; mid.onValueChange = [this](double v) { audioEngine.setMidDb((float) v); }; treble.onValueChange = [this](double v) { audioEngine.setTrebleDb((float) v); }; outputGain.onValueChange = [this](double v) { audioEngine.setOutputGainDb((float) v); };
}

juce::File MainComponent::drumKitsDirectory() const
{
    // resources/drums next to resources/defaults.
    return defaultAssetsDirectory().getParentDirectory().getChildFile("drums");
}

juce::File MainComponent::defaultAssetsDirectory() const
{
    juce::Array<juce::File> candidates;
    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    candidates.add(executable.getSiblingFile("resources").getChildFile("defaults"));
    candidates.add(executable.getParentDirectory().getParentDirectory()
                       .getChildFile("share").getChildFile("NamTestProgram")
                       .getChildFile("resources").getChildFile("defaults"));
    candidates.add(juce::File::getCurrentWorkingDirectory().getChildFile("resources").getChildFile("defaults"));
    for (const auto& candidate : candidates)
        if (candidate.isDirectory())
            return candidate;
    return {};
}

void MainComponent::configureCuratedSelectors()
{
    ampSelector.clear(juce::dontSendNotification);
    curatedAmpIds.clear();
    juce::StringArray seen;
    for (const auto& model : curatedAmpLibrary.models())
    {
        if (seen.contains(model.ampId))
            continue;
        seen.add(model.ampId);
        curatedAmpIds.add(model.ampId);
        ampSelector.addItem(model.manufacturer + " " + model.ampName, curatedAmpIds.size());
    }

    ampSelector.onChange = [this]
    {
        configurationSelector.clear(juce::dontSendNotification);
        curatedConfigurationIds.clear();
        const auto ampIndex = ampSelector.getSelectedId() - 1;
        if (! juce::isPositiveAndBelow(ampIndex, curatedAmpIds.size()))
            return;
        const auto ampId = curatedAmpIds[ampIndex];
        for (const auto& model : curatedAmpLibrary.models())
            if (model.ampId == ampId)
            {
                curatedConfigurationIds.add(model.configurationId);
                configurationSelector.addItem(model.configurationName, curatedConfigurationIds.size());
            }
        if (! curatedConfigurationIds.isEmpty())
            configurationSelector.setSelectedId(1, juce::sendNotificationSync);
    };
    configurationSelector.onChange = [this]
    {
        const auto ampIndex = ampSelector.getSelectedId() - 1;
        const auto configIndex = configurationSelector.getSelectedId() - 1;
        if (! juce::isPositiveAndBelow(ampIndex, curatedAmpIds.size())
             || ! juce::isPositiveAndBelow(configIndex, curatedConfigurationIds.size()))
            return;
        if (const auto* model = curatedAmpLibrary.find(curatedAmpIds[ampIndex], curatedConfigurationIds[configIndex]))
            applyCuratedModel(*model);
    };

    if (! curatedAmpIds.isEmpty())
        ampSelector.setSelectedId(1, juce::sendNotificationSync);
}

void MainComponent::configureRigs()
{
    addAndMakeVisible(rigSelector); addAndMakeVisible(loadRigButton); addAndMakeVisible(rigInfo); addAndMakeVisible(rigDetailsButton);
    rigSelector.setTextWhenNothingSelected(uiText("Komplette Rigs / Vorlage auswählen"));
    rigSelector.setTooltip(uiText("Lädt Pedal, Amp und Cabinet gemeinsam. Alle zusätzlichen Effekte werden ausgeschaltet."));
    juce::String error;
    const auto resources=defaultAssetsDirectory().getParentDirectory();
    if (rigLibrary.load(resources.getChildFile("rig-models/catalog.json"),resources,&error))
        for (const auto& rig : rigLibrary.rigs()) rigSelector.addItem(rig.name,rigSelector.getNumItems()+1);
    else rigInfo.setText(uiText("Keine Rig-Vorlagen gefunden (resources/rig-models fehlt)."),juce::dontSendNotification);
    rigSelector.onChange=[this]
    {
        const int index=rigSelector.getSelectedId()-1;
        if (!juce::isPositiveAndBelow(index,rigLibrary.rigs().size())) return;
        const auto& rig=rigLibrary.rigs()[index];
        const auto route=rig.routing=="full-rig" ? "Amp + Cab im Capture" : rig.routing=="amp-cab" ? "Amp > Cabinet" : "Pedal > Amp > Cabinet";
        rigInfo.setText(juce::String(route)+" | Zusatz-FX beim Laden AUS",juce::dontSendNotification);
        rigInfo.setTooltip(rig.description+"\n"+rig.provenance+"\nIm Capture enthalten: "+(rig.embeddedEffects.isEmpty()?"Keine weiteren Effekte angegeben":rig.embeddedEffects.joinIntoString(", ")));
    };
    loadRigButton.onClick=[this]{loadSelectedRig();};
    rigDetailsButton.onClick=[this]
    {
        const int index=rigSelector.getSelectedId()-1;
        if (!juce::isPositiveAndBelow(index,rigLibrary.rigs().size())) return;
        const auto& rig=rigLibrary.rigs()[index];
        const auto parts="Pedal: "+(rig.pedalFile==juce::File{} ? juce::String("im Capture / kein separater Slot") : rig.pedalFile.getFileNameWithoutExtension())
            +"\nAmp: "+rig.ampFile.getFileNameWithoutExtension()
            +"\nCabinet: "+(rig.cabFile==juce::File{} ? juce::String("im Capture") : rig.cabFile.getFileNameWithoutExtension())
            +"\n\n"+rig.description+"\n\nIm Capture enthaltene Effekte: "+(rig.embeddedEffects.isEmpty()?"keine weiteren angegeben":rig.embeddedEffects.joinIntoString(", "))
            +"\n\nOptionale Effekte: beim Laden alle AUS. Danach frei zuschaltbar.";
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Bestandteile: "+rig.name,parts);
    };
    loadRigButton.setEnabled(!rigLibrary.rigs().isEmpty());
    rigDetailsButton.setEnabled(!rigLibrary.rigs().isEmpty());
    if (!rigLibrary.rigs().isEmpty()) rigSelector.setSelectedId(1,juce::sendNotificationSync);
}

void MainComponent::loadSelectedRig()
{
    const int index=rigSelector.getSelectedId()-1;
    if (!juce::isPositiveAndBelow(index,rigLibrary.rigs().size())) return;
    const auto& rig=rigLibrary.rigs()[index];
    // Validate again: files may have moved after the catalog was read.
    for (const auto& file : {rig.ampFile,rig.pedalFile,rig.cabFile})
        if (file!=juce::File{} && !file.existsAsFile())
        {status.setText("Rig-Datei fehlt: "+file.getFullPathName(),juce::dontSendNotification); return;}
    auto sound=rigLibrary.makeSound(rig,captureSoundState());
    const auto registerFile=[](LibraryManager& library,const juce::File& file)->juce::String
    {
        if(file==juce::File{}) return {};
        for(const auto& item:library.items()) if(item.linkedFile==file) return item.id.toString();
        if(auto id=library.importLink(file,{"Rig"})) return id->toString();
        return {};
    };
    sound.namId=registerFile(namLibrary,rig.ampFile);
    sound.irId=registerFile(irLibrary,rig.cabFile);
    if(sound.namId.isEmpty() || (rig.cabFile!=juce::File{} && sound.irId.isEmpty()))
    {status.setText("Rig konnte nicht registriert werden.",juce::dontSendNotification); return;}
    sound.rigId=rig.id;
    applySoundState(sound);
    loadedPreset=juce::Uuid::null();
    presetBrowser.setSelectedPreset(juce::Uuid::null());
    audioEngine.appSettings().clearLastPreset();
    status.setText("Rig geladen: "+rig.name+" | Zusatz-Effekte AUS",juce::dontSendNotification);
}

void MainComponent::applyCuratedModel(const CuratedAmpModel& model)
{
    const auto file = defaultAssetsDirectory().getChildFile(model.namFile);
    if (! file.existsAsFile())
        return;

    currentSound = captureSoundState();

    currentSound.ampId = model.ampId;
    currentSound.rigId.clear();
    currentSound.configurationId = model.configurationId;
    for (const auto& item : namLibrary.items())
        if (item.linkedFile == file)
        {
            currentSound.namId = item.id.toString();
            break;
        }

    currentSound.namBypassed = false;
    if (model.captureType == "FullRig")
    {
        currentSound.irBypassed = true;
    }
    else if (model.recommendedIrFile.isNotEmpty())
    {
        const auto irFile = defaultAssetsDirectory().getChildFile(model.recommendedIrFile);
        for (const auto& item : irLibrary.items())
            if (item.linkedFile == irFile)
            {
                currentSound.irId = item.id.toString();
                currentSound.irBypassed = false;
                break;
            }
    }

    audioEngine.loadNamAsync(file);
    applySoundState(currentSound);
    status.setText(model.ampName + uiText(" · ") + model.configurationName, juce::dontSendNotification);
}

void MainComponent::registerBundledDefaults()
{
    const auto defaultsDirectory = defaultAssetsDirectory();
    if (! defaultsDirectory.isDirectory())
        return;

    bool namChanged = false;
    for (const auto& model : curatedAmpLibrary.models())
    {
        const auto file = defaultsDirectory.getChildFile(model.namFile);
        if (! file.existsAsFile())
            continue;
        juce::Array<juce::Uuid> matches;
        for (const auto& item : namLibrary.items())
            if (item.linkedFile == file || item.metadataJson.contains(model.namFile))
                matches.add(item.id);
        if (! matches.isEmpty())
        {
            const auto keep = matches.getFirst();
            namLibrary.relink(keep, file);
            namLibrary.setMetadataJson(keep, model.metadataJson);
            namLibrary.setTags(keep, model.tags);
            for (int i = 1; i < matches.size(); ++i)
                namLibrary.erase(matches.getReference(i));
            namChanged = true;
        }
        else
        {
            juce::String error;
            if (namLibrary.importLink(file, model.tags, model.metadataJson, &error))
                namChanged = true;
        }
    }
    for (const auto& iterator : juce::RangedDirectoryIterator(defaultsDirectory, false, "*.nam", juce::File::findFiles))
    {
        const auto file = iterator.getFile();
        bool alreadyLinked = false;
        for (const auto& item : namLibrary.items())
            if (item.linkedFile == file) { alreadyLinked = true; break; }
        if (alreadyLinked)
            continue;

        juce::StringArray tags;
        const auto name = file.getFileNameWithoutExtension().toLowerCase();
        if (name.contains("clean")) tags.add("Clean");
        if (name.contains("crunch")) tags.add("Crunch");
        if (name.contains("6505") || name.contains("high")) tags.add("High Gain");
        if (tags.isEmpty()) tags.add("Other");
        juce::String error;
        if (namLibrary.importLink(file, tags, "{\"source\":\"pelennor2170/NAM_models\",\"default\":true}", &error))
            namChanged = true;
    }
    bool irChanged = false;
    for (const auto& iterator : juce::RangedDirectoryIterator(defaultsDirectory, false, "*.wav", juce::File::findFiles))
    {
        const auto file = iterator.getFile();
        juce::StringArray tags;
        const auto name = file.getFileNameWithoutExtension().toLowerCase();
        const auto fromTone3000 = name.startsWith("t3k ");
        if (name.contains("live room"))
            tags.add("Room");
        else
        {
            tags.add("4x12");
            tags.add(name.contains("emerald") ? "Open Back" : "Closed Back");
        }
        if (fromTone3000)
            tags.add("TONE3000");
        const juce::String source = fromTone3000 ? "TONE3000" : "Jester Dyne Productions";
        juce::String error;
        juce::Array<juce::Uuid> matches;
        for (const auto& item : irLibrary.items())
            if (item.linkedFile == file || item.displayName == file.getFileNameWithoutExtension())
                matches.add(item.id);
        if (! matches.isEmpty())
        {
            const auto keep = matches.getFirst();
            irLibrary.relink(keep, file);
            irLibrary.setMetadataJson(keep, "{\"source\":\"" + source + "\",\"default\":true,\"file\":\"" + file.getFileName() + "\"}");
            irLibrary.setTags(keep, tags);
            for (int i = 1; i < matches.size(); ++i)
                irLibrary.erase(matches.getReference(i));
            irChanged = true;
        }
        else if (irLibrary.importLink(file, tags, "{\"source\":\"" + source + "\",\"default\":true,\"file\":\"" + file.getFileName() + "\"}", &error))
            irChanged = true;
    }
    if (namChanged)
        audioEngine.appSettings().saveNamLibrary(namLibrary.toValueTree());
    if (irChanged)
        audioEngine.appSettings().saveIrLibrary(irLibrary.toValueTree());
}

void MainComponent::chooseNamFile()
{
    showModelImportMenu(true, selectNam);
}

void MainComponent::chooseIrFile()
{
    showModelImportMenu(false, selectIr);
}

void MainComponent::showModelImportMenu(bool nam, juce::Component& target)
{
    const juce::Component::SafePointer<MainComponent> self(this);
    const auto choose = [self, nam](bool folders)
    {
        if (self == nullptr)
            return;
        self->modelChooser = std::make_shared<juce::FileChooser>(nam ? uiText("NAM-Modelle importieren") : uiText("Cabinet-IRs importieren"),
                                                                 juce::File {}, folders ? juce::String("*") : juce::String(nam ? "*.nam" : "*.wav"));
        const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems
                         | (folders ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles);
        self->modelChooser->launchAsync(flags, [self, nam](const juce::FileChooser& chooser)
        {
            if (self == nullptr)
                return;
            const auto files = chooser.getResults();
            if (! files.isEmpty() && files.getFirst() != juce::File {})
                self->importModels(files, nam);
            self->modelChooser.reset();
        });
    };
    juce::PopupMenu menu;
    menu.addItem(uiText("Dateien importieren … (Mehrfachauswahl)"), [choose] { choose(false); });
    menu.addItem(uiText("Ordner importieren … (inkl. Unterordner)"), [choose] { choose(true); });
    menu.addSeparator();
    addModelFolderItems(menu);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target));
}

void MainComponent::addModelFolderItems(juce::PopupMenu& menu)
{
    menu.addSectionHeader(uiText("Modellordner: ") + modelFolderRoot.getFullPathName());
    menu.addItem(uiText("Modellordner öffnen"), [this]
    {
        modelFolderRoot.createDirectory();
        modelFolderRoot.startAsProcess();
    });
    menu.addItem(uiText("Modellordner ändern …"), [this] { chooseModelFolder(); });
    menu.addItem(uiText("Modellordner neu einlesen"), [this] { syncModelFolder(); });
    menu.addItem(uiText("Verknüpfte Modelle in den Modellordner kopieren"), [this]
    {
        // Bundled defaults stay where the installer put them.
        const auto bundled = defaultAssetsDirectory().getParentDirectory();
        const auto models = modelFolder().adoptExternal(namLibrary, bundled);
        const auto irs = modelFolder().adoptExternal(irLibrary, bundled);
        audioEngine.appSettings().saveNamLibrary(namLibrary.toValueTree());
        audioEngine.appSettings().saveIrLibrary(irLibrary.toValueTree());
        audioEngine.appSettings().flush();
        namBrowser.refresh();
        irBrowser.refresh();
        status.setText(juce::String(models) + uiText(" NAM-Modelle und ") + juce::String(irs) + uiText(" IRs in den Modellordner kopiert."), juce::dontSendNotification);
    });
}

void MainComponent::chooseModelFolder()
{
    modelChooser = std::make_shared<juce::FileChooser>(uiText("Modellordner wählen"), modelFolderRoot, "*");
    const juce::Component::SafePointer<MainComponent> self(this);
    modelChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [self](const juce::FileChooser& chooser)
        {
            if (self == nullptr)
                return;
            const auto folder = chooser.getResult();
            self->modelChooser.reset();
            if (! folder.isDirectory())
                return;
            self->modelFolderRoot = folder;
            auto uiState = self->audioEngine.appSettings().loadUiState();
            if (! uiState.isValid())
                uiState = juce::ValueTree("uiState");
            uiState.setProperty("modelFolder", folder.getFullPathName(), nullptr);
            self->audioEngine.appSettings().saveUiState(uiState);
            self->syncModelFolder();
        });
}

void MainComponent::syncModelFolder()
{
    const auto models = modelFolder().sync(namLibrary);
    const auto irs = modelFolder().sync(irLibrary);
    if (models + irs > 0)
    {
        audioEngine.appSettings().saveNamLibrary(namLibrary.toValueTree());
        audioEngine.appSettings().saveIrLibrary(irLibrary.toValueTree());
        audioEngine.appSettings().flush();
        namBrowser.refresh();
        irBrowser.refresh();
        status.setText(juce::String(models + irs) + uiText(" neue Datei(en) aus dem Modellordner übernommen."), juce::dontSendNotification);
    }
}

void MainComponent::importModels(const juce::Array<juce::File>& selection, bool nam)
{
    auto& library = nam ? static_cast<LibraryManager&>(namLibrary) : static_cast<LibraryManager&>(irLibrary);
    const auto result = modelFolder().importFiles(library, selection);
    auto& settings = audioEngine.appSettings();
    if (nam) settings.saveNamLibrary(namLibrary.toValueTree()); else settings.saveIrLibrary(irLibrary.toValueTree());
    settings.flush();
    (nam ? namBrowser : irBrowser).refresh();

    // The first imported model is loaded right away, as with a single import.
    if (! result.imported.isEmpty())
        if (const auto* item = library.find(result.imported.getFirst()))
        {
            if (nam)
            {
                currentSound.namId = item->id.toString();
                currentSound.ampId = "user-models";
                currentSound.configurationId = item->displayName;
                currentSound.namBypassed = false;
                audioEngine.loadNamAsync(item->linkedFile);
            }
            else
            {
                currentSound.irId = item->id.toString();
                currentSound.irBypassed = false;
                audioEngine.loadIr(item->linkedFile);
            }
            (nam ? namBrowser : irBrowser).setSelectedItem(item->id);
        }

    juce::String summary = juce::String(result.imported.size()) + (nam ? uiText(" NAM-Modell(e)") : uiText(" IR(s)")) + uiText(" importiert");
    if (result.copied > 0)
        summary << ", " << result.copied << uiText(" in den Modellordner kopiert");
    if (result.alreadyPresent > 0)
        summary << ", " << result.alreadyPresent << uiText(" waren schon vorhanden");
    if (! result.failed.isEmpty())
        summary << uiText(" – Fehler: ") << result.failed.joinIntoString("; ");
    if (result.imported.isEmpty() && result.failed.isEmpty())
        summary = nam ? uiText("Keine .nam-Dateien gefunden.") : uiText("Keine .wav-Dateien gefunden.");
    status.setText(summary, juce::dontSendNotification);
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (view == View::arrangement)
        return false; // the arrangement takes audio drops itself
    for (const auto& path : files)
        if (juce::File(path).isDirectory() || juce::File(path).hasFileExtension("nam"))
            return true;
    return false;
}

void MainComponent::filesDropped(const juce::StringArray& paths, int, int)
{
    juce::Array<juce::File> files;
    for (const auto& path : paths)
        files.add(juce::File(path));
    importModels(files, true);
}

void MainComponent::chooseWavFile()
{
    wavChooser = std::make_shared<juce::FileChooser>(uiText("WAV auswählen (Mono oder Stereo)"), juce::File {}, "*.wav"); const juce::Component::SafePointer<MainComponent> self(this);
    wavChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [self](const juce::FileChooser& chooser) { if (self == nullptr) return; const auto file = chooser.getResult(); if (file.existsAsFile()) { self->wavBrowser.setDirectory(file.getParentDirectory()); self->loadAndPlayWav(file); } self->wavChooser.reset(); });
}

void MainComponent::chooseWavDirectory()
{
    wavDirectoryChooser = std::make_shared<juce::FileChooser>(uiText("WAV-Ordner auswählen"), wavBrowser.directory(), "*");
    const juce::Component::SafePointer<MainComponent> self(this);
    wavDirectoryChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [self](const juce::FileChooser& chooser)
        {
            if (self == nullptr) return;
            const auto directory = chooser.getResult();
            if (directory.isDirectory())
            {
                self->wavBrowser.setDirectory(directory);
                self->status.setText("WAV-Ordner: " + directory.getFileName(), juce::dontSendNotification);
            }
            self->wavDirectoryChooser.reset();
        });
}

void MainComponent::loadAndPlayWav(const juce::File& file)
{
    const auto result = audioEngine.loadWav(file);
    if (! result.wasOk())
    {
        status.setText(result.getErrorMessage(), juce::dontSendNotification);
        return;
    }
    audioEngine.setSourceMode(AudioEngine::SourceMode::wav);
    audioEngine.player().play();
    wavBrowser.setVisible(true);
    status.setText("WAV spielt: " + file.getFileName(), juce::dontSendNotification);
}

void MainComponent::chooseRecordingTarget()
{
    recordChooser = std::make_shared<juce::FileChooser>("Verarbeitete WAV-Aufnahme speichern", juce::File {}, "*.wav"); const juce::Component::SafePointer<MainComponent> self(this);
    recordChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [self](const juce::FileChooser& chooser) { if (self == nullptr) return; auto file = chooser.getResult(); if (file.getFileName().isNotEmpty()) { if (!file.hasFileExtension("wav")) file = file.withFileExtension(".wav"); const auto result = self->audioEngine.startRecording(file); if (result.wasOk()) { self->record.setButtonText("STOP"); self->record.setIcon(nam::ui::Icon::stop); self->status.setText(uiText("Aufnahme läuft: ") + file.getFileName(), juce::dontSendNotification); } else self->status.setText(result.getErrorMessage(), juce::dontSendNotification); } self->recordChooser.reset(); });
}

std::vector<juce::Component*> MainComponent::rigComponents()
{
    return { &rigSelector, &loadRigButton, &rigDetailsButton, &rigInfo, &ampSelector, &configurationSelector,
             &inputGain, &bass, &mid, &treble, &outputGain, &inputMeter, &outputMeter, &chainViewport,
             &selectNam, &selectIr, &presetBrowser, &effectParameters, &liveMode, &wavMode,
             &openWav, &chooseWavFolder, &playWav, &pauseWav, &stopWav, &loopWav, &wavTimeline, &record, &liveHint };
}

void MainComponent::configureArrangement()
{
    addChildComponent(arrangementView);
    addAndMakeVisible(viewToggle);
    viewToggle.setTooltip(uiText("Zwischen Live-Rig und Mehrspur-Arrangement wechseln (Tab)"));
    viewToggle.onClick = [this] { setArrangementVisible(! arrangementVisible); };
    addAndMakeVisible(graphicEqButton);
    graphicEqButton.setTooltip(uiText("10-Band Graphic EQ des Live-Rigs"));
    graphicEqButton.onClick = [this] { showGraphicEq(graphicEqButton.getScreenBounds()); };
    addAndMakeVisible(pitchButton);
    pitchButton.setTooltip(uiText("Live-Transponieren mit Rubber Band, z. B. eine Oktave tiefer"));
    pitchButton.onClick = [this] { showPitchShifter(pitchButton.getScreenBounds()); };

    // Main navigation and the signal chain replace the scattered buttons
    // (view toggle, libraries checkbox, GRAPHIC EQ, PITCH, NAM/IR, Pedale/VST3).
    for (auto* tab : { &tabRig, &tabArrangement, &tabLibrary })
    {
        tab->setRadioGroupId(4711);
        tab->setClickingTogglesState(true);
        tab->setConnectedEdges(tab == &tabRig ? juce::Button::ConnectedOnRight
                               : tab == &tabLibrary ? juce::Button::ConnectedOnLeft
                                                    : juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        addAndMakeVisible(*tab);
    }
    tabRig.setToggleState(true, juce::dontSendNotification);
    tabRig.setTooltip(uiText("Live-Rig: Signalkette, Amp, Effekte, Presets (Tab wechselt zum Arrangement)"));
    tabArrangement.setTooltip(uiText("Mehrspur-Arrangement: aufnehmen, schneiden, mischen (Tab)"));
    tabLibrary.setTooltip(uiText("NAM- und IR-Bibliothek: importieren, umbenennen, Tags, fehlende Dateien"));
    tabRig.onClick = [this] { if (tabRig.getToggleState()) setView(View::rig); };
    tabArrangement.onClick = [this] { if (tabArrangement.getToggleState()) setView(View::arrangement); };
    tabLibrary.onClick = [this] { if (tabLibrary.getToggleState()) setView(View::library); };
    for (auto* hidden : std::initializer_list<juce::Component*> { &viewToggle, &librariesToggle, &graphicEqButton, &pitchButton, &namEnabled, &irEnabled })
        hidden->setVisible(false);
    chainStrip.onToggle = [this](const juce::String& id) { toggleChainBlock(id); };
    chainStrip.onOpen = [this](const juce::String& id) { openChainBlock(id); };
    chainStrip.describe = [this](const chain::Node& node) { return describeBlock(node); };
    chainStrip.onLayoutChanged = [this](const chain::Layout& layout) { applyLayout(layout); };
    chainStrip.plugins = [this] { return pluginCatalog.visible(); };
    chainStrip.rescanPlugins = [this](std::function<void()> done) { scanPlugins({}, std::move(done)); };
    chainStrip.managePlugins = [this] { showPluginManager(); };
    chainViewport.setViewedComponent(&chainStrip, false);
    chainViewport.setScrollBarsShown(false, true);
    chainViewport.setScrollBarThickness(8);
    addAndMakeVisible(chainViewport);
    chainStrip.setLayout(currentSound.layout);

    arrangementView.onStatus = [this](const juce::String& text) { status.setText(text, juce::dontSendNotification); };
    arrangementView.createTrackFxEditor = [this](const juce::Uuid& trackId) -> std::unique_ptr<juce::Component>
    {
        return std::make_unique<nam::ui::TrackFxComponent>(arrangementController, trackId, trackFxSources());
    };
    arrangementController.setTrackFxHandlers(
        [] { return std::make_shared<TrackRig>(); },
        [this](daw::TrackProcessor& processor, const juce::ValueTree& settings)
        {
            if (auto* chain = dynamic_cast<RigChain*>(&processor))
                if (const auto sound = SoundState::fromValueTree(settings))
                    applyTrackSound(*chain, *sound);
        });
    arrangementController.setReturnDefaults([](int slot)
    {
        // Returns start 100 % wet: A hall, B delay, C plate, D slapback.
        auto sound = SoundState::defaults();
        sound.namBypassed = true;
        sound.irBypassed = true;
        if (slot == 1 || slot == 3)
        {
            sound.delayBypassed = false;
            sound.delayMix = 1.0f;
            sound.delayTimeMs = slot == 1 ? 375.0f : 110.0f;
            sound.delayFeedback = slot == 1 ? 0.38f : 0.1f;
            sound.delayLowCutHz = 150.0f;
            sound.delayHighCutHz = slot == 1 ? 5000.0f : 7000.0f;
        }
        else
        {
            sound.reverbBypassed = false;
            sound.reverbMix = 1.0f;
            sound.reverbSize = slot == 0 ? 0.75f : 0.55f;
            sound.reverbDecay = slot == 0 ? 2.8f : 1.6f;
            sound.reverbDamping = slot == 0 ? 0.45f : 0.2f;
            sound.reverbPreDelayMs = slot == 0 ? 25.0f : 10.0f;
            sound.reverbLowCutHz = 180.0f;
            sound.reverbHighCutHz = slot == 0 ? 8000.0f : 12000.0f;
        }
        return sound.toValueTree();
    });

    arrangementController.setInstrumentFactory([this](const juce::ValueTree& settings) -> std::shared_ptr<daw::TrackInstrument>
    {
        if (settings.getProperty("type").toString() == "kit")
        {
            juce::String error;
            if (auto kit = daw::SampleKit::load(drumKitsDirectory(), settings.getProperty("kit").toString(), error))
                return kit;
            status.setText(uiText("Drum-Kit nicht verfügbar (") + error + uiText("), internes Synth-Kit wird verwendet."), juce::dontSendNotification);
            return std::make_shared<daw::DrumSynth>();
        }
        if (settings.getProperty("type").toString() != "vst3")
            return std::make_shared<daw::DrumSynth>();
        juce::String error;
        if (auto instrument = PluginInstrument::fromSettings(settings, error))
            return instrument;
        status.setText(uiText("Drum-Instrument nicht verfügbar (") + error + uiText("), internes Kit wird verwendet."), juce::dontSendNotification);
        return std::make_shared<daw::DrumSynth>();
    });
    arrangementView.showInstrumentMenu = [this](const juce::Uuid& trackId, juce::Component& target) { showInstrumentMenu(trackId, target); };
    arrangementView.createSpaceEffect = [this] { return spaceReverbSettings(); };
    arrangementView.createDrumEditor = [this](const juce::Uuid& trackId, int pattern) -> std::unique_ptr<juce::Component>
    {
        nam::ui::DrumMachineComponent::Host host;
        host.instruments = [this] { return pluginCatalog.instruments(); };
        host.rescan = [this](std::function<void()> done) { scanPlugins({}, std::move(done)); };
        host.setSource = [this](const juce::Uuid& id, const juce::PluginDescription* description) { return setInstrumentSource(id, description); };
        host.setKit = [this](const juce::Uuid& id, const juce::String& kitId)
        {
            instrumentEditors.erase(id);
            return arrangementController.setTrackInstrument(id, kitId.isEmpty() ? daw::builtInInstrumentSettings() : daw::SampleKit::settingsFor(kitId));
        };
        host.openEditor = [this](const juce::Uuid& id) { openInstrumentEditor(id); };
        host.openArrangement = [this](const juce::Uuid& id) { arrangementView.openDrumArrangement(id, 0); };
        return std::make_unique<nam::ui::DrumMachineComponent>(arrangementController, trackId, pattern, std::move(host));
    };

    const auto saved = audioEngine.appSettings().loadUiState().getChildWithName("arrangementView");
    arrangementView.restoreState(saved);
    setView(saved.isValid() && static_cast<bool>(saved.getProperty("visible", false)) ? View::arrangement : View::rig);
}

//==============================================================================
void MainComponent::updateChainStrip()
{
    chainStrip.setLayout(currentSound.layout);
}

nam::ui::SignalChainStrip::Block MainComponent::describeBlock(const chain::Node& node)
{
    const auto libraryName = [](LibraryManager& library, const juce::String& id)
    {
        if (const auto* item = id.isNotEmpty() ? library.find(juce::Uuid(id)) : nullptr)
            return item->displayName;
        return juce::String();
    };
    nam::ui::chainblocks::Context context;
    context.ampName = ampSelector.getSelectedId() > 0 ? ampSelector.getText() : libraryName(namLibrary, currentSound.namId);
    if (configurationSelector.getSelectedId() > 0)
        context.ampName << uiText(" · ") << configurationSelector.getText();
    context.irName = libraryName(irLibrary, currentSound.irId);
    context.pedalLoaded = pedalPanel.hasModel();
    context.pedalOn = pedalPanel.isActive();
    context.pedalName = pedalPanel.modelName();
    context.rig = &audioEngine.rigChain();
    return nam::ui::chainblocks::describe(node, currentSound, context);
}

void MainComponent::applyLayout(const chain::Layout& layout)
{
    currentSound.layout = layout;
    closePluginEditors(false); // editors go before their plugins
    audioEngine.rigChain().setLayout(layout);
    if (const auto& errors = audioEngine.rigChain().loadErrors(); ! errors.isEmpty())
        status.setText(uiText("Plugin nicht geladen: ") + errors.joinIntoString(" | "), juce::dontSendNotification);
    effectParameters.setState(currentSound);
    chainStrip.setLayout(layout);
    resized(); // the chain may need a second row
    repaint(); // the panels behind may have moved
}

void MainComponent::closePluginEditors(bool all)
{
    for (auto it = pluginEditors.begin(); it != pluginEditors.end();)
        if (all || currentSound.layout.find(it->first) == nullptr)
            it = pluginEditors.erase(it);
        else
            ++it;
}

void MainComponent::openPluginEditor(const juce::String& blockId)
{
    if (const auto existing = pluginEditors.find(blockId); existing != pluginEditors.end())
    {
        existing->second->setVisible(true);
        existing->second->toFront(true);
        return;
    }
    juce::String error;
    if (auto window = nam::ui::chainblocks::openPluginWindow(audioEngine.rigChain(), blockId, error))
        pluginEditors[blockId] = std::move(window);
    else
        status.setText(error, juce::dontSendNotification);
}

void MainComponent::toggleChainBlock(const juce::String& id)
{
    auto& sound = currentSound;
    if (const auto* node = sound.layout.find(id); node != nullptr && ! node->isBuiltin())
        nam::ui::chainblocks::togglePluginBlock(sound, audioEngine.rigChain(), id);
    else if (id == "pedal")
        pedalPanel.setActive(! pedalPanel.isActive()); // the pedal panel owns the pedal's switch
    else if (nam::ui::chainblocks::toggleBuiltin(sound, id))
    {
        audioEngine.applyEffectState(sound);
        effectParameters.setState(sound);
        namEnabled.setToggleState(! sound.namBypassed, juce::dontSendNotification);
        irEnabled.setToggleState(! sound.irBypassed, juce::dontSendNotification);
    }
    updateChainStrip();
}

void MainComponent::openChainBlock(const juce::String& id)
{
    chainStrip.setSelected(id);
    const auto anchor = chainStrip.screenBoundsOf(id);
    // Effect cards in the panel on the right, in card order.
    const std::pair<const char*, int> cards[] { { "drive", 0 }, { "autoWah", 1 }, { "chorus", 2 }, { "delay", 3 },
                                                { "reverb", 4 }, { "compressor", 5 }, { "wah", 6 } };
    for (const auto& [card, index] : cards)
        if (id == card)
        {
            effectParameters.showCard(index);
            return;
        }
    const auto* node = currentSound.layout.find(id);
    if (node != nullptr && node->kind == "vst3") openPluginEditor(id);
    else if (node != nullptr && node->kind == "transpose") status.setText(uiText("Rechtsklick auf den Block: Halbtöne und Verbindungen"), juce::dontSendNotification);
    else if (id == "pitch") showPitchShifter(anchor);
    else if (id == "geq") showGraphicEq(anchor);
    else if (id == "pedal") showPedal();
    else if (id == "amp" || id == "ampEq") ampSelector.showPopup();
    else if (id == "ir") setView(View::library);
}

void MainComponent::setView(View next)
{
    view = next;
    arrangementVisible = next == View::arrangement;
    const auto rig = next == View::rig;
    for (auto* component : rigComponents())
        component->setVisible(rig);
    wavBrowser.setVisible(rig && audioEngine.getSourceMode() == AudioEngine::SourceMode::wav);
    arrangementView.setVisible(next == View::arrangement);
    namBrowser.setVisible(next == View::library);
    irBrowser.setVisible(next == View::library);
    tabRig.setToggleState(rig, juce::dontSendNotification);
    tabArrangement.setToggleState(next == View::arrangement, juce::dontSendNotification);
    tabLibrary.setToggleState(next == View::library, juce::dontSendNotification);
    resized();
    repaint();
}

void MainComponent::openProject(const juce::File& file)
{
    arrangementView.autosave();
    const auto result = arrangementController.openProject(file);
    status.setText(result.wasOk() ? uiText("Projekt geöffnet: ") + file.getFileNameWithoutExtension() : result.getErrorMessage(), juce::dontSendNotification);
    if (result.wasOk())
        setArrangementVisible(true);
}

//==============================================================================
void MainComponent::setUpCommands()
{
    nam::ui::installGermanShortcutTexts();
    commandManager.registerAllCommandsForTarget(this);
    commandManager.setFirstCommandTarget(this);
    // Only the keys that differ from the defaults are stored.
    if (const auto stored = audioEngine.appSettings().loadUiState().getChildWithName("KEYMAPPINGS"); stored.isValid())
        if (const auto xml = stored.createXml())
            commandManager.getKeyMappings()->restoreFromXml(*xml);
    commandManager.getKeyMappings()->addChangeListener(this);
    addKeyListener(commandManager.getKeyMappings());
    arrangementView.onShowShortcuts = [this] { showShortcuts(); };
}

void MainComponent::saveKeyMappings(juce::ValueTree& uiState)
{
    uiState.removeChild(uiState.getChildWithName("KEYMAPPINGS"), nullptr);
    if (const auto xml = commandManager.getKeyMappings()->createXml(true))
        uiState.addChild(juce::ValueTree::fromXml(*xml), -1, nullptr);
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    // A shortcut was changed in the overview: store it right away.
    auto& settings = audioEngine.appSettings();
    auto uiState = settings.loadUiState();
    if (! uiState.isValid())
        uiState = juce::ValueTree("uiState");
    saveKeyMappings(uiState);
    settings.saveUiState(uiState);
    settings.flush();
}

void MainComponent::showShortcuts()
{
    if (shortcutWindow == nullptr)
        shortcutWindow = std::make_unique<ShortcutWindow>(*commandManager.getKeyMappings());
    shortcutWindow->setVisible(true);
    shortcutWindow->toFront(true);
}

void MainComponent::followMouseWithWah()
{
    // The mouse plays the live rig's wah and those of track chains set to "MAUS".
    const auto live = currentSound.pedalWahMouse && ! currentSound.pedalWahBypassed;
    std::vector<RigChain*> trackChains;
    for (const auto& track : arrangementController.state().tracks)
        if (track.fx != nullptr && track.fx->enabled.load() && static_cast<bool>(track.fx->settings.getProperty("pedalWahMouse", false))
            && ! static_cast<bool>(track.fx->settings.getProperty("pedalWahBypassed", true)))
            if (auto* chain = dynamic_cast<RigChain*>(track.fx->processor.get()))
                trackChains.push_back(chain);
    if (! live && trackChains.empty())
    {
        lastMouseWah = -1.0f;
        return;
    }
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
    const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForPoint(mouse.roundToInt());
    if (display == nullptr)
        return;
    // Top of the screen = toe (bright), bottom = heel (dark); 5 % at both
    // edges are dead zones, so the end positions are easy to reach.
    const auto area = display->userArea.toFloat();
    const auto margin = area.getHeight() * 0.05f;
    const auto position = juce::jlimit(0.0f, 1.0f, (area.getBottom() - margin - mouse.y) / std::max(1.0f, area.getHeight() - 2.0f * margin));
    if (std::abs(position - lastMouseWah) < 0.002f)
        return;
    lastMouseWah = position;
    for (auto* chain : trackChains)
        chain->setWahPosition(position);
    if (! live)
        return;
    currentSound.pedalWahPosition = position;
    audioEngine.setWahPosition(position);
    effectParameters.showWahPosition(position);
}

void MainComponent::toggleMouseWah()
{
    currentSound.pedalWahMouse = ! currentSound.pedalWahMouse;
    if (currentSound.pedalWahMouse)
        currentSound.pedalWahBypassed = false;
    audioEngine.applyEffectState(currentSound);
    effectParameters.setState(currentSound);
    status.setText(currentSound.pedalWahMouse ? uiText("Wah-Pedal per Maus: oben = Toe (hell), unten = Heel (dunkel). W schaltet aus.")
                                              : uiText("Wah-Pedal per Maus aus"),
                   juce::dontSendNotification);
}

void MainComponent::getAllCommands(juce::Array<juce::CommandID>& ids)
{
    for (const auto& definition : nam::ui::commandDefinitions())
        ids.add(definition.id);
}

void MainComponent::getCommandInfo(juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    nam::ui::describeCommand(id, info, arrangementVisible);
}

bool MainComponent::perform(const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case nam::ui::commands::toggleView: setView(view == View::rig ? View::arrangement : View::rig); return true;
        case nam::ui::commands::showShortcuts: showShortcuts(); return true;
        case nam::ui::commands::wahMouse: toggleMouseWah(); return true;
        default: return arrangementVisible && arrangementView.performCommand(info.commandID);
    }
}

void MainComponent::applyGraphicEq(bool bypassed, const std::array<float, 10>& bands)
{
    currentSound.graphicEqBypassed = bypassed;
    currentSound.graphicEqDb = bands;
    audioEngine.setGraphicEqBypassed(bypassed);
    for (int band = 0; band < static_cast<int>(bands.size()); ++band)
        audioEngine.setGraphicEqBandDb(band, bands[static_cast<size_t>(band)]);
    graphicEqButton.setToggleState(! bypassed, juce::dontSendNotification);
}

void MainComponent::showPedal()
{
    if (! pedalWindow)
        pedalWindow = std::make_unique<PedalWindow>(pedalPanel);
    pedalWindow->setVisible(true);
    pedalWindow->toFront(true);
}

void MainComponent::showGraphicEq(juce::Rectangle<int> anchor)
{
    auto editor = std::make_unique<nam::ui::GraphicEqComponent>();
    editor->setValues(currentSound.graphicEqBypassed, currentSound.graphicEqDb);
    editor->onChange = [safe = juce::Component::SafePointer<MainComponent>(this)](bool bypassed, const std::array<float, 10>& bands)
    {
        if (safe != nullptr)
            safe->applyGraphicEq(bypassed, bands);
    };
    editor->setSize(560, 330);
    juce::CallOutBox::launchAsynchronously(std::move(editor), anchor, nullptr);
}

juce::Result MainComponent::setInstrumentSource(const juce::Uuid& trackId, const juce::PluginDescription* description)
{
    instrumentEditors.erase(trackId); // the editor belongs to the old instrument
    const auto* track = arrangementController.state().findTrack(trackId);
    const auto midi = track != nullptr && track->midi;
    // Drum hits go out on the General MIDI percussion channel, MIDI tracks on channel 1.
    const auto result = arrangementController.setTrackInstrument(trackId,
        description != nullptr ? PluginInstrument::settingsFor(*description, {}, midi ? 1 : 10)
                               : midi ? daw::synthInstrumentSettings() : daw::builtInInstrumentSettings());
    if (result.wasOk() && description != nullptr)
        openInstrumentEditor(trackId);
    return result;
}

void MainComponent::openInstrumentEditor(const juce::Uuid& trackId)
{
    const auto* track = arrangementController.state().findTrack(trackId);
    if (track == nullptr)
        return;
    auto instrument = std::dynamic_pointer_cast<PluginInstrument>(track->instrument->instrument);
    if (instrument == nullptr || instrument->instance() == nullptr)
    {
        status.setText(uiText("Die Spur nutzt ein eingebautes Instrument – es hat keinen Plugin-Editor."), juce::dontSendNotification);
        return;
    }
    if (const auto existing = instrumentEditors.find(trackId); existing != instrumentEditors.end())
    {
        existing->second->setVisible(true);
        existing->second->toFront(true);
        return;
    }
    auto* editor = instrument->instance()->createEditorIfNeeded();
    if (editor == nullptr)
    {
        status.setText(instrument->name() + uiText(" hat keine eigene Oberfläche."), juce::dontSendNotification);
        return;
    }
    auto window = std::make_unique<InstrumentEditorWindow>(instrument, editor);
    window->setVisible(true);
    instrumentEditors[trackId] = std::move(window);
}

juce::ValueTree MainComponent::spaceReverbSettings()
{
    // The "Atmo-Hall" return of accompaniments: Valhalla Supermassive if it is
    // installed, else Dragonfly Hall, else the built-in reverb at its largest.
    auto sound = SoundState::defaults();
    sound.namBypassed = true;
    sound.irBypassed = true;
    const auto plugins = pluginCatalog.visible();
    const juce::PluginDescription* choice = nullptr;
    for (const auto* wanted : { "Supermassive", "Dragonfly Hall" })
        for (const auto& description : plugins)
            if (choice == nullptr && description.name.containsIgnoreCase(wanted) && ! description.isInstrument)
                choice = &description;
    if (choice != nullptr)
    {
        Vst3Slot slot;
        if (slot.load(*choice).wasOk())
        {
            // 100 % wet, as a send effect should be.
            if (auto* plugin = slot.instance())
                for (auto* parameter : plugin->getParameters())
                {
                    const auto name = parameter->getName(64).trim().toLowerCase();
                    if (name == "mix" || name == "wet" || name == "dry/wet")
                        parameter->setValueNotifyingHost(1.0f);
                    else if (name.contains("dry"))
                        parameter->setValueNotifyingHost(0.0f);
                }
            const auto state = slot.saveState();
            if (state.isValid() && state.hasProperty("descriptionXml"))
            {
                chain::Node node;
                node.kind = "vst3";
                node.name = choice->name;
                node.plugin = state;
                sound.layout.add(node, static_cast<int>(sound.layout.serial.size()));
                return sound.toValueTree();
            }
        }
    }
    sound.reverbBypassed = false;
    sound.reverbMix = 1.0f;
    sound.reverbSize = 0.95f;
    sound.reverbDecay = 6.0f;
    sound.reverbDamping = 0.35f;
    sound.reverbPreDelayMs = 40.0f;
    sound.reverbLowCutHz = 200.0f;
    sound.reverbHighCutHz = 9000.0f;
    return sound.toValueTree();
}

void MainComponent::showInstrumentMenu(const juce::Uuid& trackId, juce::Component& target)
{
    const auto* track = arrangementController.state().findTrack(trackId);
    if (track == nullptr || ! track->midi)
        return;
    const auto settings = track->instrument->settings;
    const auto isSynth = settings.getProperty("type").toString() == "synth";
    juce::PopupMenu menu;
    menu.addSectionHeader(uiText("Eingebauter Synth"));
    for (int index = 0; index < daw::Synth::numPresets; ++index)
    {
        const auto preset = static_cast<daw::Synth::Preset>(index);
        menu.addItem(daw::Synth::presetName(preset), true, isSynth && daw::Synth::presetFromSettings(settings) == preset, [this, trackId, preset]
        {
            instrumentEditors.erase(trackId);
            arrangementController.setTrackInstrument(trackId, daw::synthInstrumentSettings(preset));
        });
    }
    menu.addSectionHeader(uiText("VST3-Instrumente"));
    const auto instruments = pluginCatalog.instruments();
    for (const auto& description : instruments)
        menu.addItem(description.name + (description.manufacturerName.isNotEmpty() ? "  (" + description.manufacturerName + ")" : juce::String()), true,
                     ! isSynth && settings.getProperty("name").toString() == description.name, [this, trackId, description]
                     {
                         if (const auto result = setInstrumentSource(trackId, &description); result.failed())
                             status.setText(result.getErrorMessage(), juce::dontSendNotification);
                     });
    if (instruments.isEmpty())
        menu.addItem(uiText("(keine gefunden – Plugins verwalten …)"), false, false, [] {});
    menu.addSeparator();
    menu.addItem(uiText("Plugin-Fenster öffnen"), ! isSynth, false, [this, trackId] { openInstrumentEditor(trackId); });
    menu.addItem(uiText("Plugins verwalten / importieren …"), [this] { showPluginManager(); });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target));
}

void MainComponent::scanPlugins(const juce::StringArray& only, std::function<void()> done)
{
    if (scanningPlugins)
        return;
    scanningPlugins = true;
    status.setText(uiText("Suche VST3-Plugins (in separaten Prozessen) …"), juce::dontSendNotification);
    const auto everything = only.isEmpty();
    if (pluginManager != nullptr)
        pluginManager->refresh();
    const juce::Component::SafePointer<MainComponent> self(this);
    pluginScanPool.addJob([self, catalogCopy = pluginCatalog, only, everything, done]
    {
        // Linux: a full search first sets up the Windows plugins (yabridge), whose
        // bridges then turn up in ~/.vst3 like any other plugin.
        juce::String bridgeStatus = catalogCopy.bridgeStatus;
        if (everything && windowsbridge::isSupportedPlatform()
            && (windowsbridge::bundledPluginFolder().isDirectory() || ! catalogCopy.windowsFolders.isEmpty()))
        {
            const auto error = windowsbridge::bridge(catalogCopy.windowsFolders);
            bridgeStatus = error.isNotEmpty() ? error : uiText("Windows-Plugins über yabridge eingerichtet (") + windowsbridge::wineVersion() + ").";
        }
        const auto files = everything ? catalogCopy.filesToScan() : only;
        auto result = PluginCatalog::scan(files);
        juce::MessageManager::callAsync([self, files, everything, result = std::move(result), bridgeStatus, done]
        {
            if (self == nullptr)
                return;
            self->scanningPlugins = false;
            auto& catalog = self->pluginCatalog;
            catalog.bridgeStatus = bridgeStatus;
            if (everything)
            {
                // Plugins that are gone leave the catalog with a full scan.
                catalog.plugins.clear();
                catalog.failures.clear();
            }
            catalog.merge(files, result);
            self->savePluginCatalog();
            auto message = juce::String(result.found.size()) + uiText(" VST3-Plugin(s) gefunden");
            if (! result.failures.empty())
                message << uiText(", ") << static_cast<int>(result.failures.size()) << uiText(" Datei(en) fehlerhaft – siehe „Plugins verwalten“");
            self->status.setText(message + ".", juce::dontSendNotification);
            if (self->pluginManager != nullptr)
                self->pluginManager->refresh();
            if (done)
                done();
        });
    });
}

void MainComponent::savePluginCatalog()
{
    auto uiState = audioEngine.appSettings().loadUiState();
    if (! uiState.isValid())
        uiState = juce::ValueTree("uiState");
    for (const auto* old : { "pluginCatalog", "vst3Catalog", "drumInstruments" })
        uiState.removeChild(uiState.getChildWithName(old), nullptr);
    uiState.appendChild(pluginCatalog.toValueTree(), nullptr);
    audioEngine.appSettings().saveUiState(uiState);
    updateChainStrip();
}

namespace
{
class PluginManagerWindow final : public juce::DocumentWindow
{
public:
    explicit PluginManagerWindow(juce::Component& content)
        : juce::DocumentWindow(uiText("Plugins verwalten"), Theme::surface(), closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentNonOwned(&content, true);
        setResizable(true, false);
        setResizeLimits(640, 380, 1600, 1200);
        centreWithSize(getWidth(), getHeight());
    }
    void closeButtonPressed() override { setVisible(false); }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            closeButtonPressed();
            return true;
        }
        return DocumentWindow::keyPressed(key);
    }
};
} // namespace

void MainComponent::showPluginManager()
{
    if (pluginManager == nullptr)
    {
        nam::ui::PluginManagerComponent::Host host;
        host.catalog = [this]() -> PluginCatalog& { return pluginCatalog; };
        host.changed = [this] { savePluginCatalog(); };
        host.scan = [this](const juce::StringArray& files, std::function<void()> done) { scanPlugins(files, std::move(done)); };
        host.scanning = [this] { return scanningPlugins; };
        pluginManager = std::make_unique<nam::ui::PluginManagerComponent>(std::move(host));
        pluginManagerWindow = std::make_unique<PluginManagerWindow>(*pluginManager);
    }
    pluginManager->refresh();
    pluginManagerWindow->setVisible(true);
    pluginManagerWindow->toFront(true);
}

void MainComponent::showPitchShifter(juce::Rectangle<int> anchor)
{
    auto editor = std::make_unique<nam::ui::PitchShiftComponent>();
    editor->setSettings({ currentSound.pitchBypassed, currentSound.pitchSemitones, currentSound.pitchMix,
                          currentSound.pitchFormantPreserved, currentSound.pitchHighQuality, currentSound.pitchAnalogOctave });
    const juce::Component::SafePointer<MainComponent> self(this);
    editor->onChange = [self](const nam::ui::PitchShiftComponent::Settings& settings)
    {
        if (self == nullptr)
            return;
        auto& sound = self->currentSound;
        sound.pitchBypassed = settings.bypassed;
        sound.pitchSemitones = settings.semitones;
        sound.pitchMix = settings.mix;
        sound.pitchFormantPreserved = settings.formantPreserved;
        sound.pitchHighQuality = settings.highQuality;
        sound.pitchAnalogOctave = settings.analogOctave;
        self->audioEngine.setPitch(settings.bypassed, settings.semitones, settings.mix, settings.formantPreserved, settings.highQuality, settings.analogOctave);
    };
    editor->latencyMs = [self]
    {
        if (self == nullptr)
            return 0.0;
        auto* device = self->audioEngine.deviceManager().getCurrentAudioDevice();
        return device != nullptr && device->getCurrentSampleRate() > 0.0
            ? self->audioEngine.pitchLatencySamples() * 1000.0 / device->getCurrentSampleRate() : 0.0;
    };
    juce::CallOutBox::launchAsynchronously(std::move(editor), anchor, nullptr);
}

nam::ui::TrackFxComponent::Sources MainComponent::trackFxSources()
{
    nam::ui::TrackFxComponent::Sources sources;
    juce::Array<juce::File> curatedFiles;
    for (const auto& model : curatedAmpLibrary.models())
    {
        const auto file = defaultAssetsDirectory().getChildFile(model.namFile);
        for (const auto& item : namLibrary.items())
            if (item.linkedFile == file)
            {
                sources.amps.push_back({ model.ampName + uiText(" · ") + model.configurationName, item.id.toString() });
                curatedFiles.add(file);
                break;
            }
    }
    for (const auto& item : namLibrary.items())
        if (! curatedFiles.contains(item.linkedFile) && ! item.isMissing())
            sources.amps.push_back({ "Library: " + item.displayName, item.id.toString() });
    for (const auto& item : irLibrary.items())
        if (! item.isMissing())
            sources.irs.push_back({ item.displayName, item.id.toString() });
    for (const auto& file : defaultAssetsDirectory().getSiblingFile("pedal-models").findChildFiles(juce::File::findFiles, false, "*.nam"))
        sources.pedals.push_back({ file.getFileNameWithoutExtension(), file.getFullPathName() });
    std::sort(sources.pedals.begin(), sources.pedals.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    sources.captureLiveRig = [this] { return captureSoundState(); };
    sources.plugins = [this] { return pluginCatalog.visible(); };
    sources.rescanPlugins = [this](std::function<void()> done) { scanPlugins({}, std::move(done)); };
    sources.managePlugins = [this] { showPluginManager(); };
    return sources;
}

void MainComponent::applyTrackSound(RigChain& chain, const SoundState& sound)
{
    chain.applyEffectState(sound);
    chain.setNamPedal(sound.namPedalPath, sound.namPedalBypassed, sound.namPedalInputDb, sound.namPedalOutputDb);
    auto* rig = dynamic_cast<TrackRig*>(&chain);
    if (rig == nullptr)
        return;
    if (rig->namId != sound.namId)
    {
        rig->namId = sound.namId;
        if (const auto* item = namLibrary.find(juce::Uuid(sound.namId)); item != nullptr && ! item->isMissing())
            chain.loadNamAsync(item->linkedFile);
        else
            chain.setNamBypassed(true);
    }
    if (sound.namId.isEmpty())
        chain.setNamBypassed(true);
    if (rig->irId != sound.irId)
    {
        rig->irId = sound.irId;
        if (const auto* item = irLibrary.find(juce::Uuid(sound.irId)); item != nullptr && ! item->isMissing())
            chain.loadIr(item->linkedFile);
    }
    if (sound.irId.isEmpty())
        chain.setIrBypassed(true);
    chain.setLayout(sound.layout); // plugins reload only when their state changed
}

void MainComponent::exportDesignPreviews(const juce::File& folder)
{
    folder.createDirectory();
    const auto original = getBounds();
    for (auto size : { juce::Point<int>(1020,820), juce::Point<int>(1280,860), juce::Point<int>(1800,1040) })
    {
        setSize(size.x, size.y);
        for (int card : { -1, 0, 1, 2, 3, 4 })
        {
            effectParameters.previewExpanded(card);
            auto snapshot = createComponentSnapshot(getLocalBounds(), true, 1.0f);
            juce::FileOutputStream stream(folder.getChildFile(juce::String(size.x) + "-card" + juce::String(card) + ".png"));
            juce::PNGImageFormat().writeImageToStream(snapshot, stream);
        }
        const auto savedEffects = effectParameters.state();
        for (int voice : { 1, 2 })
        {
            auto preview = savedEffects;
            preview.driveType = voice;
            effectParameters.setState(preview);
            effectParameters.previewExpanded(0);
            auto snapshot = createComponentSnapshot(getLocalBounds(), true, 1.0f);
            juce::FileOutputStream stream(folder.getChildFile(juce::String(size.x) + "-drive" + juce::String(voice) + ".png"));
            juce::PNGImageFormat().writeImageToStream(snapshot, stream);
        }
        effectParameters.setState(savedEffects);
    }
    effectParameters.previewExpanded(0);
    setBounds(original);
    if (!rigLibrary.rigs().isEmpty())
    {
        const auto savedSound=captureSoundState();
        const auto savedLoaded=loadedPreset;
        const auto savedLast=audioEngine.appSettings().loadLastPreset();
        loadSelectedRig();
        effectParameters.previewExpanded(-1);
        juce::FileOutputStream stream(folder.getChildFile("complete-rig.png"));
        juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),stream);
        const auto checkSound=captureSoundState();
        juce::XmlElement result("RigUiCheck");
        result.setAttribute("rig",checkSound.rigId);
        result.setAttribute("extraEffectsOff",checkSound.driveBypassed && checkSound.autoWahBypassed && checkSound.chorusBypassed && checkSound.delayBypassed && checkSound.reverbBypassed && std::all_of(checkSound.layout.serial.begin(), checkSound.layout.serial.end(), [](const chain::Node& n) { return n.kind != "vst3" || n.bypassed; }));
        result.writeTo(folder.getChildFile("rig-ui-check.xml"));
        applySoundState(savedSound);
        loadedPreset=savedLoaded; presetBrowser.setSelectedPreset(savedLoaded);
        if(savedLast) audioEngine.appSettings().saveLastPreset(*savedLast);
        else audioEngine.appSettings().clearLastPreset();
    }
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff171a19));
    auto wood = [&](juce::Rectangle<int> box, juce::Colour light, juce::Colour dark)
    {
        auto r = box.toFloat();
        g.setColour(juce::Colours::black.withAlpha(.5f)); g.fillRoundedRectangle(r.translated(0, 5), 14);
        g.setGradientFill(juce::ColourGradient(light, r.getX(), r.getY(), dark, r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 14);
        g.saveState(); g.reduceClipRegion(box.reduced(3));
        for (int i = 0; i < box.getHeight(); i += 3)
        {
            juce::Path grain;
            for (int j = 0; j <= box.getWidth(); j += 8)
            {
                float py = float(box.getY()+i) + std::sin(j*.017f+i*.31f)*1.5f + std::sin(j*.004f+i)*2;
                if (j == 0) grain.startNewSubPath(float(box.getX()), py);
                else grain.lineTo(float(box.getX()+j), py);
            }
            g.setColour(juce::Colour(i%9 == 0 ? 0xffe5bd85 : 0xff170f09).withAlpha(.065f));
            g.strokePath(grain, juce::PathStrokeType(.7f));
        }
        g.restoreState();
        g.setColour(juce::Colours::white.withAlpha(.15f)); g.drawRoundedRectangle(r.reduced(.5f), 14, 1);
    };
    if (view != View::rig)
        return;
    wood(soundPanel, juce::Colour(0xff705139), juce::Colour(0xff38281f));
    wood(transportPanel, juce::Colour(0xff4b3c30), juce::Colour(0xff292722));
    g.setColour(Theme::subduedText()); g.setFont(juce::FontOptions(11, juce::Font::bold));
    g.drawText("AMP / MODELL", soundPanel.getX()+22, soundPanel.getY()+14, 210, 20, juce::Justification::left);
    g.drawText("KONFIGURATION", soundPanel.getCentreX()+8, soundPanel.getY()+14, 230, 20, juce::Justification::left);
    g.drawText(uiText("QUELLE & AUFNAHME"), transportPanel.reduced(18).removeFromTop(20), juce::Justification::left);
}

void MainComponent::resized()
{
    // The rig keeps a centred, fixed-size layout; arrangement and library use the whole window.
    auto area = view == View::rig ? getLocalBounds().withSizeKeepingCentre(juce::jmin(1440, getWidth()), juce::jmin(900, getHeight())).reduced(24)
                                  : getLocalBounds().reduced(16, 10);
    auto header = area.removeFromTop(42);
    title.setBounds(header.removeFromLeft(220));
    // Main navigation: RIG | ARRANGEMENT | BIBLIOTHEK.
    auto tabs = header.removeFromLeft(400).reduced(0, 4);
    tabRig.setBounds(tabs.removeFromLeft(110));
    tabArrangement.setBounds(tabs.removeFromLeft(150));
    tabLibrary.setBounds(tabs.removeFromLeft(130));
    configureAudio.setBounds(header.removeFromRight(114).reduced(0,4));
    header.removeFromRight(12); audioSummaryLabel.setBounds(header);
    audioSummaryLabel.setJustificationType(juce::Justification::centredRight);
    const auto layoutFooter = [this](juce::Rectangle<int> footer)
    {
        diagnosticsToggle.setBounds(footer.removeFromRight(104));
        latency.setBounds(footer.removeFromRight(330));
        status.setBounds(footer);
    };
    const auto placeOverlays = [this]
    {
        if (diagnosticsPanel.isVisible())
            diagnosticsPanel.setBounds(getLocalBounds().reduced(70).removeFromBottom(240));
    };
    if (view != View::rig)
    {
        // Arrangement and library use the full window below the header; the live rig keeps running.
        auto full = getLocalBounds().reduced(16).withTrimmedTop(area.getY() - 16);
        layoutFooter(full.removeFromBottom(28));
        full.removeFromBottom(6);
        arrangementView.setBounds(full);
        namBrowser.setBounds(full.removeFromLeft(full.getWidth() / 2).withTrimmedRight(6));
        irBrowser.setBounds(full.withTrimmedLeft(6));
        soundPanel = transportPanel = {};
        placeOverlays();
        return;
    }
    auto rigRow=area.removeFromTop(44).reduced(0,5);
    rigSelector.setBounds(rigRow.removeFromLeft(juce::jmin(400,rigRow.getWidth()/2)));
    rigRow.removeFromLeft(8); loadRigButton.setBounds(rigRow.removeFromLeft(90));
    rigRow.removeFromLeft(8); rigDetailsButton.setBounds(rigRow.removeFromLeft(65));
    rigRow.removeFromLeft(12); rigInfo.setBounds(rigRow);
    layoutFooter(area.removeFromBottom(32));
    // The signal chain across the full width: the rig's central control surface.
    area.removeFromTop(8);
    {
        // Many blocks scroll sideways; a second row appears for blocks beside the chain.
        const auto width = area.getWidth();
        const auto stripWidth = chainStrip.preferredWidth(width);
        const auto height = chainStrip.preferredHeight() + (stripWidth > width ? 10 : 0);
        chainViewport.setBounds(area.removeFromTop(height));
        chainStrip.setSize(stripWidth, chainStrip.preferredHeight());
    }
    area.removeFromTop(14); area.removeFromBottom(12);
    auto right = area.removeFromRight(juce::jlimit(300, 420, area.getWidth()/3));
    area.removeFromRight(18);
    effectParameters.setBounds(right);
    soundPanel = area.removeFromTop(318);
    auto sound = soundPanel.reduced(22);
    sound.removeFromTop(18);
    auto selectors = sound.removeFromTop(38);
    auto half = selectors.getWidth()/2;
    ampSelector.setBounds(selectors.removeFromLeft(half).withTrimmedRight(8));
    configurationSelector.setBounds(selectors.withTrimmedLeft(8));
    sound.removeFromTop(14);
    auto knobs = sound.removeFromTop(140);
    auto cell = knobs.getWidth()/5;
    for (auto* knob : { &inputGain, &bass, &mid, &treble, &outputGain })
        knob->setBounds(knobs.removeFromLeft(cell).reduced(2));
    auto meters = sound.removeFromTop(50);
    inputMeter.setBounds(meters.removeFromLeft(meters.getWidth()/2).withTrimmedRight(10));
    outputMeter.setBounds(meters.withTrimmedLeft(10));
    sound.removeFromTop(8);
    selectIr.setBounds(sound.removeFromRight(110).withHeight(28)); sound.removeFromRight(6);
    selectNam.setBounds(sound.removeFromRight(126).withHeight(28));
    area.removeFromTop(14);
    presetBrowser.setBounds(area.removeFromTop(110));
    area.removeFromTop(14);
    transportPanel = area;
    auto transport = area.reduced(18); transport.removeFromTop(26);
    auto row = transport.removeFromTop(30);
    liveMode.setBounds(row.removeFromLeft(62));
    wavMode.setBounds(row.removeFromLeft(62)); row.removeFromLeft(12);
    record.setBounds(row.removeFromRight(90));
    chooseWavFolder.setBounds(row.removeFromLeft(120)); row.removeFromLeft(6);
    openWav.setBounds(row.removeFromLeft(104));
    const auto wav = audioEngine.getSourceMode() == AudioEngine::SourceMode::wav;
    for (auto* c : std::initializer_list<juce::Component*> { &chooseWavFolder, &openWav, &playWav, &pauseWav, &stopWav, &loopWav, &wavTimeline })
        c->setVisible(wav);
    liveHint.setVisible(! wav);
    transport.removeFromTop(10);
    liveHint.setBounds(transport.withHeight(std::min(60, transport.getHeight())));
    row = transport.removeFromTop(28);
    playWav.setBounds(row.removeFromLeft(86)); row.removeFromLeft(6);
    pauseWav.setBounds(row.removeFromLeft(62)); row.removeFromLeft(6);
    stopWav.setBounds(row.removeFromLeft(62)); row.removeFromLeft(8);
    loopWav.setBounds(row.removeFromLeft(90));
    transport.removeFromTop(6); wavTimeline.setBounds(transport.removeFromTop(24));
    transport.removeFromTop(8); wavBrowser.setBounds(transport);
    savePreset.setVisible(false); toggleAB.setVisible(false);
    for (auto* t : { &wahEnabled, &chorusEnabled, &delayEnabled, &reverbEnabled }) t->setVisible(false);
    placeOverlays();
}
