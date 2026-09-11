# Engine

## Purpose
Owns: the realtime boundary itself — `AudioEngine` (device I/O callback), `SignalGraph` (ordered chain execution), `ParameterManager` (control→audio parameter delivery), `DeferredReclaimer` (the atomic-swap primitive everything else in the project builds on).
Does not own: what the chain contains (see `Source/Effects`) or who builds/owns processor instances (see `Source/UI/MainComponent`) — `SignalGraph` only holds non-owning pointers.

## Code Map

### Find It Fast
| Looking for... | Go to |
|----------------|-------|
| Audio device callback, block loop | `AudioEngine.cpp::audioDeviceIOCallbackWithContext` |
| Input/output channel selection | `AudioEngine.h` — `setInputChannel`/`setOutputChannelPair`, both plain atomics, no graph rebuild |
| The atomic-swap pattern used everywhere (graphs, NAM models, IRs) | `DeferredReclaimer.h` |
| Control→audio parameter updates | `ParameterManager.h` (SPSC lock-free queue) |
| Chain ordering/execution | `SignalGraph.h` |

### Key Relationships
- `AudioEngine` owns one `DeferredReclaimer<SignalGraph>`; `SignalGraph` owns non-owning `EffectProcessor*` pointers into whatever the UI's `chain` (`std::vector<std::unique_ptr<EffectProcessor>>`) keeps alive.
- `Effects/` → `Engine/`: effect processors depend on `DeferredReclaimer.h` for their own internal model/IR swaps (NAM models, IRs) — same primitive, different payload.
- Never the reverse: `Engine/` must not depend on concrete `Effects/` types.

## Public API
| Export | Used By | Change Impact |
|--------|---------|---------------|
| `AudioEngine::setSignalGraph()` | `MainComponent::rebuildSignalGraph()` | The only way to change what's playing; control thread only |
| `AudioEngine::getAvailable{Input,Output}...Names()` | UI I/O selectors | Must stay live-queried, never cached (see Contracts) |
| `DeferredReclaimer<T>` | `Effects/NAMProcessor`, `Effects/IRLoaderProcessor`, `AudioEngine` | Template — any realtime-boundary swap should reuse this, not reinvent it |
| `ParameterManager::push()`/`drain()` | Not yet wired into a processor (infra exists since Phase 0, consumers are Phase 2+) | |

## Entry Points
| Task | Start Here |
|------|------------|
| Add a new realtime-swappable resource | `DeferredReclaimer.h`, see Patterns below |
| Change audio device I/O / channel routing | `AudioEngine.{h,cpp}` |
| Wire a new control→audio parameter path | `ParameterManager.h` |

## Decisions
| Decision | Why | Rejected |
|----------|-----|----------|
| `DeferredReclaimer<T>` (atomic pointer swap + deferred delete) over `std::atomic<std::shared_ptr<T>>` | Common stdlib impls of the latter aren't actually lock-free (fall back to internal mutex) | `shared_ptr`-based swap |
| Old object freed later by a control-thread sweep (~500ms margin), never inline in `exchange()` | Audio thread may still be mid-`process()` holding the old pointer; deleting there is use-after-free | Deleting synchronously in `publish()` |
| `SignalGraph` doesn't own processors | Lets the UI add/remove one block without resetting every other block's params/loaded model | `SignalGraph` owning `unique_ptr<EffectProcessor>` |
| Output routing = real hardware channel pairs ("Out 1/2", "Out 3/4"), queried live from the device | An abstract stereo/L/R concept doesn't scale past 2 outputs (this project targets a 6-channel interface) | Fixed stereo/mono/left/right enum |

## Contracts
- The audio thread never allocates (`new`/`malloc`) outside `prepare()`, never takes a blocking mutex, never does I/O, never calls anything non-deterministic-latency (exceptions, dynamic RTTI, string allocation), never waits on the control thread. Violating this inside `AudioEngine::process()` or any `EffectProcessor::process()` is an architecture bug (see root AGENTS.md).
- `DeferredReclaimer::currentRaw()` is the *only* access point the audio thread is allowed to call — a single atomic load, no allocation.
- `getAvailableInputChannelNames()`/`getAvailableOutputPairNames()` must re-query the currently-open device every call, never return a cached/hardcoded list — PipeWire's generic ALSA passthrough device reported a placeholder 128/128 channel count that made I/O selectors look "infinite" until this was fixed live (commit `783588c`); actual routing was never affected.
- `sweep()` must be called periodically from the *control* thread only (a `juce::Timer` at 10–20Hz is the existing pattern) with a safety margin generously larger than one audio block.

## Patterns

### Adding a new realtime-boundary swap (new kind of hot-swappable state)
1. Wrap the payload type in `DeferredReclaimer<T>`.
2. Build/prepare the new `T` entirely on the control thread.
3. `publish()` it — audio thread picks it up via `currentRaw()` on its next block, no lock.
4. Add a `juce::Timer` (or reuse an existing one) calling `sweep()` at 10–20Hz from the control thread.

## Pitfalls
- `ParameterManager`'s queue silently *drops the newest change* when full instead of blocking or growing — this is intentional (stalling the audio thread is worse than losing one update), not a bug to "fix" by growing the buffer.
- A `STATIC` (archive) library that registers types via static-initializer side effects (no directly-referenced symbol) gets those translation units silently dropped at link time by the linker's archive pruning. This bit `nam_core` (see `Effects/AGENTS.md`) — the general lesson lives here because it's a linking/build concern, not specific to NAM: if you add another self-registering factory anywhere in this project, make sure its target is `OBJECT`, not `STATIC`.
- `AudioEngine` actively tries to switch off the OS's generic/placeholder audio device at startup, in favor of a real interface — don't assume `deviceManager`'s initial state reflects the real hardware.

## Boundaries

### Never
- Add a blocking call (mutex, I/O, allocation, exception, dynamic RTTI) inside `AudioEngine::process()`'s call path.
- Delete a `DeferredReclaimer`-managed object anywhere but `sweep()`, from the control thread.
- Cache `getAvailableInputChannelNames()`/`getAvailableOutputPairNames()` results — always re-query.
