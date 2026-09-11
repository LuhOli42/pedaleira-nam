# Effects

## Purpose
Owns: every stage of the signal chain — `EffectProcessor` (the base contract) and each concrete pedal/NAM/cab subclass (`GateProcessor`, `CompressorProcessor`, `OverdriveProcessor`, `NAMProcessor`, `IRLoaderProcessor`), plus `EffectRegistry` (factory + display-name mapping) at `Source/EffectRegistry.{h,cpp}`.
Does not own: chain ordering/execution (`Source/Engine/SignalGraph`), who owns processor instances or the UI around them (`Source/UI/MainComponent`), or realtime-swap machinery itself (`Source/Engine/DeferredReclaimer.h` — Effects only *uses* it).

## Code Map

### Find It Fast
| Looking for... | Go to |
|----------------|-------|
| The contract every processor implements | `EffectProcessor.h` |
| Factory (name → processor), display-name ↔ registry-key mapping | `Source/EffectRegistry.{h,cpp}` |
| NAM inference wrapper (3 roles: Amp/Amp+Cab/Pedal, same class) | `NAMProcessor.{h,cpp}` |
| Cab/Reverb convolution (2 roles, same class) | `IRLoaderProcessor.{h,cpp}` |
| Shared attack/release detector (used by Gate + Compressor) | `EnvelopeFollower.h` |
| Effect icon glyphs, category colours, which glyph each processor uses | `docs/icons/AGENT-icon-notes.md` (read before adding any new effect's icon) |

### Key Relationships
- `Effects/` → `Engine/DeferredReclaimer.h`: NAM models and IRs use the same atomic-swap pattern as `SignalGraph` itself.
- `SignalGraph`/`AudioEngine` never know concrete `Effects/` types — everything goes through `EffectRegistry`. Don't add special-casing for a specific effect type in `SignalGraph` (see root AGENTS.md).

## Public API
| Export | Used By | Change Impact |
|--------|---------|---------------|
| `EffectProcessor` (virtual contract) | Every concrete processor, `SignalGraph`, UI | Every subclass must implement the full contract — no partial exceptions (see root AGENTS.md code conventions) |
| `EffectRegistry::registerType/create/displayNameForKey/keyForDisplayName` | `MainComponent` (Add-effect menu, preset save/load) | `displayNameForKey`/`keyForDisplayName` are built once by *probing* a real instance's `getName()` at `registerType()` time — this is the single source of truth for the Add-effect menu AND preset serialization, so they can't drift apart. Don't hardcode display names elsewhere. |
| `EffectProcessor::wantsModelFile()`/`loadModelFile()` | Generic UI "Load model..." action | False/no-op default; true only for NAM/IR processors — lets UI code avoid `dynamic_cast` |

## Entry Points
| Task | Start Here |
|------|------------|
| Add a new effect processor | See "Adding a new effect processor" under Patterns below |
| Understand the base contract | `EffectProcessor.h` |
| Wire a processor into TONE3000 download routing | `../Tone3000/GearRouting.h` (see `Source/Tone3000/AGENTS.md`) |

## Decisions
| Decision | Why | Rejected |
|----------|-----|----------|
| One `NAMProcessor` class fills 3 chain roles (Neural Amp / Neural Amp + Cab / Neural Pedal) | Inference engine doesn't care which — only the loaded `.nam` file differs; split is purely so TONE3000 search stays scoped to one gear at a time | 3 separate processor classes |
| One `IRLoaderProcessor` class fills 2 roles (Cab / Reverb) | Same convolution operation underneath; only typical IR length and chain position differ | 2 separate processor classes |
| `nam_core` built as a CMake `OBJECT` library, never `STATIC` | NAM registers each architecture (LSTM, WaveNet...) into `get_dsp()`'s factory via static-initializer side effects with no directly-referenced symbol — a real `.a` archive silently dropped those translation units at link time (confirmed: `get_dsp()` threw "No config parser registered for architecture: LSTM") | `STATIC` library |
| Eigen pinned to an exact commit (not the `3.4.1` tag) | The tagged release is missing `Eigen::placeholders::lastN`, which `NAM/lstm.h` needs; pinned commit is the exact one NAM's own submodule vendors (verified via GitHub API) | The obvious tagged release |
| `NAMProcessor`/`IRLoaderProcessor` override `getState()`/`setState()` | Base class only serializes float params; presets need to restore *which* model/IR is loaded, not just gain (fixed in commit `921e0e9` — previously a preset couldn't actually restore the loaded sound) | Relying on the base class default |
| `drawIcon()` glyphs match the user's reference sheet exactly, re-checked per icon rather than assumed | Two earlier passes guessed shapes from memory/description and got them wrong in both directions (chip-vs-amp-head for NAM roles, distinct-vs-shared for the same 3 roles) before a 2026-09-11 pass read the actual image directly and got user approval on a full 68-glyph prototype first — see `docs/icons/AGENT-icon-notes.md` | Inventing a consistent-looking icon language without the reference |

## Contracts
- Every effect class implements the *full* `EffectProcessor` contract — no partial exceptions (see root AGENTS.md).
- `process()` never allocates, blocks, or does I/O — model/IR loading always happens in `prepare()`/`loadModel()`/`loadImpulseResponse()` on the control thread, then handed to the audio thread via `DeferredReclaimer`.
- `nam::DSP::process()` output saturates to exactly `1.0f` in float32 at extreme drive — a bound test must use `<=`, not `<` (real bug, commit `dc090fe`).
- `juce::AudioProcessorParameterGroup`'s constructor is variadic-template (each `unique_ptr<Parameter>` as a separate argument), not a vector-taking constructor.
- `format` is the load-bearing TONE3000 field, not `gear`: `"nam"` → `NAMProcessor` only, `"ir"` → `IRLoaderProcessor` only. Mixing these up is a routing bug (see `Source/Tone3000/AGENTS.md`), not a matter of taste.

## Patterns

### Adding a new effect processor
1. Subclass `EffectProcessor`, implement the full contract (`prepare`/`process`/`reset`/`getParameters`/`getName`; override `getState`/`setState` only if state is more than float params).
2. Register it in `EffectRegistry::registerBuiltInEffects` (`Source/EffectRegistry.cpp`) — this is the one place concrete types are known.
3. Add its glyph to `docs/icons/AGENT-icon-notes.md` and implement `drawIcon()`/`getAccentColour()` to match the shared reference sheet — don't invent an ad-hoc glyph (see root AGENTS.md UI/UX Design Philosophy).
4. If it needs a loaded file (model/IR), override `wantsModelFile()`→true and `loadModelFile()`, and add a route in `Source/Tone3000/GearRouting.h` if it should be reachable via TONE3000 search.

## Pitfalls
- `EffectBlockComponent::paint()` used to hand `drawIcon()` a *stretched* (non-square) rectangle because width/height were reduced by different factors on a non-square block — every icon lacking a defensive `jmin(width,height)` came out visibly squashed until fixed in commit `1dbdc21`. New `drawIcon()` overrides can rely on the caller now passing a proper centred square, but don't assume every historical caller does.
- "Neural Amp" and "Neural Amp + Cab" process *identically* (same `.nam` file → `nam::get_dsp()`) — they look like they should be different code paths but are the exact same one; the split exists only for TONE3000 search scoping.
- Renaming a chain-role display name (e.g. the "NAM Amp"/"Neural Drive" → "Neural Amp"/"Neural Pedal" rename in commit `8d4316a`) requires updating every place that pattern-matches on the old name in lockstep — `GearRouting.h` and `MainComponent` both string-match on `getName()`; these are not meant to drift independently.

## Boundaries

### Always
- Implement the full `EffectProcessor` contract on every new subclass.
- Route new NAM/IR file types through `GearRouting.h` if TONE3000-downloadable.

### Never
- Add effect-type-specific special cases to `SignalGraph` — it must stay ignorant of concrete types.
- Call `loadModel()`/`loadImpulseResponse()` from the audio thread — control thread only, both mutate in place.
