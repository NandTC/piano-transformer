# JazzGptMidi - Creative Brief

## Overview

**Type:** Utility Plugin (Text-to-MIDI Converter + Player)
**Core Concept:** Parse LLM-generated jazz compositions, play them back, and export MIDI files
**Status:** 💡 Ideated
**Created:** 2026-02-02

## Vision

A DAW plugin that bridges the gap between LLM-generated jazz compositions and music production. Users copy a structured composition from any LLM (ChatGPT, Claude, etc.) and paste it into JazzGptMidi. The plugin parses the text, validates the structure, plays it back with a built-in piano sound, and provides drag-and-drop MIDI export for melody, chords, and combined tracks.

No LLM API calls—the plugin is purely a parser, player, and MIDI exporter. The magic happens in the human-LLM conversation; this plugin just brings it to life.

## Tech Stack

- **Framework:** JUCE (C++)
- **Plugin Formats:** VST3, AU
- **UI:** WebView (HTML/CSS/JS) with Ma aesthetic
- **Audio:** Built-in simple piano synth for preview playback
- **MIDI Export:** Mode-dependent (see below)

## Export Behavior

| Format | Export Method | UI |
|--------|---------------|-----|
| **Standalone** | Download/save MIDI files | `↓ Melody` `↓ Chords` `↓ Full` buttons |
| **VST3/AU** | Drag MIDI to DAW tracks | `Melody` `Chords` `Full` drag targets |

The plugin detects which mode it's running in and shows appropriate controls.

## UI Design

**Aesthetic:** Ma (monochrome, Japanese minimalism)
**Branding:** Nobody And The Computer
**Window Size:** 480×520px (fixed, no scrolling)

### Layout

```
┌─────────────────────────────────────────┐
│ JAZZGPT MIDI      Nobody And The Computer│
├─────────────────────────────────────────┤
│                                         │
│         [Text Area]                     │
│                                         │
├─────────────────────────────────────────┤
│ 68 bpm │ Eb key │ 4/4 │ 8 bars │ 4 │ 12│
├─────────────────────────────────────────┤
│ [▶ Play] [■ Stop]  [Melody][Chords][Full]│
├─────────────────────────────────────────┤
│ Ready to export                         │
└─────────────────────────────────────────┘
```

### Visual Style

