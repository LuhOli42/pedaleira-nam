# Effect icon set — reference and rules

The user supplied a reference sheet ("PEDALBOARD UI — Icones de Efeitos —
Padrao Unificado") of the icon set every effect block should eventually use:
one consistent line-art glyph per effect type, white on the block's own
category-coloured outline, no fill colour on the glyph itself. Its own
tagline says it best: **same size, same style, more music.**

The sheet was pasted inline in chat and this agent has no tool that can
save an inline image to disk — only what a human explicitly attaches as a
file makes it into the repo as a real asset. **If you have the original
PNG, drop it in this folder (e.g. `docs/icons/reference-sheet.png`)** so
future work can check pixel-accurate shapes instead of the categories/
glyph-descriptions transcribed below from memory of that one viewing.

## Rule for every effect's `drawIcon()`

- White line art only (`juce::Colours::white`), 1.6–2.5px stroke. No fill
  except small accent dots (see NAM's chip glyph).
- The glyph communicates the effect TYPE, never the category colour or the
  block's on/off state — `EffectBlockComponent::paint()` already handles
  colour/border/bypass around it (see AGENT.md's UI/UX Design Philosophy).
- When two roles of the same processor class are close enough in concept
  that the reference sheet itself draws them near-identically (e.g. Neura
  Amp / Neura Pedal), don't invent a fake distinction — use the same glyph
  and let the category colour + block label carry the difference, exactly
  like the sheet does.
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

## Glyphs implemented so far (mapped from the sheet)

| Processor (chain role) | Category | Glyph used | File |
|---|---|---|---|
| GateProcessor | Dinamica → Noise Gate | plus/cross | `Source/Effects/GateProcessor.cpp` |
| CompressorProcessor | Dinamica → Compressor | heartbeat/ECG pulse | `Source/Effects/CompressorProcessor.cpp` |
| OverdriveProcessor | Drive → Overdrive | smooth S-curve | `Source/Effects/OverdriveProcessor.cpp` |
| NAMProcessor ("Neural Amp") | Amplificadores → Neura Amp | chip (IC outline, centre dot, 4 pin stubs) | `Source/Effects/NAMProcessor.cpp` |
| NAMProcessor ("Neural Pedal") | Amplificadores → Neura Pedal | same chip glyph as Neura Amp | `Source/Effects/NAMProcessor.cpp` |
| IRLoaderProcessor ("Cab") | Amplificadores → Cab | isometric cube | `Source/Effects/IRLoaderProcessor.cpp` |
| IRLoaderProcessor ("Reverb") | Reverb → Ambient (closest generic match) | 3 concentric rings + centre dot | `Source/Effects/IRLoaderProcessor.cpp` |

Every future effect class (Delay, Modulation, Filtro/FX, Utilitarios --
Phases 2-5) should pull its glyph from this same sheet rather than
inventing a new visual language, and get added as a new row above once
implemented.
