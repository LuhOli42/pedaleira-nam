#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

if command -v distrobox >/dev/null 2>&1 && distrobox list 2>/dev/null | grep -q juce-dev; then
  distrobox enter juce-dev -- bash -c "cd '$(pwd)' && ctest --test-dir build --output-on-failure"
else
  ctest --test-dir build --output-on-failure
fi
