# NAM Pedalboard

A digital guitar processor for live use — realtime audio engine in C++/JUCE, Neural Amp Modeling, and TONE3000 integration.

- **Full architecture:** [`ARCHITECTURE.md`](./ARCHITECTURE.md)
- **Guide for AI agents working in this repo:** [`AGENT.md`](./AGENT.md)
- **Current phase:** 0 — Audio Engine + JUCE/CMake skeleton, running on an x86 PC. The final hardware (Radxa Cubie A7S) only enters at Phase 6.

## Build

Prerequisites: CMake ≥ 3.22, a C++20 compiler (GCC/Clang), and JUCE's Linux dev libraries (ALSA, X11, FreeType, fontconfig, curl). On regular distros (Ubuntu/Debian/Fedora) install these through the normal package manager. On immutable distros (Bazzite/Fedora Atomic/Silverblue), use a [distrobox](https://distrobox.it) container with a full Fedora/Ubuntu image — that's what this repo assumes by default; see `scripts/build.sh` for the container name it expects.

```sh
./scripts/build.sh   # configure + build (first run fetches JUCE via FetchContent)
./scripts/test.sh    # run the unit tests (juce::UnitTest)
./scripts/run.sh      # run the app -- opens the default audio device and stays in passthrough
```

The Phase 0 binary does nothing beyond opening the default audio device and passing the signal straight through (empty graph = total bypass). It's proof that the realtime boundary (`AudioEngine` → `SignalGraph` → device) works end to end before any real effect exists (Phase 1).

## Layout

```
Source/
├── Engine/       # AudioEngine, SignalGraph, ParameterManager, DeferredReclaimer
├── Effects/      # EffectProcessor (base contract) -- concrete pedals arrive in Phase 1
├── EffectRegistry.{h,cpp}
└── Main.cpp
Tests/            # juce::UnitTest -- SignalGraph, DeferredReclaimer, ParameterManager
scripts/          # build.sh / test.sh / run.sh
```

Details on each module, decisions already made, and the full roadmap live in `ARCHITECTURE.md`.