- Pure black background (#000000)
- White elements with transparency hierarchy
- Light weight typography, generous letter spacing
- Thin delicate borders (rgba 255,255,255,0.12)
- Borders strengthen on hover (0.35)
- Inverted active states (white bg, black text)
- Monospace font for text area and status

### Components

| Component | Description |
|-----------|-------------|
| Header | Title + brand name, separated by thin border |
| Text Area | Flex-grow input, monospace, placeholder text |
| Preview Bar | Single row: bpm, key, time, bars, notes, chords |
| Play/Stop | Transport controls for built-in piano playback |
| Drag Targets | Three 72×40px draggable regions (Melody, Chords, Full) |
| Status Bar | Single line status/error message |

## Playback Engine

### Built-in Piano Synth

Simple piano sound for previewing compositions without routing to external instruments.

**Approach:** Basic wavetable or additive synthesis piano
- Clean, simple tone suitable for composition preview
- Velocity-sensitive (melody: 90, chords: 70)
- Responds to parsed tempo for accurate playback

### Transport

| Control | Function |
|---------|----------|
| Play | Start playback from beginning |
| Stop | Stop playback, reset position |

### Playback Behavior

- Parses composition on Play (auto-validates)
- Plays melody and chords simultaneously
- Syncs to parsed TEMPO value
- Visual feedback: status bar shows current bar/beat

### Mockup Files

- `v1-ma-aesthetic.html` - Original spacious version
- `v2-compact.html` - Compact DAW-friendly version
- `v3-with-playback.html` - Added play/stop controls
- `v4-dual-mode.html` - **Approved** shows Standalone vs VST export behavior

## Input Format (LLM Prompt)

The user provides this prompt to their LLM:

```
You are JazzGPT, a jazz composer. You craft compositions that breathe with the imperfect perfection of human performance.

YOUR VOICE:
Speak only in musical structure. Let the rhythm sway, let notes fall slightly ahead or behind. Jazz lives in these spaces between the beats.

THE LANGUAGE OF COMPOSITION:

TITLE: [your composition's name]
TEMPO: [heartbeat in BPM]
KEY: [tonal center]
TIME: [meter]
SWING: [none/light/medium/heavy]
BARS: [length of your statement]

CHORDS:
bar [number]: [harmony]

Single chord per measure:
bar 1: Cmaj7

Multiple chords with durations in parentheses:
bar 2: Cmaj7(2) Dm7(2)
bar 3: C7(1) F7(1) Bb7(2)

Durations must complete the measure (sum to 4 in 4/4, to 3 in 3/4).

MELODY:
bar [number] beat [position]: [pitch] duration [beats]

TIMING AS HUMAN GESTURE:

Beat positions breathe freely—use any decimal to express human timing:

beat 1.0 = precisely on the downbeat

beat 1.5 = exactly halfway through beat 1

beat 1.52 = slightly late, relaxed, laid back

beat 1.48 = slightly early, anticipating, pushing forward

beat 2.05 = just a touch behind beat 2

beat 2.97 = rushing into beat 3

beat 3.33 = somewhere in the flow, finding its own moment

Let notes arrive when they feel right. The space between 1.5 and 1.52 can be the difference between mechanical and alive.

Most notes will land close to round numbers (1.0, 1.5, 2.0), but when a note needs to breathe, let it drift—a subtle 0.03 or 0.08 can give it soul.

NOTATION GUIDE:

Pitches: note + octave (C4, Eb5, F#3, Bb4)

Use # for sharps, b for flats

Middle C = C4

Durations (in beats, can be any decimal):

4.0 = whole note

2.0 = half note

1.5 = dotted quarter

1.0 = quarter note

0.5 = eighth note

0.25 = sixteenth note

0.33 = triplet eighth

Chord vocabulary: Cmaj7, Dm7, G7, Am7b5, Bdim7, C7alt, Csus4, C9, C13, Ebmaj7#11...

COMPLETE EXAMPLE:

TITLE: Midnight Blue
TEMPO: 68
KEY: Eb
TIME: 4/4
SWING: medium
BARS: 8

CHORDS:
bar 1: Ebmaj7
bar 2: Fm7(2) Bb7(2)
bar 3: Ebmaj7
bar 4: Abmaj7
bar 5: Abm7(2) Db7(2)
bar 6: Ebmaj7(2) C7(2)
bar 7: Fm7(2) Bb7(2)
bar 8: Ebmaj7

MELODY:
bar 1 beat 1.0: G4 duration 0.5
bar 1 beat 1.52: Bb4 duration 0.5
bar 1 beat 2.05: Eb5 duration 1.0
bar 1 beat 3.1: D5 duration 0.5
bar 1 beat 3.58: C5 duration 0.5
bar 1 beat 4.0: Bb4 duration 1.0
bar 2 beat 1.0: Ab4 duration 0.5
bar 2 beat 1.48: G4 duration 0.5
bar 2 beat 2.0: F4 duration 1.0
bar 2 beat 3.03: Ab4 duration 0.5
bar 2 beat 3.52: G4 duration 0.5
bar 2 beat 4.0: F4 duration 1.0
bar 3 beat 1.0: Eb4 duration 4.0
bar 5 beat 1.0: Db5 duration 0.5
bar 5 beat 1.5: C5 duration 0.5
bar 5 beat 2.02: Bb4 duration 1.0
bar 5 beat 3.08: Ab4 duration 0.5
bar 5 beat 3.55: G4 duration 0.5
bar 5 beat 4.0: F4 duration 1.0
bar 6 beat 1.0: E4 duration 1.0
bar 6 beat 1.97: F4 duration 1.0
bar 6 beat 3.0: G4 duration 0.5
bar 6 beat 3.45: A4 duration 0.5
bar 6 beat 4.0: Bb4 duration 1.0
bar 7 beat 1.0: C5 duration 0.5
bar 7 beat 1.48: D5 duration 0.5
bar 7 beat 2.0: Eb5 duration 1.0
bar 7 beat 3.05: F5 duration 0.5
bar 7 beat 3.53: G5 duration 0.5
bar 7 beat 4.0: Ab5 duration 1.0
bar 8 beat 1.0: G5 duration 4.0

THE ESSENCE:
Most notes stay close to the grid—1.0, 1.5, 2.0—but when you feel a note should breathe differently, let the decimal express it. A note at 2.05 instead of 2.0 carries a different weight, a different intention.

BEFORE YOU COMPOSE:
✓ Bar numbers flow sequentially
✓ Chord durations sum to the time signature
✓ Beat positions are decimals (can be any value)
✓ Note durations are positive decimals
✓ Pitches follow [note][accidental][octave]
✓ Nothing exists outside this structure
✓ No explanations, no markdown, just the composition

You are now ready. Reply only: "JazzGPT ready. Tell me what you hear."
```

## Parser Specification

### Header Fields

| Field | Format | Example |
|-------|--------|---------|
| TITLE | Free text | Midnight Blue |
| TEMPO | Integer BPM | 68 |
| KEY | Note name | Eb |
| TIME | n/n | 4/4 |
| SWING | none/light/medium/heavy | medium |
| BARS | Integer | 8 |

### CHORDS Section

**Format:** `bar [number]: [chord]` or `bar [number]: [chord](duration) [chord](duration)`

**Rules:**
- Single chord with no parentheses = full bar duration
- Multiple chords must have durations in parentheses
- Durations must sum to beats_per_bar (with float tolerance)

### MELODY Section

**Format:** `bar [number] beat [position]: [pitch] duration [beats]`

**Rules:**
- Beat positions are floats (1.0 to beats_per_bar+1.0 with tolerance)
- Pitch format: note + optional accidental + octave (C4, Eb5, F#3)
- Duration is positive float

### Absolute Time Conversion

```
bar_start = (bar_number - 1) * beats_per_bar
note_start = bar_start + (beat_position - 1.0)
```

### Validation Rules

1. TEMPO must be numeric
2. TIME must be parseable (n/n format)
3. Chord durations per bar must sum to beats_per_bar (tolerance: 0.01)
4. Note durations must be > 0
5. Beat positions must be >= 1.0

## MIDI Rendering

### Track Structure

| Track | Content |
|-------|---------|
| melody.mid | Monophonic melody notes |
| chords.mid | Chord voicings (simultaneous notes) |
| full.mid | Both tracks combined |

### Chord Expansion

Expand chord symbols to MIDI pitches. Support at minimum:
- maj, min, dim, aug
- 7, maj7, m7, m7b5, dim7
- sus4, 9, 13
- alt, #11, b9, #9, b5, #5

**Fallback strategy:** Parse chord symbol into root + quality + extensions, build chord tones by intervals. Unknown chords should not crash—use best-effort interpretation.

### Pitch Mapping

- Middle C = C4 = MIDI 60
- Normalize accidentals (Eb, Bb, F#, etc.)

### Velocity

- Melody: 90 (default)
- Chords: 70 (default)
- Future: configurable

### Tempo

Set MIDI tempo meta event from parsed TEMPO value.

## Use Cases

- Jazz composition workflow with LLM assistance
- Rapid prototyping of jazz arrangements
- Educational tool for understanding jazz harmony
- Quick MIDI sketches from text descriptions

## Deliverables

1. **JUCE Plugin** (VST3 + AU)
2. **WebView UI** implementing Ma aesthetic
3. **README.md** with:
   - Installation instructions
   - Usage guide
   - Input format specification (LLM prompt)
   - Example paste
   - Known limitations

## Technical Notes

### Parser (C++)
- Resilient to minor LLM formatting drift
- Strict enough to catch real errors with helpful messages
- Returns structured data for both playback and MIDI export

### MIDI Drag-and-Drop
- Generate MIDI files to temp directory
- Implement JUCE drag-and-drop source for DAW integration
- Clean up temp files appropriately

### Piano Synth
- Simple built-in sound (no external dependencies)
- Could use basic additive synthesis or embedded samples
- Polyphonic for chord playback

## Next Steps

- [ ] Create UI mockup (`/dream JazzGptMidi` → option 3)
- [ ] Start implementation (`/implement JazzGptMidi`)
