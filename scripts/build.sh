#!/usr/bin/env bash
# Phase 0 build on the dev PC (see ARCHITECTURE.md / AGENT.md).
#
# This project is set up on an immutable Linux distro (Bazzite/Fedora
# Atomic), so cmake and JUCE's Linux dev libraries (ALSA, X11, FreeType,
# fontconfig, curl, webkit2gtk -- required transitively even for a
# console-only app, because JUCE's build-time "juceaide" helper always
# compiles juce_gui_basics) come from a distrobox container instead of the
# host package manager. If your machine already has these libs through
# apt/dnf normally, just run the cmake/cmake --build lines directly and
# skip the container wrapper below.
set -euo pipefail

cd "$(dirname "$0")/.."

if command -v distrobox >/dev/null 2>&1 && distrobox list 2>/dev/null | grep -q juce-dev; then
  distrobox enter juce-dev -- bash -c "cd '$(pwd)' && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel"
else
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --parallel
fi
