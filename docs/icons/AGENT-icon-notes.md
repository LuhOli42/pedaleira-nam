# Effect icon set — reference and rules

**Mandatory check before writing or reviewing any `drawIcon()`:** confirm
against `docs/icons/reference-sheet.png` (or the latest attachment the user
has shared) which glyph a given effect actually uses — never assume, guess,
or reuse a glyph from a "close enough" effect. If the reference file isn't
in this folder yet, or the sheet doesn't clearly cover the effect you're
implementing, stop and ask the user rather than transcribing from memory —
that's exactly how the Cab glyph below drifted from the real sheet.

The user supplied a reference sheet ("PEDALBOARD UI — Icones de Efeitos —
Padrao Unificado") of the icon set every effect block should eventually use:
one consistent line-art glyph per effect type, white on the block's own
category-coloured outline, no fill colour on the glyph itself. Its own
tagline says it best: **same size, same style, more music.**

The sheet was pasted inline in chat (twice now, 2026-09-10 and again
2026-09-11) and this agent has no tool that can save an inline image to
disk — only what a human explicitly attaches as a file makes it into the
repo as a real asset. **If you have the original PNG, drop it in this
folder (e.g. `docs/icons/reference-sheet.png`)** so future work can check
pixel-accurate shapes instead of the descriptions transcribed below.

The 2026-09-11 viewing was read carefully, category by category, with the
image actually visible (not recalled from an earlier description), and an
SVG-based prototype of the full 68-glyph set was built and approved by the
user before any of it touched `drawIcon()` — see "Glyphs implemented so
far" below for what shipped, and the category table above for the full
name list per category. The 8 glyphs below are the only ones translated
into real JUCE `Graphics`/`Path` calls; the rest have no `EffectProcessor`
yet (Phases 2–5) and are deliberately left undrawn rather than speculatively
coded against a class that doesn't exist.

## Rule for every effect's `drawIcon()`

- White line art only (`juce::Colours::white`), 1.6–2.5px stroke. Small
  filled dots are fine as accents (control knobs, footswitch, speaker
  grille) — see NAM's amp/amp+cab/pedal glyphs.
- The glyph communicates the effect TYPE, never the category colour or the
  block's on/off state — `EffectBlockComponent::paint()` already handles
  colour/border/bypass around it (see AGENT.md's UI/UX Design Philosophy).
- Don't assume two roles of the same processor class share OR don't share a
  glyph — check the sheet each time. This flip-flopped twice already: a
  guessed-from-memory pass claimed Neura Amp and Neura Pedal use different
  glyphs (amp-head-with-knobs vs. stompbox), then a corrected pass drew both
  as the same bare chip, and the 2026-09-11 direct reading of the real sheet
  confirmed the sheet genuinely does reuse one glyph across several related
  effects on purpose (Neura Amp = Neura Amp+Cab's chip = Neura Pedal;
  Compressor = Expander = IR Loader's pulse trace; Sustainer = Looper's
  infinity symbol). Match the sheet's own reuse, don't assume either way.
- Keep icons simple enough to draw as a handful of `juce::Path`/
  `Graphics::drawLine`/`drawEllipse` calls — no bitmaps, no external asset
  loading on the audio-adjacent UI thread.

## Categories and colours (as seen on the reference sheet)

| Category (PT label) | Colour | Effects on the sheet |
|---|---|---|
| Amplificadores | orange/red | Amp, Amp+Cab, Cab, Neura Amp, Neura Amp+Cab, Neura Pedal |
| Dinamica | red | Compressor, Limiter, Noise Gate, Expander, Sustainer, Auto Swell |
| Drive | yellow/orange | Overdrive, Distortion, Fuzz, Boost, EQ Drive |
| Modulacao | purple | Chorus, Flanger, Phaser, Tremolo, Vibrato, Rotary, Uni-Vibe, Pitch Mod |
| Delay | blue | Digital, Analog, Tape, Reverse, Dual, Ping Pong, Multi Tap, Looper, Hold |
| Reverb | cyan | Hall, Plate, Room, Spring, Shimmer, Mod Reverb, Cloud, Ambient, Gated, Reverse |
| Filtro/FX | green | Wah, Auto Wah, Filter, Envelope, Octaver, Pitch Shift, Harmonizer, Ring Mod, Synth, Slicer, Bitcrusher, Volume, Bitfession, Expression, Sequencer |
| Utilitarios | grey | Tuner, IR Loader, MIDI, Send/Return, Splitter, Merger, A/B Switch, Buffer, Utility |

Note: `EffectProcessor::getAccentColour()` per class currently picks its own
specific hue rather than these exact category colours — worth reconciling
once more effect classes exist and the palette can be centralised (e.g. a
`GearRouting`-style category→colour table), rather than guessing a full
palette now for effects that don't exist yet.

## Glyphs implemented so far (mapped from the sheet, corrected 2026-09-11)

| Processor (chain role) | Category | Glyph used | File |
|---|---|---|---|
| GateProcessor | Dinamica → Noise Gate | gate post: vertical line + short crossbar near the top | `Source/Effects/GateProcessor.cpp` |
| CompressorProcessor | Dinamica → Compressor | heartbeat/ECG pulse (same trace as Expander and IR Loader's reverb role) | `Source/Effects/CompressorProcessor.cpp` |
| OverdriveProcessor | Drive → Overdrive | soft-clipped waveform, ~1.5 cycles with flattened peaks (not a single sigmoid transfer curve) | `Source/Effects/OverdriveProcessor.cpp` |
| NAMProcessor ("Neural Amp") | Amplificadores → Neura Amp | chip/IC glyph: square + circuit "face" (two dot eyes, curved smile) + one pin tick per side | `Source/Effects/NAMProcessor.cpp` |
| NAMProcessor ("Neural Amp + Cab") | Amplificadores → Neura Amp + Cab | same chip (smaller) on top of a cab box with 2x2 speaker-grille dots | `Source/Effects/NAMProcessor.cpp` |
| NAMProcessor ("Neural Pedal") | Amplificadores → Neura Pedal | same chip glyph as Neural Amp — the sheet reuses it, not a distinct stompbox shape | `Source/Effects/NAMProcessor.cpp` |
| IRLoaderProcessor ("Cab") | Amplificadores → Cab | box + 2x2 speaker-grille dots | `Source/Effects/IRLoaderProcessor.cpp` |
| IRLoaderProcessor ("Reverb") | Reverb → Hall | tall pointed arch (was wrongly drawn as Ambient's concentric rings — corrected 2026-09-11; this role covers every IR-based space in one block, so Hall stands in as the one glyph until Plate/Room/Spring/... are separate blocks) | `Source/Effects/IRLoaderProcessor.cpp` |

A full 68-glyph SVG prototype (all 8 categories, including Bitfession and
Expression which have no `EffectProcessor` yet) was built and approved by
the user 2026-09-11 before this pass — every future effect class (Delay,
Modulation, most of Filtro/FX, Utilitarios — Phases 2–5) should pull its
glyph from that same approved design rather than inventing a new visual
language. That prototype lived in the session's scratch directory, not the
repo, so re-derive from the reference sheet + the category table above
rather than assuming a file exists — re-confirm with the user before
implementing rather than guessing which prototype shape was final.
