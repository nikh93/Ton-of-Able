#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "audio/AudioEngine.h"
#include "ui/components/AmberTheme.h"
#include "ui/components/Icons.h"
#include "ui/components/AudioMeter.h"
#include "ui/components/RotaryParameterControl.h"
#include "ui/diagnostics/DiagnosticsPanel.h"
#include "library/NamLibrary.h"
#include "library/IRLibrary.h"
#include "presets/PresetManager.h"
#include "ui/presets/PresetManagementComponent.h"
#include "ui/effects/EffectParameterPanel.h"
#include "ui/library/LibraryBrowser.h"
#include "ui/playback/WavFileBrowser.h"
#include "library/CuratedAmpLibrary.h"
#include "library/RigLibrary.h"
#include "library/ModelFolder.h"
#include "ui/plugins/NamPedalPanel.h"
#include "ui/plugins/PluginManagerComponent.h"
#include "audio/plugins/PluginCatalog.h"
#include "audio/plugins/WindowsPluginBridge.h"
#include "ui/arrangement/ArrangementView.h"
#include "ui/arrangement/TrackFxComponent.h"
#include "ui/effects/PitchShiftComponent.h"
#include "ui/arrangement/DrumMachineComponent.h"
#include "audio/plugins/PluginInstrument.h"
#include "audio/plugins/Vst3Slot.h"
#include "daw/ArrangementController.h"
#include "daw/Synth.h"
#include "ui/shortcuts/Shortcuts.h"
#include "ui/rig/SignalChainStrip.h"

