#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
exec ./build/PedaleiraNAM_artefacts/Release/PedaleiraNAM "$@"
