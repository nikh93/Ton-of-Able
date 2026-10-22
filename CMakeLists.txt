cmake_minimum_required(VERSION 3.24)

project(NamTestProgram VERSION 0.2.0 LANGUAGES C CXX)

# Keep the standalone application portable: embed the MSVC runtime instead of
# requiring the Visual C++ Redistributable on the target machine. Third-party
# VST3 binaries remain external and may still have their own dependencies.
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Use static MSVC runtime" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT WIN32)
    message(FATAL_ERROR "NamTestProgram currently targets Windows 10 and Windows 11 only.")
endif()

add_subdirectory(third_party/JUCE)

# NeuralAmpModelerCore currently exposes source files and tools, rather than a
# consumer library target. Keep this integration local and explicit so its API
# can be updated independently of the app's DSP architecture.
file(GLOB NAM_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NeuralAmpModelerCore/NAM/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NeuralAmpModelerCore/NAM/*/*.cpp")

# Rubber Band Library (time-stretching / pitch-shifting, GPL-2.0-or-later) is
# compiled from its single-file build with the built-in FFT and resampler, so no
# separate library or extra runtime dependency is required.
set(RUBBERBAND_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rubberband/single/RubberBandSingle.cpp")
if (MSVC)
    set_source_files_properties(${RUBBERBAND_SOURCES} PROPERTIES COMPILE_OPTIONS "/W1")
endif()

set(DAW_SOURCES
    src/daw/Arrangement.cpp
    src/daw/Arrangement.h
    src/daw/ArrangementEngine.cpp
    src/daw/ArrangementEngine.h
    src/daw/AudioCodecs.cpp
    src/daw/AudioCodecs.h
    src/daw/AudioData.cpp
    src/daw/AudioData.h
    src/audio/effects/LivePitchShifter.cpp
    src/audio/effects/LivePitchShifter.h
    src/audio/effects/CompressorProcessor.h
    src/audio/effects/FlipFlopOctaver.h
    src/daw/DawText.h
    src/daw/Drums.cpp
    src/daw/Drums.h
    src/daw/TimeStretcher.cpp
    src/daw/TimeStretcher.h
)

juce_add_gui_app(NamTestProgram
    PRODUCT_NAME "Ton of Able"
    COMPANY_NAME "Ton of Able"
    BUNDLE_ID "com.tonofable.standalone"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon/app-icon.png"
)
set_target_properties(NamTestProgram PROPERTIES OUTPUT_NAME "Ton of Able")

target_sources(NamTestProgram PRIVATE
    src/Main.cpp
    src/audio/AudioEngine.cpp
    src/audio/AudioEngine.h
    src/audio/NamProcessor.cpp
    src/audio/NamProcessor.h
    src/audio/IRProcessor.cpp
    src/audio/IRProcessor.h
    src/audio/RigChain.cpp
    src/audio/RigChain.h
    src/audio/effects/GraphicEQProcessor.h
    src/audio/plugins/PluginInstrument.cpp
    src/audio/plugins/PluginInstrument.h
    src/audio/plugins/Vst3Slot.cpp
    src/ui/plugins/PluginRackPanel.cpp
    src/ui/plugins/NamPedalPanel.cpp
    src/audio/NamPedalProcessor.cpp
    src/audio/effects/DriveProcessor.cpp
    src/audio/effects/DriveProcessor.h
    src/library/LibraryManager.cpp
    src/library/LibraryManager.h
    src/library/ModelFolder.cpp
    src/library/ModelFolder.h
    src/library/NamLibrary.h
    src/library/IRLibrary.h
    src/library/CuratedAmpLibrary.cpp
    src/library/RigLibrary.cpp
    src/library/CuratedAmpLibrary.h
    src/presets/PresetManager.cpp
    src/presets/PresetManager.h
    src/playback/FilePlayer.cpp
    src/playback/FilePlayer.h
    src/playback/Recorder.cpp
    src/playback/Recorder.h
    src/diagnostics/PerformanceMonitor.cpp
    src/diagnostics/PerformanceMonitor.h
    src/diagnostics/Logger.cpp
    src/diagnostics/Logger.h
    src/config/AppSettings.cpp
    src/config/AppSettings.h
    src/ui/MainComponent.cpp
    src/ui/MainComponent.h
    src/ui/presets/PresetManagementComponent.cpp
    src/ui/presets/PresetManagementComponent.h
    src/ui/components/AmberTheme.cpp
    src/ui/components/AmberTheme.h
    src/ui/components/RotaryParameterControl.cpp
    src/ui/components/RotaryParameterControl.h
    src/ui/components/AudioMeter.cpp
    src/ui/components/AudioMeter.h
    src/ui/diagnostics/DiagnosticsPanel.cpp
    src/ui/diagnostics/DiagnosticsPanel.h
    src/ui/library/LibraryBrowser.cpp
    src/ui/library/LibraryBrowser.h
    src/ui/effects/EffectParameterPanel.cpp
    src/ui/effects/EffectParameterPanel.h
    src/ui/playback/WavFileBrowser.cpp
    src/ui/playback/WavFileBrowser.h
    src/daw/ArrangementController.cpp
    src/daw/ArrangementController.h
    src/ui/shortcuts/Shortcuts.cpp
    src/ui/shortcuts/Shortcuts.h
    src/ui/arrangement/ArrangementView.cpp
    src/ui/arrangement/ArrangementView.h
    src/ui/arrangement/DrumMachineComponent.cpp
    src/ui/arrangement/DrumMachineComponent.h
    src/ui/arrangement/TrackFxComponent.cpp
    src/ui/arrangement/TrackFxComponent.h
    src/ui/effects/GraphicEqComponent.cpp
    src/ui/effects/GraphicEqComponent.h
    src/ui/effects/PitchShiftComponent.cpp
    src/ui/effects/PitchShiftComponent.h
    src/ui/arrangement/TimelineComponent.cpp
    src/ui/arrangement/TimelineComponent.h
    src/ui/arrangement/TrackHeaderComponent.cpp
    src/ui/arrangement/TrackHeaderComponent.h
    ${DAW_SOURCES}
    ${RUBBERBAND_SOURCES}
    ${NAM_CORE_SOURCES}
)

target_include_directories(NamTestProgram PRIVATE
    src
    third_party/rubberband
    third_party/NeuralAmpModelerCore
    third_party/NeuralAmpModelerCore/Dependencies/eigen
    third_party/NeuralAmpModelerCore/Dependencies/nlohmann
)

target_compile_definitions(NamTestProgram PRIVATE
    JUCE_ASIO=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    NAM_ENABLE_A2_FAST
    NAM_SAMPLE_FLOAT
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(NamTestProgram PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
)

if (MSVC)
    target_compile_options(NamTestProgram PRIVATE /W4 /permissive- /utf-8)
endif()

option(NAM_TEST_PROGRAM_BUILD_TESTS "Build NAM Test Program unit tests" ON)
if (NAM_TEST_PROGRAM_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(NamTestProgramTests)
    target_sources(NamTestProgramTests PRIVATE
        tests/TestMain.cpp
        tests/NamTestProgramTests.cpp
        tests/DriveVoicingTests.cpp
        src/library/LibraryManager.cpp
        src/library/ModelFolder.cpp
        src/library/CuratedAmpLibrary.cpp
        src/presets/PresetManager.cpp
        src/playback/FilePlayer.cpp
        src/diagnostics/PerformanceMonitor.cpp
        src/audio/effects/DriveProcessor.cpp
        tests/ArrangementTests.cpp
        tests/BundledModelTests.cpp
        tests/MixingTests.cpp
        src/ui/shortcuts/Shortcuts.cpp
        ${DAW_SOURCES}
        ${RUBBERBAND_SOURCES}
        ${NAM_CORE_SOURCES}
    )
    target_include_directories(NamTestProgramTests PRIVATE src third_party/rubberband third_party/NeuralAmpModelerCore
        third_party/NeuralAmpModelerCore/Dependencies/eigen third_party/NeuralAmpModelerCore/Dependencies/nlohmann)
    target_compile_definitions(NamTestProgramTests PRIVATE JUCE_USE_CURL=0 NAM_SAMPLE_FLOAT NAM_ENABLE_A2_FAST NOMINMAX WIN32_LEAN_AND_MEAN
        TON_OF_ABLE_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
    target_link_libraries(NamTestProgramTests PRIVATE
        juce::juce_audio_formats
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_extra
    )
    if (MSVC)
        target_compile_options(NamTestProgramTests PRIVATE /utf-8 /bigobj)
    endif()
    add_test(NAME NamTestProgramTests COMMAND NamTestProgramTests)
endif()

include(GNUInstallDirs)
juce_add_console_app(DriveAudition)
set_target_properties(DriveAudition PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(DriveAudition PRIVATE tests/DriveAudition.cpp src/audio/effects/DriveProcessor.cpp ${NAM_CORE_SOURCES})
target_include_directories(DriveAudition PRIVATE src third_party/NeuralAmpModelerCore
    third_party/NeuralAmpModelerCore/Dependencies/eigen third_party/NeuralAmpModelerCore/Dependencies/nlohmann)
target_compile_definitions(DriveAudition PRIVATE NAM_SAMPLE_FLOAT NAM_ENABLE_A2_FAST JUCE_USE_CURL=0 NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(DriveAudition PRIVATE juce::juce_audio_formats juce::juce_dsp)
if (MSVC)
    target_compile_options(DriveAudition PRIVATE /utf-8)
endif()
install(TARGETS NamTestProgram
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY resources/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/NamTestProgram/resources
)
