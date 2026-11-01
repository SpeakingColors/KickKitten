cmake_minimum_required(VERSION 3.22)
project(KickKitten VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Engine test (no JUCE needed): renders every factory preset and compares with the mockup's own renders.
add_executable(kickkitten_parity tests/parity.cpp src/engine/KickEngine.cpp src/engine/Params.cpp)
target_include_directories(kickkitten_parity PRIVATE src)

# Plugin. Pass -DKICKKITTEN_JUCE_DIR=/path/to/JUCE to use a local JUCE, otherwise JUCE is fetched.
option(KICKKITTEN_BUILD_PLUGIN "Build the VST3 plugin" ON)
if(KICKKITTEN_BUILD_PLUGIN)
  if(KICKKITTEN_JUCE_DIR)
    add_subdirectory(${KICKKITTEN_JUCE_DIR} juce)
  else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.16-security GIT_SHALLOW ON)
    FetchContent_MakeAvailable(juce)
  endif()

  juce_add_binary_data(KickKittenData SOURCES assets/bank.bin
    assets/fonts/BarlowCondensed-Regular.ttf assets/fonts/BarlowCondensed-SemiBold.ttf assets/fonts/BarlowCondensed-Bold.ttf
    assets/fonts/JetBrainsMono-Regular.ttf assets/fonts/JetBrainsMono-SemiBold.ttf)
  set_target_properties(KickKittenData PROPERTIES POSITION_INDEPENDENT_CODE ON)

  juce_add_plugin(KickKitten
    COMPANY_NAME "Studio 87"
    BUNDLE_ID "com.studio87.kickkitten"
    PLUGIN_MANUFACTURER_CODE Spkc
    PLUGIN_CODE KicK
    FORMATS VST3 Standalone
    PRODUCT_NAME "Kick Kitten"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Synth Drum
    COPY_PLUGIN_AFTER_BUILD FALSE)

  target_sources(KickKitten PRIVATE
    src/PluginProcessor.cpp src/PluginEditor.cpp src/engine/KickEngine.cpp src/engine/Params.cpp)
  target_include_directories(KickKitten PRIVATE src)
  target_compile_definitions(KickKitten PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 KK_VERSION="${PROJECT_VERSION}")
  target_link_libraries(KickKitten PRIVATE KickKittenData
    juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

# Offscreen snapshot of the real editor (for screenshots and checks). Off by default: -DKICKKITTEN_BUILD_TOOLS=ON
option(KICKKITTEN_BUILD_TOOLS "Build the offscreen snapshot tool" OFF)
if(KICKKITTEN_BUILD_PLUGIN AND KICKKITTEN_BUILD_TOOLS)
  juce_add_console_app(KickKittenSnap PRODUCT_NAME "KickKittenSnap")
  target_sources(KickKittenSnap PRIVATE tests/snapshot.cpp src/PluginProcessor.cpp src/PluginEditor.cpp src/engine/KickEngine.cpp src/engine/Params.cpp)
  target_include_directories(KickKittenSnap PRIVATE src)
  target_compile_definitions(KickKittenSnap PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 KK_VERSION="${PROJECT_VERSION}")
  target_link_libraries(KickKittenSnap PRIVATE KickKittenData juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
endif()

if(KICKKITTEN_BUILD_PLUGIN AND KICKKITTEN_BUILD_TOOLS)
  juce_add_console_app(KickKittenCheck PRODUCT_NAME "KickKittenCheck")
  target_sources(KickKittenCheck PRIVATE tests/proctest.cpp src/PluginProcessor.cpp src/PluginEditor.cpp src/engine/KickEngine.cpp src/engine/Params.cpp)
  target_include_directories(KickKittenCheck PRIVATE src)
  target_compile_definitions(KickKittenCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 KK_VERSION="${PROJECT_VERSION}")
  target_link_libraries(KickKittenCheck PRIVATE KickKittenData juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
endif()
