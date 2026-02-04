# JazzGptMidi Notes

## Status
- **Current Status:** 🚧 Stage 0 (Research & Planning Complete)
- **Version:** 1.0.0 (planned)
- **Type:** Utility Plugin (Text-to-MIDI Converter + Preview Player)
- **Complexity:** 2.0 (Moderate - single-pass implementation)

## Lifecycle Timeline

- **2026-02-02:** Ideation complete - Creative brief created
- **2026-02-03 (Stage 0):** Research & Planning complete - Architecture and plan documented (Complexity 2.0)

## Description

A DAW plugin that bridges the gap between LLM-generated jazz compositions and music production. Users copy a structured composition from any LLM (ChatGPT, Claude, etc.) and paste it into JazzGptMidi. The plugin parses the text, validates the structure, plays it back with a built-in piano sound, and provides drag-and-drop MIDI export for melody, chords, and combined tracks.

**Core Functionality:**
1. Parse LLM-generated jazz composition text (structured format)
2. Preview playback with built-in simple piano synth
3. Generate MIDI files (melody.mid, chords.mid, full.mid)
4. Export via drag-and-drop (VST/AU) or download (Standalone)

**Key Features:**
- Text parser (TITLE, TEMPO, KEY, TIME, SWING, BARS, CHORDS, MELODY sections)
- Chord symbol expansion (Cmaj7, Fm7, Bb7, etc. → MIDI pitch arrays)
- Simple piano synth (additive synthesis: fundamental + 2 harmonics)
- Transport controls (Play/Stop with position tracking)
- Mode detection (Standalone vs VST/AU for appropriate export UI)
- Ma aesthetic UI (monochrome Japanese minimalism)

## Plugin Type Classification

**NOT a traditional DSP effect:**
- NO audio input processing
- NO automatable effect parameters
- NO APVTS parameters (text input and action buttons instead)
- Utility/converter plugin (generates audio from parsed text)

**Bus Configuration:**
- Output-only (no audio input bus)
- Standard stereo output
- IS_SYNTH FALSE (utility plugin, not instrument/effect)

## Complexity Breakdown

**Score: 2.0** (Moderate - single-pass implementation)

- **Parameters:** 0 (no APVTS parameters, action-based plugin)
- **DSP Algorithms:** 1 (simple piano synth with additive synthesis)
- **Features:** 1 (MIDI file I/O)
- **Total:** 0.0 + 1.0 + 1.0 = 2.0

**Implementation Strategy:** Single-pass (no phased implementation needed)

## Architecture Highlights

**Text Parser:**
- Line-by-line regex parsing with state machine (header → chords → melody)
- Tolerant of LLM formatting drift (flexible whitespace, minor variations)
- Helpful error messages for validation failures

**Chord Symbol Expansion:**
- Algorithmic chord builder (parse root + quality + extensions → intervals)
- Support: maj, min, 7, maj7, m7, m7b5, dim7, sus4, 9, 13, alt (expandable)
- Fallback: Unknown chords → root + major triad (never crash)

**MIDI File Generation:**
- Type 0 MIDI files (single track) for simplicity
- Tempo and time signature meta events
- Bar/beat → MIDI ticks conversion (480 TPPQN)
- Velocity: Melody 90, Chords 70

**Simple Piano Synth:**
- Additive synthesis (fundamental + 2nd harmonic + 3rd harmonic)
- ADSR envelope (attack 8ms, decay 800ms, release duration*0.4)
- Lowpass filter (2000Hz melody, 1200Hz chords)
- High shelf cut (-6dB at 3000Hz for vintage warmth)
- Preview quality (not production-grade piano)

**Transport System:**
- Play: Parse → schedule MIDI events → trigger synth voices
- Stop: Clear events → stop synth → reset position
- Position tracking: Display current bar/beat in status bar

**Mode-Specific Export:**
- **Standalone:** File chooser dialog → save MIDI to user location
- **VST/AU:** Drag-and-drop source → temp MIDI file → DAW import
- Fallback: "Export to Desktop" button if drag-and-drop fails

## WebView UI

**Design:** Ma aesthetic (monochrome Japanese minimalism, v7-warm-rhodes.html mockup)

**Components:**
- Text area: Paste LLM output
- Preview bar: bpm, key, time, bars, notes count, chords count
- Transport: Play/Stop buttons
- Export targets: Melody / Chords / Full (download OR drag-and-drop)
- Status bar: Playback position, error messages, success confirmations

**Communication:**
- JavaScript → C++: `window.juce.playComposition(text)`, `window.juce.exportMIDI(mode)`
- C++ → JavaScript: `updatePreview(data)`, `updatePlayhead(position)`, `showStatus(message, type)`
- NO WebSliderRelay (no APVTS parameters)

## JUCE Modules

