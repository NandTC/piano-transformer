# Parameter Specification: JazzGptMidi

**Plugin Type:** Utility (Text-to-MIDI Converter)
**APVTS Parameters:** None

---

## Parameter Summary

This plugin has **NO APVTS parameters**.

JazzGptMidi is an action-based utility plugin:
- Text input (composition text from LLM)
- Action buttons (Play, Stop, Export Melody, Export Chords, Export Full)
- Status display (parsing results, playback position)

All interaction happens through the WebView UI, not through automatable parameters.

---

## UI State (Non-Automatable)

| State | Type | Description |
|-------|------|-------------|
| compositionText | string | Raw text pasted from LLM |
| isPlaying | bool | Transport playback state |
| currentBar | int | Current playback position (bar) |
| currentBeat | float | Current playback position (beat) |
| parsedTempo | int | Extracted tempo from text |
| parsedKey | string | Extracted key from text |
| parsedTime | string | Extracted time signature |
| parsedBars | int | Extracted bar count |
| noteCount | int | Number of melody notes parsed |
| chordCount | int | Number of chords parsed |
| statusMessage | string | Current status/error message |

---

## Why No Parameters?

Traditional audio plugins expose parameters for:
- Real-time audio processing control
- DAW automation
- Preset saving/loading

JazzGptMidi doesn't process audio in real-time. It:
1. Receives text input
2. Parses it into structured data
3. Generates MIDI files on demand
4. Plays preview audio (non-automatable)

The "parameters" are the composition text itself, which is too complex for APVTS (arbitrary-length string with structured data).

---

## Implementation Notes

- No `juce::AudioProcessorValueTreeState` needed
- State persistence via `getStateInformation()` / `setStateInformation()` for composition text only
- WebView communicates with C++ via custom message passing (not parameter listeners)
