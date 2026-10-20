cmake_minimum_required(VERSION 3.22)

# Universal Mac build (Apple Silicon + Intel). Must be set before project().
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

project(SoulfulSauce VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout if given (-DSAUCE_JUCE_DIR=libs/JUCE), otherwise download it.
set(SAUCE_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")
if(SAUCE_JUCE_DIR)
    add_subdirectory(${SAUCE_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Chord engine (no JUCE) + parity test against the original FL Studio script
# ---------------------------------------------------------------------------
add_library(SauceEngine STATIC source/engine/ChordEngine.cpp source/engine/Pattern.cpp)
target_include_directories(SauceEngine PUBLIC source/engine)
# Exact floating point: no fused multiply-add, so results match the reference script.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(SauceEngine PRIVATE -ffp-contract=off)
endif()
set_target_properties(SauceEngine PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(EngineParityTest tests/EngineParityTest.cpp)
target_link_libraries(EngineParityTest PRIVATE SauceEngine)

enable_testing()
add_test(NAME EngineParity COMMAND EngineParityTest ${CMAKE_CURRENT_SOURCE_DIR}/tests/reference_cases.txt)

# ---------------------------------------------------------------------------
# The plugin
# ---------------------------------------------------------------------------
set(SAUCE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SAUCE_FORMATS AU)
endif()

juce_add_plugin(SoulfulSauce
    COMPANY_NAME "Just Feezy"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.justfeezy.soulfulsauce"
    PLUGIN_MANUFACTURER_CODE JFzy
    PLUGIN_CODE SSau
    PRODUCT_NAME "Soulful Sauce"
    DESCRIPTION "Soulful chord progression generator"
    FORMATS ${SAUCE_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED FALSE)

juce_add_binary_data(SauceAssets SOURCES
    resources/fonts/Manrope-SemiBold.ttf
    resources/fonts/Manrope-Bold.ttf
    resources/fonts/Manrope-ExtraBold.ttf
    resources/fonts/JetBrainsMono-Medium.ttf
    resources/fonts/JetBrainsMono-Bold.ttf)
set_target_properties(SauceAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(SoulfulSauce PRIVATE
    source/Parameters.cpp
    source/SauceSynth.cpp
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/ui/SauceLookAndFeel.cpp
    source/ui/Widgets.cpp
    source/ui/ChordViews.cpp)

target_include_directories(SoulfulSauce PRIVATE source)

target_compile_definitions(SoulfulSauce PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SoulfulSauce
    PRIVATE
        SauceEngine
        SauceAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Optional: run the real processor + editor without a DAW (-DSAUCE_SMOKE_TEST=ON)
# ---------------------------------------------------------------------------
option(SAUCE_SMOKE_TEST "Build the plugin smoke test" OFF)
if(SAUCE_SMOKE_TEST)
    juce_add_console_app(PluginSmokeTest PRODUCT_NAME "PluginSmokeTest")
    target_sources(PluginSmokeTest PRIVATE
        tests/PluginSmokeTest.cpp
        source/Parameters.cpp
        source/SauceSynth.cpp
        source/PluginProcessor.cpp
        source/PluginEditor.cpp
        source/ui/SauceLookAndFeel.cpp
        source/ui/Widgets.cpp
        source/ui/ChordViews.cpp)
    target_include_directories(PluginSmokeTest PRIVATE source)
    target_compile_definitions(PluginSmokeTest PRIVATE
        JucePlugin_Name="Soulful Sauce"
        JucePlugin_VersionString="0.1.0"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(PluginSmokeTest PRIVATE
        SauceEngine
        SauceAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_recommended_config_flags)
endif()