- `juce::MidiFile`, `juce::MidiMessage`, `juce::MidiMessageSequence` - MIDI generation
- `juce::Synthesiser`, `juce::SynthesiserVoice` - Piano synth
- `juce::WebBrowserComponent` - WebView UI
- `juce::File`, `juce::FileOutputStream`, `juce::TemporaryFile` - File I/O
- `juce::FileChooser` - Standalone file save dialog
- `juce::DragAndDropContainer` - VST/AU drag-and-drop export
- `juce::IIRFilter` - Synth filtering
- `juce::ADSR` - Synth envelope

## State Persistence

**Saved State:**
- Text area content (last pasted composition)
- NO playback state (always stops on plugin close)
- NO export history

**Serialization:**
- Custom state via `juce::ValueTree`
- Store text content as XML string
- Restore text to WebView on plugin load

## Known Issues

None (Stage 0 complete, implementation not started)

## Implementation Risks

**High Risk (30%):** Chord Symbol Expansion
- Mitigation: Algorithmic builder with fallback, incremental dictionary expansion
- Fallback: Unknown chords → root + major triad (never crash)

**Medium Risk (20%):** MIDI Drag-and-Drop DAW Compatibility
- Mitigation: Test in target DAWs early, provide "Export to Desktop" fallback
- Fallback: Always show file chooser option if drag fails

**Medium Risk (20%):** Text Parser Tolerance
- Mitigation: Regex flexibility, helpful error messages, test with real LLM output
- Fallback: Show specific validation errors (user can fix composition text)

**Medium Risk (15%):** Piano Synth Sound Quality
- Mitigation: Accept preview quality (not production), tune for warmth
- Fallback: Users export MIDI for professional piano VSTs

**Low Risk (10%):** MIDI File Generation
- Mitigation: JUCE API is solid, time math is simple

**Low Risk (5%):** WebView Integration
- Mitigation: Patterns well-established (critical patterns #8, #9, #10)

## Testing Strategy

**Unit Testing:**
- Text parser with variety of LLM outputs (valid, malformed, edge cases)
- Chord expansion with common jazz chords
- Time conversion (bar/beat → MIDI ticks)
- MIDI generation (parse → export → verify file structure)

**Integration Testing:**
- End-to-end: Paste text → Play → hear preview → Export → import to DAW
- Mode switching: Test both Standalone and VST/AU modes
- State persistence: Save → reload → verify text restores

**DAW Testing:**
- Logic Pro, Ableton Live, FL Studio: Drag-and-drop MIDI export
- Standalone: Download functionality
- Verify tempo, notes, and MIDI file structure

**Real-World Testing:**
- Real LLM output from ChatGPT, Claude
- Jazz standards (Autumn Leaves, All The Things You Are)
- Long compositions (16+ bars, 100+ notes)

## Timeline Estimate

**Total: 15-21 hours**
- Stage 2 (Foundation): 1-2 hours
- Stage 3 (Shell): 1 hour
- Stage 4 (DSP): 6-8 hours
- Stage 5 (GUI): 4-6 hours
- Stage 6 (Validation): 3-4 hours

## Reference Plugins

**Similar Plugins:**
- **AudioCipher** - Text-to-MIDI generator VST ($59.99)
- **WIDI Professional** - Audio-to-MIDI conversion with batch export
- **Random Chord Progression Generator** - Chord symbol to MIDI notes

**System Reference (for implementation):**
- **GainKnob** - WebView integration patterns
- **TapeAge** - WebView ↔ C++ communication
- **FlutterVerb** - Simple synth implementation
- **LushPad** - IS_SYNTH configuration, output-only bus

## Critical Patterns

**From juce8-critical-patterns.md:**
- Pattern #8: Explicit URL mapping in WebView resource provider (NOT generic loop)
- Pattern #9: `NEEDS_WEB_BROWSER TRUE` in CMakeLists.txt for VST3 support
- Pattern #10: Always install to system folders for testing
- Pattern #4: Output-only bus configuration (no input bus for utility plugin)

## Additional Notes

**Use Cases:**
- Jazz composition workflow with LLM assistance (ChatGPT, Claude)
- Rapid prototyping of jazz arrangements
- Educational tool for understanding jazz harmony
- Quick MIDI sketches from text descriptions

**Deliverables:**
- JUCE Plugin (VST3 + AU + Standalone)
- WebView UI (Ma aesthetic)
- README.md with installation, usage guide, LLM prompt, example paste

**Formats:**
- VST3: `~/Library/Audio/Plug-Ins/VST3/JazzGptMidi.vst3` (when installed)
- AU: `~/Library/Audio/Plug-Ins/Components/JazzGptMidi.component` (when installed)
- Standalone: Application in build artifacts

**User Workflow:**
1. Paste LLM-generated jazz composition text into plugin UI
2. Click Play to preview with built-in piano synth
3. Click Export (Melody/Chords/Full) to generate MIDI files
4. Drag MIDI to DAW track (VST/AU) OR download to disk (Standalone)
5. Use MIDI with professional piano VSTs for production
