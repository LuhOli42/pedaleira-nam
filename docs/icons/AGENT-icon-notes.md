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

- White line art only (`juce::Colours::white`), 1.6–2.5px stroke. Small
  filled dots are fine as accents (control knobs, footswitch, speaker
  grille) — see NAM's amp/amp+cab/pedal glyphs.
- The glyph communicates the effect TYPE, never the category colour or the
  block's on/off state — `EffectBlockComponent::paint()` already handles
  colour/border/bypass around it (see AGENT.md's UI/UX Design Philosophy).
- Don't assume two roles of the same processor class share a glyph just
  because they're conceptually close — an earlier version of this file
  claimed Neura Amp and Neura Pedal use the same "chip" icon on the sheet;
  that was wrong (confirmed by the user, who has the actual sheet), and got
  corrected to amp-head-with-knobs vs. stompbox-with-footswitch. Distinct
  glyphs per role are the default assumption unless you can actually see
  the sheet says otherwise.
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
| NAMProcessor ("Neural Amp") | Amplificadores → Neura Amp | amp head: box + row of control-knob dots on top | `Source/Effects/NAMProcessor.cpp` |
| NAMProcessor ("Neural Amp + Cab") | Amplificadores → Neura Amp + Cab | amp head stacked over a cab box (2x2 speaker-grille dots) | `Source/Effects/NAMProcessor.cpp` |
| NAMProcessor ("Neural Pedal") | Amplificadores → Neura Pedal | stompbox outline (narrower top, footswitch dot near the bottom) | `Source/Effects/NAMProcessor.cpp` |
| IRLoaderProcessor ("Cab") | Amplificadores → Cab | isometric cube | `Source/Effects/IRLoaderProcessor.cpp` |
| IRLoaderProcessor ("Reverb") | Reverb → Ambient (closest generic match) | 3 concentric rings + centre dot | `Source/Effects/IRLoaderProcessor.cpp` |

Every future effect class (Delay, Modulation, Filtro/FX, Utilitarios --
Phases 2-5) should pull its glyph from this same sheet rather than
inventing a new visual language, and get added as a new row above once
implemented.
