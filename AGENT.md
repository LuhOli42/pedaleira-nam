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
└── UI/           # dev-facing chain builder exists now (pulled forward from Phase 7); touch-optimized polish, drag-to-reorder, and split/merge routing are still Phase 7 work
```

Every new effect (pedal, modulation, delay, reverb, pitch, whatever) is a new `EffectProcessor` subclass in `Effects/`, registered in `EffectRegistry`. Don't create special cases in `SignalGraph` for specific effect types — the graph doesn't know (and shouldn't know) the difference between a `GateProcessor` and a `NAMProcessor`.

## Roadmap status (see `ARCHITECTURE.md` section F)

| Phase | Status | Hardware |
|---|---|---|
| 0 — Architecture + JUCE/CMake skeleton | **done** | PC x86 |
| 1 — Audio Engine + Pedals + NAM + Cab/IR + TONE3000 + Presets | **in progress** (Gate/Compressor/Overdrive + NAMProcessor + a dev GUI + Tone3000Manager done; Cab/IR and Presets not started) | PC x86 |
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

## UI/UX Design Philosophy (don't reopen without a new reason)

This is a standalone hardware pedalboard, not a desktop app that happens to
run on a PC first. Every screen has to make sense on a browserless
touchscreen with no window manager and no filesystem the player is ever
meant to think about. Concretely, until Phase 7 replaces this dev GUI:

- **No separate OS windows for anything but the app itself.** Login,
  search, "browse installed gear" — all of it is a card drawn on top of
  the single app window via `Source/UI/OverlayHost.h`, not a
  `juce::DialogWindow`/`DocumentWindow`. `OverlayHost` supports stacking
  (a card can open a further card on top of itself, e.g. TONE3000 login
  opening its embedded browser) and dismisses the top card on an
  outside tap, mobile-bottom-sheet style. The only real top-level
  `DocumentWindow` in the whole app is `Main.cpp`'s `MainWindow` — that's
  the actual app window and can't be anything else.
- **No native file-open dialogs as the primary flow.** Picking an
  installed model/IR is an in-app list (`Source/UI/ModelListDialog.h`),
  not a folder browser — tap a row, it loads. An empty list is a normal
  state with plain text ("Nothing installed yet"), not an error or a
  blank menu. A native `juce::FileChooser` still exists as a single small
  "Import from device" affordance for getting a file onto the PC
  prototype at all; it is not something the design should route through
  more than that one place, and it won't exist on the final device (gear
  only ever arrives via TONE3000 search or a future companion app).
- **The parameter/detail panel is a drawer, not a permanent toolbar.** It
  only appears once a block is selected, and even then it's capped at
  1/4 of the window height (`MainComponent::resized()`), never filling
  whatever space is left like a desktop inspector panel. With nothing
  selected it renders nothing at all — no title, no hint text, just the
  flat background.
- **Effect blocks are black by default with a strong outline in the
  block's own category colour** (`EffectProcessor::getAccentColour()`),
  not a solid colour fill — see `EffectBlockComponent::paint()`. The one
  block currently open in the detail panel gets a light glaze of that
  same colour so it's visually obvious which block the panel belongs to.
  Bypassed blocks are flat grey, no category colour at all. Icons stay
  white regardless of state (see the Quad Cortex Grid reference this is
  modelled on).
- **I/O selectors only ever list what the live hardware actually has.**
  `AudioEngine::getAvailableInputChannelNames()` /
  `getAvailableOutputPairNames()` read the currently-open device every
  call — never a cached, hardcoded, or dev-machine-specific list. Output
  routing is a choice of real channel *pairs* ("Out 1/2", "Out 3/4", ...),
  not an abstract "stereo/left/right" concept that doesn't scale to an
  interface with more than 2 outputs.
- **The top bar reserves a preset-number slot** (`MainComponent`'s
  `presetBadge`) even though presets aren't implemented yet (Phase 1
  still has them not-started) — so that work, when it happens, has a
  place to land instead of reshuffling the whole top bar again. It is a
  placeholder only; don't wire real behaviour to it without also
  implementing the preset system itself.
- **Settings live in exactly one place**, reached through the "..."
  button (`MainComponent::showSettingsPanel()`, `Source/UI/Tone3000Panel`
  despite the filename — it's the app's general Settings screen, TONE3000
  account is just its first section). Don't add a second top-level
  settings surface; add a new section to this one.

## Code conventions

- C++/JUCE, CMake as the build system.
- No comments explaining the obvious. Only comment the non-obvious reason (e.g. why a lock-free queue has that specific size, why an unsafe cast is actually safe here).
- Every effect class implements the full `EffectProcessor` contract (see `ARCHITECTURE.md` section C) — no partial exceptions.
- Isolated per-processor benchmark tests live in `Tests/`, not mixed in with functional tests.

## Where pending issues and risks live

Don't re-derive the risk analysis here — it lives in `ARCHITECTURE.md` section I and is maintained there. If you, as an agent, find a new risk during implementation, add it to the risk table in `ARCHITECTURE.md`, not to this file.