class MainComponent final : public juce::Component, public juce::FileDragAndDropTarget, public juce::ApplicationCommandTarget,
                            private juce::Timer, private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    AppSettings& appSettings() noexcept { return audioEngine.appSettings(); }

    void paint(juce::Graphics&) override;
    void resized() override;
    void exportDesignPreviews(const juce::File&);
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    /** Opens a .tonproj (e.g. passed on the command line) in the arrangement view. */
    void openProject(const juce::File&);

    // juce::ApplicationCommandTarget: every keyboard shortcut is a command (see ui/shortcuts).
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands(juce::Array<juce::CommandID>&) override;
    void getCommandInfo(juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform(const InvocationInfo&) override;

private:
    void chooseNamFile();
    void chooseIrFile();
    void chooseWavFile();
    void chooseWavDirectory();
    void loadAndPlayWav(const juce::File& file);
    void chooseRecordingTarget();
    void configureMainControls();
    void configureCuratedSelectors();
    void applyCuratedModel(const CuratedAmpModel& model);
    void configureRigs();
    void loadSelectedRig();
    juce::File defaultAssetsDirectory() const;
    juce::File drumKitsDirectory() const;
    void registerBundledDefaults();
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void setUpCommands();
    void saveKeyMappings(juce::ValueTree& uiState);
    void showShortcuts();
    /** Mouse as wah pedal: the vertical position on the screen moves the pedal. */
    void followMouseWithWah();
    void toggleMouseWah();
    [[nodiscard]] SoundState captureSoundState(bool includePlugins = true);
    void applySoundState(const SoundState& state);
    void configurePresetBrowser();
    void persistPresetState();
    void setLoadedPreset(const juce::Uuid& id);
    void switchAB(bool useA);
    void showAudioSettings();
    juce::String audioSummary() const;
    void configureArrangement();
    /** Main views, chosen with the tabs in the header. */
    enum class View { rig, arrangement, library };
    void setView(View);
    void setArrangementVisible(bool visible) { setView(visible ? View::arrangement : View::rig); }
    void showGraphicEq(juce::Rectangle<int> anchor);
    void applyGraphicEq(bool bypassed, const std::array<float, 10>& bands);
    void showPitchShifter(juce::Rectangle<int> anchor);
    void showPedal();
    /** Applies an edited signal chain to the live rig. */
    void applyLayout(const chain::Layout&);
    [[nodiscard]] nam::ui::SignalChainStrip::Block describeBlock(const chain::Node&);
    void openPluginEditor(const juce::String& blockId);
    /** Closes plugin windows of blocks that are gone (or all). */
    void closePluginEditors(bool all);
    // The signal chain strip: blocks in processing order, LED = on/off, click = editor.
    void updateChainStrip();
    void toggleChainBlock(const juce::String& id);
    void openChainBlock(const juce::String& id);
    [[nodiscard]] ModelFolder modelFolder() const { return ModelFolder(modelFolderRoot); }
    void importModels(const juce::Array<juce::File>& selection, bool nam);
    void showModelImportMenu(bool nam, juce::Component& target);
    void addModelFolderItems(juce::PopupMenu&);
    void chooseModelFolder();
    void syncModelFolder();
    /** Scans plugin files (each in a child process) into the catalog; no files: everything. */
    void scanPlugins(const juce::StringArray& files, std::function<void()> done);
    void savePluginCatalog();
    void showPluginManager();
    void openInstrumentEditor(const juce::Uuid& trackId);
    /** Instrument of a drum or MIDI track: a VST3 instrument, or null for the built-in one. */
    juce::Result setInstrumentSource(const juce::Uuid& trackId, const juce::PluginDescription* description);
    /** The instrument menu of a MIDI track (synth presets, VST3 instruments, plugin window). */
    juce::ValueTree spaceReverbSettings();
    void showInstrumentMenu(const juce::Uuid& trackId, juce::Component& target);
    [[nodiscard]] nam::ui::TrackFxComponent::Sources trackFxSources();
    void applyTrackSound(RigChain& chain, const SoundState& sound);
    [[nodiscard]] std::vector<juce::Component*> rigComponents();

    AudioEngine audioEngine;
    daw::ArrangementController arrangementController { audioEngine.arrangement(), audioEngine.deviceManager(),
                                                       [this] { return audioEngine.rigLatencySamples(); } };
    nam::ui::ArrangementView arrangementView { arrangementController };
    juce::TextButton viewToggle { "ARRANGEMENT" };
    juce::TextButton graphicEqButton { "GRAPHIC EQ" };
    juce::TextButton pitchButton { "PITCH" };
    juce::File modelFolderRoot;
    std::shared_ptr<juce::FileChooser> modelChooser;
    PluginCatalog pluginCatalog;
    std::unique_ptr<nam::ui::PluginManagerComponent> pluginManager;
    std::unique_ptr<juce::DocumentWindow> pluginManagerWindow;
    std::map<juce::String, std::unique_ptr<juce::DocumentWindow>> pluginEditors; // by chain block
    juce::ThreadPool pluginScanPool { juce::ThreadPoolOptions {}.withThreadName("Plugin scan").withNumberOfThreads(1) };
    bool scanningPlugins = false;
    std::map<juce::Uuid, std::unique_ptr<juce::DocumentWindow>> instrumentEditors;
    bool arrangementVisible = false;
    View view = View::rig;
    nam::ui::SignalChainStrip chainStrip;
    juce::Viewport chainViewport;
    juce::TextButton tabRig { "RIG" }, tabArrangement { "ARRANGEMENT" }, tabLibrary { "BIBLIOTHEK" };
    NamPedalPanel pedalPanel { audioEngine };
    std::unique_ptr<juce::DocumentWindow> pedalWindow;
    juce::ApplicationCommandManager commandManager;
    juce::TimedCallback wahMouseTimer { [this] { followMouseWithWah(); } };
    float lastMouseWah = -1.0f;
    std::unique_ptr<juce::DocumentWindow> shortcutWindow; // after commandManager: it edits its key mappings
    NamLibrary namLibrary;
    IRLibrary irLibrary;
    PresetManager presetManager;
    nam::ui::PresetManagementComponent presetBrowser;
    nam::ui::EffectParameterPanel effectParameters;
    nam::ui::WavFileBrowser wavBrowser;
    nam::ui::LibraryBrowser namBrowser { namLibrary, "NAM-BIBLIOTHEK" };
    nam::ui::LibraryBrowser irBrowser { irLibrary, "IR-BIBLIOTHEK" };
    CuratedAmpLibrary curatedAmpLibrary;
    RigLibrary rigLibrary;
    juce::ComboBox rigSelector;
    juce::TextButton loadRigButton {"Rig laden"};
    juce::TextButton rigDetailsButton {"Details"};
    juce::Label rigInfo;
    juce::Uuid loadedPreset;
    SoundState currentSound;
    ABState abState;
    bool abInitialised = false;
    juce::Label title;
    juce::Label status;
    juce::Label latency;
    /** Audio device summary in the header; a click opens the settings. */
    struct ClickableLabel final : juce::Label
    {
        std::function<void()> onClick;
        void mouseUp(const juce::MouseEvent& event) override { if (onClick && event.mouseWasClicked()) onClick(); }
    };
    ClickableLabel audioSummaryLabel;
    juce::TextButton configureAudio { "Einstellungen" };
    juce::TextButton selectNam { "NAM importieren" };
    juce::TextButton selectIr { "IR importieren" };
    juce::ComboBox ampSelector;
    juce::ComboBox configurationSelector;
    juce::TextButton liveMode { "LIVE" };
    juce::TextButton wavMode { "WAV" };
    juce::TextButton openWav { "WAV Oeffnen" };
    juce::TextButton chooseWavFolder { "Ordner" };
    juce::TextButton playWav { "Abspielen" };
    juce::TextButton pauseWav { "Pause" };
    juce::TextButton stopWav { "Stopp" };
    juce::ToggleButton loopWav { "Schleife" };
    juce::Label liveHint;
    juce::Slider wavTimeline;
    juce::ToggleButton namEnabled { "NAM" };
    juce::ToggleButton irEnabled { "IR" };
    nam::ui::IconButton record { nam::ui::Icon::record, "REC" };
    juce::TextButton savePreset { "Save" };
    juce::TextButton toggleAB { "A" };
    juce::ToggleButton diagnosticsToggle { "Diagnose" };
    juce::ToggleButton librariesToggle { "Bibliotheken" };
    juce::ToggleButton wahEnabled { "Auto-Wah" };
    juce::ToggleButton chorusEnabled { "Chorus" };
    juce::ToggleButton delayEnabled { "Delay" };
    juce::ToggleButton reverbEnabled { "Reverb" };
    nam::ui::RotaryParameterControl inputGain { "INPUT" };
    nam::ui::RotaryParameterControl bass { "BASS" };
    nam::ui::RotaryParameterControl mid { "MID" };
    nam::ui::RotaryParameterControl treble { "TREBLE" };
    nam::ui::RotaryParameterControl outputGain { "OUTPUT" };
    nam::ui::AudioMeter inputMeter { &audioEngine.inputPeakSource() };
    nam::ui::AudioMeter outputMeter { &audioEngine.outputPeakSource() };
    nam::ui::DiagnosticsPanel diagnosticsPanel {
        [this] { return audioEngine.performanceSnapshot(); },
        [this] { return audioEngine.recentLogEntries(); }
    };
    std::unique_ptr<juce::DocumentWindow> audioSettingsWindow;
    std::shared_ptr<juce::FileChooser> namChooser;
    std::shared_ptr<juce::FileChooser> irChooser;
    std::shared_ptr<juce::FileChooser> wavChooser;
    std::shared_ptr<juce::FileChooser> wavDirectoryChooser;
    std::shared_ptr<juce::FileChooser> recordChooser;
    nam::ui::StudioLookAndFeel lookAndFeel;
    juce::Rectangle<int> soundPanel, transportPanel;
    juce::StringArray curatedAmpIds;
    juce::StringArray curatedConfigurationIds;
    // On the desktop (no parent), so tooltips also work in the separate windows.
    juce::TooltipWindow tooltips { nullptr, 500 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
