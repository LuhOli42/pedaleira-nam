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

# Sora (SIL OFL, see Assets/Fonts/SORA-LICENSE.txt), embedded rather than
# relying on the OS having a decent font installed -- the final target is a
# from-scratch embedded Linux build with no guaranteed font at all. Static
# weights, not the variable font: JUCE's own font engine doesn't reliably
# expose a variable font's weight axis, but it renders a real bold/extrabold
# file exactly as designed. Switched from Inter to Sora per user request
# (2026-09-10) -- Inter read as generic/default, Sora's geometric shapes and
# distinctive rounded terminals read more as a purpose-built hardware/gear
# product than a stock UI font.
juce_add_binary_data(PedaleiraNAM_Fonts
  SOURCES
    Assets/Fonts/Sora-Regular.ttf
    Assets/Fonts/Sora-Bold.ttf
    Assets/Fonts/Sora-ExtraBold.ttf
)

# Effect block icons -- embedded SVGs, not hand-transcribed juce::Path calls.
# The first attempt at drawIcon() reimplemented each glyph's geometry by eye
# from the approved reference-sheet prototype and drifted from it in several
# ways per icon (stroke width, inset ratios, pin lengths, filled vs. outline
# circles) -- see docs/icons/AGENT-icon-notes.md. Embedding the literal
# approved SVG and rendering it with juce::Drawable guarantees the on-screen
# glyph is pixel-identical to what got approved, the same reasoning as
# embedding Sora above instead of hoping the OS font looks similar enough.
juce_add_binary_data(PedaleiraNAM_Icons
  NAMESPACE IconData
  HEADER_NAME IconData.h
  SOURCES
    Assets/Icons/gate.svg
    Assets/Icons/pulse.svg
    Assets/Icons/overdrive.svg
    Assets/Icons/neura_chip.svg
    Assets/Icons/neura_chip_cab.svg
    Assets/Icons/cab.svg
    Assets/Icons/hall.svg
    Assets/Icons/digital_delay.svg
    Assets/Icons/tape_delay.svg
    Assets/Icons/ambient.svg
    Assets/Icons/spring.svg
)

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
  Source/Effects/IRLoaderProcessor.cpp
  Source/Effects/DelayProcessor.cpp
  Source/Effects/TapeDelayProcessor.cpp
  Source/Effects/ReverbProcessor.cpp
  Source/Effects/SpringReverbProcessor.cpp
  Source/UI/ChainContainer.cpp
  Source/UI/EffectBlockComponent.cpp
  Source/UI/FooterBar.cpp
  Source/UI/IOSelectorBlock.cpp
  Source/UI/ModelListDialog.cpp
  Source/UI/OverlayHost.cpp
  Source/UI/ParameterPanel.cpp
  Source/UI/MainComponent.cpp
  Source/UI/PedaleiraLookAndFeel.cpp
  Source/UI/PresetBadge.cpp
  Source/UI/PresetListDialog.cpp
  Source/UI/Tone3000Panel.cpp
  Source/UI/Tone3000SearchDialog.cpp
  Source/UI/OAuthLoginDialog.cpp
  Source/Presets/PresetManager.cpp
  Source/Tone3000/Tone3000Manager.cpp
)

target_include_directories(PedaleiraNAM PRIVATE Source)

# JUCE_WEB_BROWSER=1: TONE3000 login renders in an embedded WebBrowserComponent
# (WebKitGTK on Linux, already available -- webkit2gtk-4.1 was found during the
# juceaide configure step regardless), never a separate system browser process.
# Curl is needed for TONE3000's REST calls (Tone3000Manager).
target_compile_definitions(PedaleiraNAM PRIVATE
  JUCE_WEB_BROWSER=1
  JUCE_USE_CURL=1
)

# JUCE_USE_CURL only makes juce_core.cpp call into libcurl -- it doesn't link
# it. Without this, the link fails with undefined references to curl_*.
find_package(CURL REQUIRED)

# juce_gui_extra.cpp needs GTK/WebKitGTK headers for WebBrowserComponent on
# Linux. JUCE's CMake resolves these for its own juceaide helper build, but
# not for consumer targets -- without this, compiling juce_gui_extra.cpp
# fails with "gtk/gtk.h: No such file or directory". Same pattern as the
# CURL::libcurl fix above: ask pkg-config directly rather than assume JUCE
# wires it up.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
pkg_check_modules(WEBKIT2GTK REQUIRED IMPORTED_TARGET webkit2gtk-4.1)

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
  juce::juce_cryptography
  juce::juce_dsp
  juce::juce_events
  juce::juce_gui_basics
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
  nam_core
  PedaleiraNAM_Fonts
  PedaleiraNAM_Icons
  CURL::libcurl
  PkgConfig::GTK3
  PkgConfig::WEBKIT2GTK
)

enable_testing()
add_subdirectory(Tests)
