#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# The binary filename has a space (PRODUCT_NAME "Pedaleira NAM" in
# CMakeLists.txt) -- juce_add_gui_app names the output after PRODUCT_NAME,
# unlike juce_add_console_app which used the CMake target name (no space).
exec "./build/PedaleiraNAM_artefacts/Release/Pedaleira NAM" "$@"
