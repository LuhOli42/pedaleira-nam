# UI

## Purpose
Owns: the dev-facing patchbay GUI — the routing graph and its canvas (`RoutingGraph`, `RoutingCanvas`, `EffectBlockComponent`), the parameter drawer (`ParameterPanel`), I/O routing tiles (`IOSelectorBlock`), TONE3000/preset/model dialogs, the overlay/card system (`OverlayHost`), look-and-feel (`PedaleiraLookAndFeel`), and `MainComponent` — the one persistent owner of every `EffectProcessor` in the live chain.
Does not own: audio processing itself (`Source/Engine`, `Source/Effects`) or what a preset's XML contains beyond `MainComponent::buildPresetXml`/`applyPresetXml` (storage mechanics are `Source/Presets`).

This is explicitly a **dev-facing chain builder pulled forward from Phase 7**, not the final touch UI — see root AGENTS.md roadmap. Read root AGENTS.md's "UI/UX Design Philosophy" section before touching layout/sizing here; it is binding on this directory, not background reading.

## Code Map

### Find It Fast
| Looking for... | Go to |
|----------------|-------|
| Top-level layout, chain ownership, add/remove/reorder logic | `MainComponent.{h,cpp}` |
| Minimum tap-target constant (48px) | `TouchSizing.h::touch::minTapTarget` — use from the start on any new interactive row |
| Card-stack overlay system (replaces OS windows) | `OverlayHost.{h,cpp}` |
| One chain tile (select/drag) | `EffectBlockComponent.{h,cpp}` |
| The signal topology itself (nodes/ports/cables, processing order, cycle rules) | `RoutingGraph.{h,cpp}` — pure data + graph algorithms, no GUI types |
| Lanes, ports, cables, drag-to-patch | `RoutingCanvas.{h,cpp}` |
| Per-block parameter drawer | `ParameterPanel.{h,cpp}` — also hosts contextual TONE3000 search, scoped to the selected block |
| App font (Sora, embedded) | `PedaleiraLookAndFeel.{h,cpp}`, `Assets/Fonts/` |
| Settings screen (despite the filename) | `Tone3000Panel.{h,cpp}` — general Settings, TONE3000 account is just its first section |
| Effect icon reference (categories/colours/glyphs) | `../../docs/icons/AGENT-icon-notes.md` — read before adding any icon |
| Bottom bar (tuner/BPM-tap/IN-OUT meters) | `FooterBar.{h,cpp}` — visual placeholder only, see its class doc comment before wiring up real DSP |

### Key Relationships
- `MainComponent` owns `chain` (`vector<unique_ptr<EffectProcessor>>`) and `blocks` (`OwnedArray<EffectBlockComponent>`) as two parallel arrays — any reorder must move both in lockstep (see Pitfalls).
- `UI/` → `EffectRegistry`/`AudioEngine`/`PresetManager`/`Tone3000Manager` (one-directional): UI drives these, never the reverse.
- All former OS-level popups now route through `OverlayHost` — `MainWindow` (in `Main.cpp`) is the only real top-level `DocumentWindow` left in the app.

## Public API
| Export | Used By | Change Impact |
|--------|---------|---------------|
| `touch::minTapTarget` | Every interactive row across this directory | Changing it changes every tap target app-wide — see root AGENTS.md, this is a hard floor, not a suggestion |
| `OverlayHost::pushOverlay/popOverlay` | Every dialog (login, search, model list, settings, presets) | Supports stacking (login can open a further card on top of itself) |
| `EffectProcessor::getAccentColour()`/`drawIcon()` | `EffectBlockComponent::paint()` | Generic dispatch — new effect types must implement both to render correctly in the chain (see `Source/Effects/AGENTS.md`) |

## Data Flow
```
MainComponent::addEffect() → registry.create() → chain.push_back() → blocks.add(EffectBlockComponent)
                                                          ↓
                                              rebuildSignalGraph() → AudioEngine::setSignalGraph()
                                                          (atomic swap, see Source/Engine/AGENTS.md)

Block tapped → selectBlock() → ParameterPanel shows that processor's knobs
Block dragged → handleBlockDragEnded() → chainColumns maps drop x/y → flat index →
                 OwnedArray::move(blocks) + std::rotate(chain) in lockstep → rebuildSignalGraph()
Block removed → moved into `graveyard` (NOT destroyed) until the old SignalGraph
                 referencing it is provably retired
```

