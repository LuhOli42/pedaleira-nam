juce_add_console_app(PedaleiraNAM_Tests
  PRODUCT_NAME "Pedaleira NAM Tests"
  BUNDLE_ID "com.pedaleiranam.PedaleiraNAMTests"
)

target_sources(PedaleiraNAM_Tests PRIVATE
  RunTests.cpp
  SignalGraphTests.cpp
  DeferredReclaimerTests.cpp
  ParameterManagerTests.cpp
  GateProcessorTests.cpp
  CompressorProcessorTests.cpp
  OverdriveProcessorTests.cpp
  NAMProcessorTests.cpp
  DelayProcessorTests.cpp
  TapeDelayProcessorTests.cpp
  ReverbProcessorTests.cpp
  SpringReverbProcessorTests.cpp
  PingPongDelayProcessorTests.cpp
  ReverseDelayProcessorTests.cpp
  HoldProcessorTests.cpp
  RoutingGraphTests.cpp
  ../Source/EffectRegistry.cpp
  ../Source/Engine/SignalGraph.cpp
  ../Source/Engine/ParameterManager.cpp
  ../Source/Effects/GateProcessor.cpp
  ../Source/Effects/CompressorProcessor.cpp
  ../Source/Effects/OverdriveProcessor.cpp
  ../Source/Effects/NAMProcessor.cpp
  ../Source/Effects/IRLoaderProcessor.cpp
  ../Source/Effects/DelayProcessor.cpp
  ../Source/Effects/TapeDelayProcessor.cpp
  ../Source/Effects/ReverbProcessor.cpp
  ../Source/Effects/SpringReverbProcessor.cpp
  ../Source/Effects/PingPongDelayProcessor.cpp
  ../Source/Effects/ReverseDelayProcessor.cpp
  ../Source/Effects/HoldProcessor.cpp
  ../Source/UI/RoutingGraph.cpp
)

target_include_directories(PedaleiraNAM_Tests PRIVATE ../Source)

target_compile_definitions(PedaleiraNAM_Tests PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  PEDALEIRA_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(PedaleiraNAM_Tests PRIVATE
  juce::juce_audio_basics
  juce::juce_audio_processors
  juce::juce_core
  juce::juce_dsp
  juce::juce_events
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
  nam_core
  PedaleiraNAM_Icons
)

add_test(NAME PedaleiraNAM_UnitTests COMMAND PedaleiraNAM_Tests)
