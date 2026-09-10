# AGENT.md — guide for agents working in this repository

This file exists to give context to an AI agent (Claude Code or otherwise) opening this repository without having been part of the earlier decisions. Read this before touching any code.

The source of truth for the architecture is [`ARCHITECTURE.md`](./ARCHITECTURE.md) — this file is an operational summary, not a duplicate. If the two disagree, `ARCHITECTURE.md` wins, and this file should be corrected.

---

## What this project is

A digital guitar processor/pedalboard for live use: hardware + software, C++/JUCE, with Neural Amp Modeling (NAM), pedals, cab/IR, and TONE3000 integration. The goal is a real physical product, not a demo prototype.

**Two-legged development strategy:**
1. Everything (Phases 0–5) is written and validated on a **regular x86 Linux PC**, with native JUCE over ALSA/PipeWire.
2. Only in **Phase 6** does the same code get ported to the target hardware: **Radxa Cubie A7S** (Allwinner A733), with a documented fallback to Raspberry Pi 5 / Orange Pi 5 Plus in case the A733 bring-up doesn't mature in time.

This means: **no Phase 0–5 code should assume any specific ARM hardware.** If you're an agent writing code in that phase and you feel the urge to optimize for a specific SoC, stop — that belongs to Phase 6.

## The rule that is not up for negotiation: the realtime boundary

There are two conceptual threads in the system, and communication between them is strictly one-directional through allowed primitives only:

- **Audio thread (realtime):** guitar → input → `AudioEngine` → `SignalGraph` → output. Processes in fixed-size blocks, with a microsecond-scale time budget.
- **Control/network thread:** UI, `PresetManager`, `Tone3000Manager`, `ModelRepository`. Can take as long as it needs.

**The audio thread never:**
- allocates memory (`new`/`malloc`) outside of `prepare()`
- takes a blocking mutex
- performs network I/O, disk I/O, or file logging
- calls anything with non-deterministic latency (exceptions, dynamic RTTI, strings)
- waits on a response from the control thread

The bridge between the two threads is always one of these three primitives — never anything else:
1. **Atomic pointer swap** — for NAM models and complete signal graphs
2. **Lock-free SPSC queue** — for parameters and telemetry (CPU%, clipping)
3. **Double buffering** — for swapping whole presets

If a code change introduces any blocking call inside `AudioEngine::process()` or any `EffectProcessor::process()`, that's an architecture bug, not an implementation detail — reject it or fix it before moving on.

## Directory layout (see `ARCHITECTURE.md` section C for the full contract)

```
Source/
├── Engine/       # AudioEngine, SignalGraph, ParameterManager — the realtime boundary lives here
├── Effects/      # EffectProcessor (base class) + every pedal/NAM/cab as an independent subclass
├── Models/       # ModelRepository, ModelValidator, ModelLoader — nothing here ever runs on the audio thread
├── Tone3000/     # Tone3000Manager — OPTIONAL, pluggable module; the product works without it
├── Presets/      # PresetManager — serialization + double-buffered swapping
└── UI/           # only from Phase 7 onward — reads state via a lock-free FIFO, never calls the Engine directly
```

Every new effect (pedal, modulation, delay, reverb, pitch, whatever) is a new `EffectProcessor` subclass in `Effects/`, registered in `EffectRegistry`. Don't create special cases in `SignalGraph` for specific effect types — the graph doesn't know (and shouldn't know) the difference between a `GateProcessor` and a `NAMProcessor`.

## Roadmap status (see `ARCHITECTURE.md` section F)

| Phase | Status | Hardware |
|---|---|---|
| 0 — Architecture + JUCE/CMake skeleton | **done** | PC x86 |
| 1 — Audio Engine + Pedals + NAM + Cab/IR + TONE3000 + Presets | **in progress** (Gate/Compressor/Overdrive + NAMProcessor done; Cab/IR, TONE3000, Presets not started) | PC x86 |
| 2 — Delay + Reverb | not started | PC x86 |
| 3 — Modulation | not started | PC x86 |
| 4 — Pitch | not started | PC x86 |
| 5 — Looper, tuner, MIDI, advanced routing | not started | PC x86 |
| 6 — Port to the Radxa Cubie A7S | not started | Cubie A7S |
| 7 — Full touch UI | not started | Cubie A7S + touchscreen |
| 8–10 — Footswitches, PCB, final validation | not started | final hardware |

Update this table when a phase is completed — don't let it silently go stale.

## Decisions already made (don't reopen without a new reason)

- **NAM engine:** `NeuralAmpModelerCore` (sdatkinson, upstream, MIT) directly — **not** `NeuralAudio` as originally planned. Reason for the change: `NeuralAudio`'s CMake hardcodes relative `../deps/...` paths populated only via git submodules, which isn't FetchContent-friendly, and its bundled test models are CC BY-NC-ND (not usable in a commercial product's test suite anyway). `NeuralAmpModelerCore` ships no CMake library target of its own either (see `cmake/NAMCore.cmake` for how `nam_core` is built from its sources), but integrates cleanly and its `example_models/lstm.nam` is MIT. **Do not use AIDA-X or GuitarML code directly** — both GPL-3.0, license contamination in a closed-source product. Use them only as architectural reference, if needed.
- **`nam_core` must be a CMake `OBJECT` library, never `STATIC`.** NAM registers each architecture (LSTM, WaveNet, ...) into `get_dsp()`'s factory via a static-initializer side effect that no other code directly references by symbol — a real `.a` archive silently drops those translation units at link time (confirmed: `get_dsp()` threw "No config parser registered for architecture: LSTM" until this was fixed). `OBJECT` passes every `.o` straight into the final link with no archive pruning.
- **`nam_core`'s Eigen dependency is pinned to an exact commit, not a tag** — see `cmake/NAMCore.cmake`. The obvious `3.4.1` release tag is missing `Eigen::placeholders::lastN`, which `NAM/lstm.h` needs; the pinned commit is the exact one NeuralAmpModelerCore's own `Dependencies/eigen` submodule vendors (checked via the GitHub API, not guessed). Don't "clean this up" to a tagged release without re-verifying the symbol exists.
- **NPU:** don't count on it for NAM. RKNN/eIQ/VIP9000 have no confirmed support for causal/dilated Conv1D — all NAM processing is CPU-bound (NEON where available).
- **IR convolution:** partitioned, via `juce::dsp::Convolution` as a starting point.
- **TONE3000:** integrate via the official API (OAuth2+PKCE), but as an **optional** module under `Tone3000/` — commercial use on embedded hardware still has no contractual confirmation (see risks in `ARCHITECTURE.md` section I). Don't couple any core product feature to this integration.
- **Phase 1 default sample rate/buffer:** 48 kHz, 128-sample block (~2.7 ms) — tightening as profiling allows, final round-trip target < 10 ms.

## Code conventions

- C++/JUCE, CMake as the build system.
- No comments explaining the obvious. Only comment the non-obvious reason (e.g. why a lock-free queue has that specific size, why an unsafe cast is actually safe here).
- Every effect class implements the full `EffectProcessor` contract (see `ARCHITECTURE.md` section C) — no partial exceptions.
- Isolated per-processor benchmark tests live in `Tests/`, not mixed in with functional tests.

## Where pending issues and risks live

Don't re-derive the risk analysis here — it lives in `ARCHITECTURE.md` section I and is maintained there. If you, as an agent, find a new risk during implementation, add it to the risk table in `ARCHITECTURE.md`, not to this file.