## Decisions
| Decision | Why | Rejected |
|----------|-----|----------|
| Chain wraps into rows (≤4 visible), not horizontal scroll | Matches Quad Cortex Grid reference; drag-to-reorder now works in both dimensions | Single-row horizontal scroll (original design) |
| Parameter drawer sizes to actual content, floored at one knob row, capped at 45% window height | Fixed-fraction sizing clipped heavy processors and left pointless scrolling on light ones. Originally capped at 1/4, but that still cut a 2nd knob row's value readout off for most amp-style (5-6 knob) processors on touch, forcing a scroll for basic knob use — raised per explicit user feedback | Always-fixed-fraction drawer, and the original 1/4 cap |
| Knob cell 118x140 (was 110x126), diameter 90 (was 84), value textbox 90x26 (was 80x20) | Stage-readability pass 2026-09-10: needs to read from ~2m on a 10" display. Note this no longer mathematically guarantees 2 full knob rows fit without scrolling at the dev window's default 1280x800 (needs ~386px, window only has ~360px to give the drawer) -- fine today since no processor has more than 5 params and one row holds ~10 at this width, but a future processor with >10 params will need either a taller dev window or the drawer's cap revisited | Keeping the smaller pre-2026-09-10 knob geometry to preserve the 2-row guarantee |
| Removed blocks go to `graveyard`, not destroyed immediately | The just-superseded `SignalGraph` may still be mid-use on the audio thread and points at this processor | Immediate `delete` on removal |
| **Signal topology is a graph (`RoutingGraph`: nodes + ports + connections), not an ordered list.** Four lanes are visual rails only — a block's lane/column never affects the audio; `MainComponent::rebuildSignalGraph()` feeds `SignalGraph` from `RoutingGraph::processingOrder()` (a topological sort of the cables) | Per explicit user requirement 2026-09-11: "a ordem deve surgir naturalmente das conexões… eu conecto o áudio como se estivesse plugando cabos", NOT "eu escolho a posição de cada efeito em uma lista". Splits, merges and parallel paths are impossible to express as a list at all | The previous ordered grid, where a block's cell WAS its processing position (`gridSlot`); also rejected: four independent fixed chains (would be four presets, not a routing graph) |
| One cable per INPUT port, many per OUTPUT port | An output feeding several inputs is a split, which is the point; a second cable into one input would be an invisible implicit sum — summing is an explicit merge node you can see and click | Allowing multiple cables into an input and silently summing them |
| `connect()` refuses a cable that would close a loop (`canConnect` walks forward from the target first) | A feedback loop has no valid processing order — `processingOrder()` would silently drop the whole cycle, so the block that just got patched would go quiet with no explanation | Allowing the cycle and detecting it later at graph-build time |
| Inactive nodes/cables (not on a path from an audioInput to an audioOutput) are dimmed and left out of `SignalGraph` entirely | The whole stated priority is "olhar para a tela e entender imediatamente: por onde o áudio está passando?" — an unpatched block that still made sound would contradict what the canvas shows | Processing everything regardless of whether it reaches an output |
| Real parallel DSP (true split/merge in the audio engine) deliberately NOT built yet — parallel branches currently flatten into one serial order | The user staged it explicitly: "primeiro implemente a camada visual e o modelo de dados do grafo. Depois conectaremos esse grafo ao processamento de áudio real" | Rewriting `SignalGraph` for parallel buses in the same pass as the UI |
| `FooterBar` (tuner/BPM-tap-tempo/IN-OUT meters) pinned at the bottom, reserved before the parameter drawer's cap math | User wants the full intended screen (title + 4 rows + footer) visible from the start, matching the "always-configured" chain rows above. Explicitly VISUAL PLACEHOLDER ONLY — asked the user real-DSP-now vs. reserve-the-layout-now, they picked the latter; no pitch detection/level metering/tap-tempo logic exists yet (`Source/UI/FooterBar.h`, Phase 5 in the roadmap) | Building the real tuner/metering DSP now (would pull Phase 5 work forward and touch the realtime audio path ahead of schedule) |
| Window default bumped 1280x800 → 1280x850 | The footer's fixed height has to come out of the same window-height budget as everything else; without the bump the parameter drawer's no-scroll guarantee (see the knob-geometry decision above) would regress | Keeping 800 and shrinking the footer/drawer instead |
| Chain block HEIGHT is derived fresh in `resized()` from whatever's left after top bar/footer/drawer (`MainComponent::blockHeight`, a member, not the `constexpr` it used to be) -- block WIDTH stays fixed | The 4 rows used to sit packed at a fixed size at the top of the window with a large dead-black gap below them down to the footer; user wants the rows to always fill the full available height instead | Fixed block height with the leftover space just left empty (the original behaviour, corrected 2026-09-10) |
| Drawer's space reserved BEFORE the chain's in `resized()` (used to be the other way around) | Chain height is now derived from "whatever's left", so the drawer has to claim its share first or there'd be nothing left to derive from | Chain-first ordering (worked fine when block height was a fixed constant, incompatible with it becoming dynamic) |
| `topBarHeight` 76→92, `footerHeight` 64→92, `FooterBar`'s tuner redrawn as a horizontal center-zero gauge + note letter (was a plain label), IN/OUT meters redrawn as two stacked horizontal bars labelled directly on the bar (was two small vertical bars with a caption underneath) | Follow-up user feedback the same day (2026-09-10) after seeing the first pass: header/footer both asked to be more prominent, and the meter/tuner shapes corrected to match a hand-drawn mockup | Keeping the original vertical-bar meters and plain-text tuner (superseded same day) |
| Output routing = real hardware channel pairs, queried live | See `Source/Engine/AGENTS.md` — same decision, UI consequence is `IOSelectorBlock` never hardcodes a channel list | Abstract stereo/L/R selector |
| Every former OS popup → `OverlayHost` card | Standalone touchscreen device has no window manager; "open another window" isn't a pedalboard mental model | `juce::DialogWindow`/`DocumentWindow` per popup |

