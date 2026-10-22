#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "audio/AudioEngine.h"
#include "ui/components/AmberTheme.h"
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
#include "ui/plugins/PluginRackPanel.h"
#include "ui/arrangement/ArrangementView.h"
#include "ui/arrangement/TrackFxComponent.h"
#include "ui/effects/PitchShiftComponent.h"
#include "ui/arrangement/DrumMachineComponent.h"
#include "audio/plugins/PluginInstrument.h"
#include "daw/ArrangementController.h"

class MainComponent final : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    AppSettings& appSettings() noexcept { return audioEngine.appSettings(); }

    void paint(juce::Graphics&) override;
    void resized() override;
    void exportDesignPreviews(const juce::File&);
    bool keyPressed(const juce::KeyPress&) override;
    /** Opens a .tonproj (e.g. passed on the command line) in the arrangement view. */
    void openProject(const juce::File&);

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
    void registerBundledDefaults();
    void timerCallback() override;
    [[nodiscard]] SoundState captureSoundState(bool includePlugins = true);
    void applySoundState(const SoundState& state);
    void configurePresetBrowser();
    void persistPresetState();
    void setLoadedPreset(const juce::Uuid& id);
    void switchAB(bool useA);
    void showAudioSettings();
    juce::String audioSummary() const;
    void configureArrangement();
    void setArrangementVisible(bool visible);
    void showGraphicEq();
    void applyGraphicEq(bool bypassed, const std::array<float, 10>& bands);
    void showPitchShifter();
    void scanDrumInstruments(std::function<void()> done);
    void openInstrumentEditor(const juce::Uuid& trackId);
    juce::Result setDrumSource(const juce::Uuid& trackId, const juce::PluginDescription* description);
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
    juce::Array<juce::PluginDescription> drumInstruments;
    juce::ThreadPool instrumentScanPool { juce::ThreadPoolOptions {}.withThreadName("Instrument scan").withNumberOfThreads(1) };
    bool scanningInstruments = false;
    std::map<juce::Uuid, std::unique_ptr<juce::DocumentWindow>> instrumentEditors;
    bool arrangementVisible = false;
    PluginRackPanel pluginRack { audioEngine };
    std::unique_ptr<juce::DocumentWindow> pluginWindow;
    juce::TextButton pluginsButton { "Pedals / VST3" };
    NamLibrary namLibrary;
    IRLibrary irLibrary;
    PresetManager presetManager;
    nam::ui::PresetManagementComponent presetBrowser;
    nam::ui::EffectParameterPanel effectParameters;
    nam::ui::WavFileBrowser wavBrowser;
    nam::ui::LibraryBrowser namBrowser { namLibrary, "NAM LIBRARY" };
    nam::ui::LibraryBrowser irBrowser { irLibrary, "IR LIBRARY" };
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
    juce::Label audioSummaryLabel;
    juce::TextButton configureAudio { "Settings" };
    juce::TextButton selectNam { "NAM importieren" };
    juce::TextButton selectIr { "IR importieren" };
    juce::ComboBox ampSelector;
    juce::ComboBox configurationSelector;
    juce::TextButton liveMode { "LIVE" };
    juce::TextButton wavMode { "WAV" };
    juce::TextButton openWav { "WAV Oeffnen" };
    juce::TextButton chooseWavFolder { "Ordner" };
    juce::TextButton playWav { "Play" };
    juce::TextButton pauseWav { "Pause" };
    juce::TextButton stopWav { "Stop" };
    juce::ToggleButton loopWav { "Loop" };
    juce::Slider wavTimeline;
    juce::ToggleButton namEnabled { "NAM" };
    juce::ToggleButton irEnabled { "IR" };
    juce::TextButton record { "● REC" };
    juce::TextButton savePreset { "Save" };
    juce::TextButton toggleAB { "A" };
    juce::ToggleButton diagnosticsToggle { "Diagnostics" };
    juce::ToggleButton librariesToggle { "Libraries" };
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
    std::unique_ptr<juce::DialogWindow> audioSettingsWindow;
    std::shared_ptr<juce::FileChooser> namChooser;
    std::shared_ptr<juce::FileChooser> irChooser;
    std::shared_ptr<juce::FileChooser> wavChooser;
    std::shared_ptr<juce::FileChooser> wavDirectoryChooser;
    std::shared_ptr<juce::FileChooser> recordChooser;
    nam::ui::StudioLookAndFeel lookAndFeel;
    juce::Rectangle<int> soundPanel, transportPanel;
    juce::StringArray curatedAmpIds;
    juce::StringArray curatedConfigurationIds;
    juce::TooltipWindow rigTooltips {this,400};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
