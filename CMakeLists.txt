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

juce_add_console_app(PedaleiraNAM
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
)

target_include_directories(PedaleiraNAM PRIVATE Source)

# Phase 0: no networking (TONE3000 arrives in Phase 1) and no webview -- keeps the build lean.
target_compile_definitions(PedaleiraNAM PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

target_link_libraries(PedaleiraNAM PRIVATE
  juce::juce_audio_basics
  juce::juce_audio_devices
  juce::juce_audio_processors
  juce::juce_core
  juce::juce_events
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)

enable_testing()
add_subdirectory(Tests)