## Entry Points
| Task | Start Here |
|------|------------|
| Add a new dialog/overlay card | `OverlayHost::pushOverlay`, pattern-match `Tone3000SearchDialog` or `ModelListDialog` |
| Change chain layout/reorder logic | `MainComponent::layoutChain`/`handleBlockDragEnded` |
| Add a new icon for an effect | `EffectProcessor::drawIcon()` override + `docs/icons/AGENT-icon-notes.md` |
| Touch-size a new interactive row | `TouchSizing.h::touch::minTapTarget` |

## Contracts
- Every tappable element (button, menu row, list row, knob, selector tile) must be ≥48px (`touch::minTapTarget`) on both axes — enforced app-wide since commit `4fc5234` after touch sizing was initially applied only to newly-added menus.
- `chain` and `blocks` are parallel arrays that must move in lockstep on any reorder — `handleBlockDragEnded` uses `OwnedArray::move` for `blocks` and a hand-written `std::rotate` for `chain` (`std::vector` has no equivalent built-in) over the same index span.
- A removed processor is never destroyed synchronously — it goes to `graveyard` until the superseding `SignalGraph` is provably no longer referenced (see `Source/Engine/AGENTS.md`'s `DeferredReclaimer` pattern this mirrors).
- `EffectBlockComponent::paint()` must hand `drawIcon()` a properly centred *square*, not a stretched rectangle (fixed bug, commit `1dbdc21` — see `Source/Effects/AGENTS.md`).

## Patterns

### Adding a new touchscreen-facing row/menu
1. Use `touch::minTapTarget` from `TouchSizing.h` from the start — don't size for a mouse and revisit later.
2. `juce::PopupMenu` rows: `PopupMenu::Options().withStandardItemHeight(touch::minTapTarget)`.
3. `juce::ListBox` rows: `setRowHeight(touch::minTapTarget)`.
4. If it's a dialog/popup: host it via `OverlayHost::pushOverlay`, not a native `DialogWindow`.

### Adding a new effect block's visuals
1. Implement `EffectProcessor::getAccentColour()` and `drawIcon()` on the processor (in `Source/Effects/`, not here).
2. Add its category/glyph to `docs/icons/AGENT-icon-notes.md`.
3. `EffectBlockComponent` and `RoutingCanvas` need no changes — they dispatch generically through the base contract.

## Pitfalls
- `PopupMenu::Options::withStandardItemHeight()` doesn't reach `juce::ComboBox` dropdowns — those needed the popup menu *font* bumped separately to hit touch sizing (commit `ede104b`, `architectureBox`).
- The parameter drawer's height used to be computed to exactly match the knob grid's height with zero slack — fragile by construction, produced an unwanted scrollbar for typical (1-row) processors until `getPreferredContentHeight()` added a fixed margin (commit `cd04417`). Don't reintroduce zero-slack height math here.
- The main window and the TONE3000 dialog both had a real close-button bug (not user error): native/Wayland title bar forwarded through distrobox made the close button too easy to hit (main window) and apparently unclickable (TONE3000 dialog). Fixed by using JUCE-drawn window decorations instead of native ones on both — don't switch either back to native decorations without re-testing under the same Wayland/distrobox setup.
- `juce::FileChooser` still exists as one "Import from device" affordance — it is not a pattern to extend elsewhere; it won't exist on the final device at all (gear only ever arrives via TONE3000 search or a future companion app). See root AGENTS.md.

## Boundaries

### Always
- Size every new interactive element to `touch::minTapTarget` or larger.
- Route new dialogs/popups through `OverlayHost`.
- Move `chain` and `blocks` together on any reorder.

### Never
- Introduce a second top-level settings surface — everything goes through `Tone3000Panel` (the app's one Settings screen despite its filename).
- Destroy a processor synchronously on removal from the chain — always via `graveyard`.
- Use a native `juce::DialogWindow`/`DocumentWindow` for anything but `Main.cpp`'s `MainWindow`.
