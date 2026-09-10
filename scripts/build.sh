#!/usr/bin/env bash
# Phase 0/1 build on the dev PC (see ARCHITECTURE.md / AGENT.md).
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

# Cap parallel compile jobs. This dev machine has 16 CPU cores but only
# ~7.5GB of RAM -- an unbounded `cmake --build --parallel` spawns up to 16
# GCC processes at once compiling JUCE/Eigen/nam_core's heavy translation
# units, which overran RAM and hard-crashed the whole machine once already
# (2026-09-09, building the NAM engine integration). Override with
# `BUILD_JOBS=N ./scripts/build.sh` if you're building on different/beefier
# hardware.
BUILD_JOBS="${BUILD_JOBS:-2}"

if command -v distrobox >/dev/null 2>&1 && distrobox list 2>/dev/null | grep -q juce-dev; then
  distrobox enter juce-dev -- bash -c "cd '$(pwd)' && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel $BUILD_JOBS"
else
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --parallel "$BUILD_JOBS"
fi
