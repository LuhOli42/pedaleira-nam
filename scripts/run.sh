#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# juce_add_gui_app names the output binary after PRODUCT_NAME (CMakeLists.txt).
exec "./build/OpenGuitarMultiFx_artefacts/Release/OpenGuitarMultiFx" "$@"
