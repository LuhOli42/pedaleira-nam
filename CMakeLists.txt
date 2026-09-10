cmake_minimum_required(VERSION 3.22)
project(PedaleiraNAM VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.9
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

include(cmake/NAMCore.cmake)

# A real (if dev-facing, pre-touch, pre-Phase-7) GUI, not a console app --
# see AGENT.md: the user wants to SEE the chain being built, not just trust
# test output. Still no presets/split-merge yet, on purpose.
juce_add_gui_app(PedaleiraNAM
  PRODUCT_NAME "Pedaleira NAM"
  COMPANY_NAME "Pedaleira NAM"
  BUNDLE_ID "com.pedaleiranam.PedaleiraNAM"
)

target_sources(PedaleiraNAM PRIVATE
  Source/Main.cpp
  Source/EffectRegistry.cpp
  Source/Engine/AudioEngine.cpp
  Source/Engine/SignalGraph.cpp
  Source/Engine/ParameterManager.cpp
  Source/Effects/GateProcessor.cpp
  Source/Effects/CompressorProcessor.cpp
  Source/Effects/OverdriveProcessor.cpp
  Source/Effects/NAMProcessor.cpp
  Source/UI/EffectBlockComponent.cpp
  Source/UI/ParameterPanel.cpp
  Source/UI/MainComponent.cpp
)

target_include_directories(PedaleiraNAM PRIVATE Source)

# No webview needed. Curl IS needed from here on for TONE3000 (Tone3000Manager).
target_compile_definitions(PedaleiraNAM PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=1
)

# JUCE_USE_CURL only makes juce_core.cpp call into libcurl -- it doesn't link
# it. Without this, the link fails with undefined references to curl_*.
find_package(CURL REQUIRED)

# No LTO: this is a dev-iteration build, and link-time optimization roughly
# doubles peak memory during the final link (confirmed: it was spawning 20
# parallel LTRANS jobs). On a machine this size that's the difference
# between a slow build and a hard crash -- see scripts/build.sh for the
# incident this comment refers to. Revisit for an actual release build.
target_link_libraries(PedaleiraNAM PRIVATE
  juce::juce_audio_basics
  juce::juce_audio_devices
  juce::juce_audio_processors
  juce::juce_core
  juce::juce_events
  juce::juce_gui_basics
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
  nam_core
  CURL::libcurl
)

enable_testing()
add_subdirectory(Tests)
