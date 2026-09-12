juce_add_console_app(OpenGuitarMultiFx_Tests
  PRODUCT_NAME "OpenGuitarMultiFx Tests"
  BUNDLE_ID "com.openguitarmultifx.OpenGuitarMultiFxTests"
)

target_sources(OpenGuitarMultiFx_Tests PRIVATE
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
  HallReverbProcessorTests.cpp
  PlateReverbProcessorTests.cpp
  RoomReverbProcessorTests.cpp
  ShimmerReverbProcessorTests.cpp
  GatedReverbProcessorTests.cpp
  AnalogDelayProcessorTests.cpp
  DualDelayProcessorTests.cpp
  MultiTapDelayProcessorTests.cpp
  PingPongDelayProcessorTests.cpp
  ReverseDelayProcessorTests.cpp
  HoldProcessorTests.cpp
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
  ../Source/Effects/HallReverbProcessor.cpp
  ../Source/Effects/PlateReverbProcessor.cpp
  ../Source/Effects/RoomReverbProcessor.cpp
  ../Source/Effects/ShimmerReverbProcessor.cpp
  ../Source/Effects/GatedReverbProcessor.cpp
  ../Source/Effects/AnalogDelayProcessor.cpp
  ../Source/Effects/DualDelayProcessor.cpp
  ../Source/Effects/MultiTapDelayProcessor.cpp
  ../Source/Effects/PingPongDelayProcessor.cpp
  ../Source/Effects/ReverseDelayProcessor.cpp
  ../Source/Effects/HoldProcessor.cpp
)

target_include_directories(OpenGuitarMultiFx_Tests PRIVATE ../Source)

target_compile_definitions(OpenGuitarMultiFx_Tests PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  OGMFX_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(OpenGuitarMultiFx_Tests PRIVATE
  juce::juce_audio_basics
  juce::juce_audio_processors
  juce::juce_core
  juce::juce_dsp
  juce::juce_events
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
  nam_core
  OpenGuitarMultiFx_Icons
)

add_test(NAME OpenGuitarMultiFx_UnitTests COMMAND OpenGuitarMultiFx_Tests)
