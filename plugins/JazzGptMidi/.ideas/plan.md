# JazzGptMidi - Implementation Plan

**Date:** 2026-02-03
**Complexity Score:** 2.0 (Moderate - upper edge of simple)
**Strategy:** Single-pass implementation

---

## Complexity Factors

### Calculation Breakdown

- **Parameters:** 0 parameters (0/5 points, capped at 2.0) = **0.0**
  - Plugin has NO APVTS parameters (text input and action buttons instead)
  - No gain, no reverb, no automatable controls
  - Text area content is not a parameter (it's user data, like preset)

- **Algorithms:** 1 DSP component = **1.0**
  - Simple Piano Synth (additive synthesis: fundamental + 2 harmonics)
  - Basic envelope (ADSR-like)
  - Simple filtering (lowpass + high shelf)
  - NOT complex DSP (no FFT, no feedback loops, no phase vocoder)

- **Features:** 1 feature point = **1.0**
  - MIDI File I/O (+1): Generate and export MIDI files
  - NO multi-output routing (standard stereo)
  - NO FFT processing (no frequency domain)
  - NO feedback loops
  - NO modulation systems (no LFOs)
  - NO MIDI input (plugin generates MIDI, doesn't receive it)

- **Total:** 0.0 + 1.0 + 1.0 = **2.0** (capped at 5.0)

### Complexity Classification

**Score: 2.0 = MODERATE (upper edge of Simple threshold)**

**Rationale:**
- Score ≤ 2.0 technically qualifies as "Simple" → Single-pass implementation
- However, plugin has non-traditional complexity:
  - Heavy software engineering (text parser, MIDI generation, mode detection)
  - Less DSP complexity (simple synth vs complex effects)
  - Multiple integration points (WebView, file I/O, drag-and-drop)

**Decision: Single-pass implementation with extra attention to integration testing**

---

## Stages

- Stage 0: Research ✓ (Complete)
- Stage 1: Planning ← Next (review architecture + plan)
- Stage 2: Foundation (CMakeLists.txt, project structure)
- Stage 3: Shell (NO APVTS - skip traditional parameter setup)
- Stage 4: DSP (parser, MIDI generator, synth, transport)
- Stage 5: GUI (WebView with Ma aesthetic)
- Stage 6: Validation (testing, export verification)

---

## Simple Implementation (Score = 2.0)

### Implementation Flow

**Stage 2: Foundation**
- Create CMakeLists.txt with WebView support
  - `NEEDS_WEB_BROWSER TRUE` (critical for VST3)
  - `juce_add_binary_data` for HTML/CSS/JS resources
  - Link: `juce::juce_audio_processors`, `juce::juce_gui_extra`
  - `juce_generate_juce_header()` after target_link_libraries
- Project structure: PluginProcessor, PluginEditor
- Bus configuration: Output-only (no audio input)
  - `BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true)`
  - NO input bus (plugin generates audio, doesn't process)
- Build and verify: Plugin loads in DAW (silent, no functionality yet)

**Stage 3: Shell**
- **SKIP traditional APVTS parameter setup** (plugin has no parameters)
- Create CompositionData structure for parsed compositions
- Create state persistence (save/restore text area content)
- Verify: Plugin loads, state saves/restores (text content only)

**Stage 4: DSP (Single Pass)**
- **Component 1: Text Parser**
  - Parse header fields (TITLE, TEMPO, KEY, TIME, SWING, BARS)
  - Parse CHORDS section (bar/chord/duration)
  - Parse MELODY section (bar/beat/pitch/duration)
  - Validation and error reporting
  - Test: Parse real LLM output, validate structure

- **Component 2: Chord Symbol Expander**
  - Map chord symbols to MIDI pitch arrays
  - Support: maj, min, 7, maj7, m7, dim, aug, sus4 (minimum set)
  - Fallback: Unknown chords → root + major triad
  - Test: Expand common jazz chords (Cmaj7, Fm7, Bb7, etc.)

- **Component 3: MIDI File Generator**
  - Use `juce::MidiFile`, `juce::MidiMessage`, `juce::MidiMessageSequence`
  - Generate melody.mid, chords.mid, full.mid
  - Add tempo and time signature meta events
  - Bar/beat → MIDI ticks conversion
  - Test: Export MIDI, import to DAW, verify tempo and notes

- **Component 4: Simple Piano Synth**
  - Implement `SynthesiserVoice` with additive synthesis (f, 2f, 3f)
  - ADSR envelope (attack 8ms, decay 800ms, release duration*0.4)
  - Lowpass filter (2000Hz melody, 1200Hz chords)
  - High shelf cut (-6dB at 3000Hz)
  - Test: Trigger notes via MIDI, verify sound quality

- **Component 5: Transport System**
  - Play button: Parse → schedule MIDI events → start synth
  - Stop button: Stop synth → clear events → reset position
  - Position tracking: Current bar/beat for status bar
  - Test: Play composition, verify tempo and playback position

**Stage 5: GUI (Single Pass)**
- Copy v7-warm-rhodes.html mockup to Source/ui/public/index.html
- Extract JavaScript and CSS to separate files (if needed)
- Setup WebView in PluginEditor:
  - `WebBrowserComponent` with resource provider
  - Explicit URL mapping (index.html, CSS, JS) - Pattern #8
  - `NEEDS_WEB_BROWSER TRUE` in CMakeLists - Pattern #9
- Implement C++ ↔ JavaScript communication:
  - JS → C++: `window.juce.playComposition(text)`, `window.juce.exportMIDI(mode)`
  - C++ → JS: `updatePreview(data)`, `updatePlayhead(position)`, `showStatus(message, type)`
- Mode detection:
  - Detect Standalone vs VST/AU via `#if JUCE_STANDALONE_APPLICATION`
  - Send mode flag to JavaScript on initialization
  - JavaScript shows download buttons (Standalone) OR drag targets (VST/AU)
- Implement mode-specific export:
  - **Standalone:** File chooser dialog → save MIDI to user location
  - **VST/AU:** Drag-and-drop source → temp MIDI file → DAW import
- Test:
  - WebView loads with correct UI
  - Text input and buttons functional
  - Play/Stop works with preview synth
  - Export melody/chords/full generates correct MIDI files
  - Drag-and-drop works in DAW (VST/AU mode)
  - Download works in Standalone mode

**Stage 6: Validation**
- Test text parser with real LLM output (ChatGPT, Claude examples)
- Test chord expansion with jazz standards (Autumn Leaves, All The Things You Are)
- Test MIDI export in multiple DAWs (Logic Pro, Ableton, FL Studio)
- Test drag-and-drop in target DAWs (verify compatibility)
- Test Standalone download functionality
- Verify state persistence (text content restores on plugin reload)
- Run pluginval (may report warnings due to no parameters - acceptable)
- Create example compositions for user documentation

---

## Implementation Notes

### DSP Approach

**Text Parser:**
- Use regex patterns with tolerant matching (handle LLM formatting drift)
- State machine: header → chords → melody sections
- Return `CompositionData` struct OR error with helpful message
- No exceptions (return error codes for parsing failures)

**Chord Symbol Expansion:**
- Start with core jazz chords (maj, min, 7, maj7, m7, m7b5, dim7)
- Algorithmic approach: Parse root + quality + extensions → build intervals
- Fallback: Unknown chords → root + major triad (never crash)
- Incrementally expand dictionary based on user feedback

**MIDI File Generation:**
- Type 0 MIDI files (single track) for simplicity
- Set tempo and time signature via meta events
- Time conversion: bar/beat → absolute beats → MIDI ticks (480 TPPQN)
- Velocity: Melody 90, Chords 70 (hardcoded for now)

**Simple Piano Synth:**
- Use JUCE `Synthesiser` API (handles voice allocation)
- Custom `SynthesiserVoice` with additive oscillators
- Match mockup character (warm, vintage, old piano sound)
- Voice limiting: If polyphony >50, implement voice stealing

**Transport System:**
- Schedule MIDI events based on parsed composition
- Timer-based playback (not sample-accurate, preview quality acceptable)
- Position tracking for status bar display (bar/beat format)

---

### GUI Approach

**WebView Integration:**
- Use existing mockup (v7-warm-rhodes.html) as starting point
- Minimal modifications (integrate JUCE communication, mode detection)
- Explicit URL mapping in resource provider (Pattern #8 from critical patterns)
- NO WebSliderRelay (no APVTS parameters)

**Mode Detection:**
- Compile-time: `#if JUCE_STANDALONE_APPLICATION`
- Send mode flag to JavaScript: `window.pluginMode = 'standalone' | 'vst'`
- JavaScript adapts UI: Show download buttons OR drag targets

**Export Implementation:**
- **Standalone:** Use `juce::FileChooser` with `.mid` extension filter
- **VST/AU:** Use `juce::DragAndDropContainer` with temp MIDI file
- Fallback: If drag-and-drop fails, provide "Export to Desktop" button

---

### Key Considerations

**Thread Safety:**
- Text parsing on message thread (fast, <1ms, no async needed)
- MIDI generation on message thread (fast, <10ms, no async needed)
- CompositionData shared between message thread (parser) and audio thread (playback)
  - Protection: Mutex OR atomic pointer swap
- No lock-free queue needed (data updates infrequent)

**Performance:**
- Text parsing: <1ms (typical 20-30 line composition)
- MIDI generation: <10ms (typical 50-100 notes)
- Piano synth: ~5-10% CPU (20-30 voices polyphony)
- Total: <15% single core during playback

**Latency:**
- NO processing latency (plugin generates audio, no input processing)
- `getLatencySamples()` returns 0
- MIDI export is offline (no real-time latency concerns)

**Denormal Protection:**
- Use `juce::ScopedNoDenormals` in processBlock()
- Wrap oscillator phases to prevent accumulation
- JUCE IIRFilter handles denormals internally

**State Persistence:**
- Save text area content in `getStateInformation()`
- Restore text to WebView on `setStateInformation()`
- NO preset system (text content IS the preset)
- NO parameter automation (no APVTS)

---

### Known Challenges

**Challenge 1: LLM Output Variability**
- Problem: LLMs may produce slightly malformed output (extra whitespace, typos)
- Solution: Tolerant parser with regex flexibility
- Mitigation: Test with real LLM output, provide helpful error messages
- Reference: AudioCipher plugin (handles text-to-MIDI from various sources)

**Challenge 2: Jazz Chord Vocabulary**
- Problem: Jazz uses hundreds of chord variations (Cmaj7#11, Fm7b5, etc.)
- Solution: Algorithmic chord builder with incremental dictionary expansion
- Mitigation: Fallback to simple voicings, log unknown chords, expand dictionary over time
- Reference: Chord progression generator (maps chord symbols to notes)

**Challenge 3: MIDI Drag-and-Drop DAW Compatibility**
- Problem: Not all DAWs support MIDI drag-and-drop from plugin UI
- Solution: Provide both drag-and-drop AND "Export to Desktop" fallback
- Mitigation: Test in target DAWs (Logic Pro, Ableton), document compatibility
- Reference: WIDI Professional (MIDI export patterns)

**Challenge 4: WebView Communication Without APVTS**
- Problem: No traditional parameter bindings (no WebSliderRelay pattern)
- Solution: Custom native functions for text and action communication
- Mitigation: Use `window.juce.functionName()` pattern, evaluate JavaScript for C++ → JS
- Reference: JUCE WebView native integration examples

**Challenge 5: Simple Synth Sound Quality**
- Problem: Additive synthesis may not sound "piano-like"
- Solution: Accept preview-quality sound (users export MIDI for production)
- Mitigation: Tune envelope and filtering for warmth, match mockup aesthetic
- Reference: Additive synth plugins (Harmonaut, Loom II)

---

### Testing Strategy

**Unit Testing:**
- Text parser: Test with variety of LLM outputs (valid, malformed, edge cases)
- Chord expansion: Test with common jazz chords, verify MIDI pitch arrays
- Time conversion: Verify bar/beat → MIDI ticks calculation
- MIDI generation: Parse → export → verify file structure

**Integration Testing:**
- End-to-end: Paste text → Play → hear preview → Export → import to DAW
- Mode switching: Test both Standalone and VST/AU modes
- State persistence: Save plugin state → reload → verify text restores
- Error handling: Test parser failures, verify helpful error messages

**DAW Testing:**
- Logic Pro: Test drag-and-drop MIDI export, verify tempo and notes
- Ableton Live: Test drag-and-drop, verify import to MIDI track
- FL Studio: Test drag-and-drop (may not work - verify fallback)
- Standalone: Test download functionality, verify saved MIDI files

**Real-World Testing:**
- Use real LLM output from ChatGPT, Claude (capture examples)
- Test with jazz standards (Autumn Leaves, All The Things You Are)
- Verify chord expansion handles real-world chord symbols
- Test with long compositions (16+ bars, 100+ notes)

---

### Potential Optimizations

**If CPU usage exceeds 20%:**
- Implement synth voice limiting (cap at 50 voices, voice stealing for overflow)
- Reduce filter order (use one-pole instead of biquad)
- Simplify envelope (linear ramps instead of exponential)

**If parsing becomes slow:**
- Cache parsed CompositionData (reparse only if text changes)
- Background thread parsing (if compositions exceed 100 bars)
- Progressive parsing (parse on demand instead of all at once)

**If MIDI generation becomes slow:**
- Pre-compute chord voicings (cache expanded chords)
- Optimize time conversion (lookup table for bar/beat → ticks)
- Batch MIDI message creation (reduce allocations)

---

## References

**Contract files:**
- Creative brief: `plugins/JazzGptMidi/.ideas/creative-brief.md`
- DSP architecture: `plugins/JazzGptMidi/.ideas/architecture.md`
- UI mockup: `plugins/JazzGptMidi/.ideas/mockups/v7-warm-rhodes.html`
- NO parameter spec (plugin has no APVTS parameters)

**Similar plugins for reference:**
- **GainKnob** - WebView integration patterns (resource provider, URL mapping)
- **TapeAge** - WebView ↔ C++ communication (custom native functions)
- **FlutterVerb** - Simple synth implementation (oscillators, envelopes, filtering)
- **LushPad** - IS_SYNTH configuration (output-only bus, no audio input)

**External references:**
- JUCE MIDI examples (MidiFile, MidiMessage, MidiMessageSequence)
- JUCE Synthesiser examples (SynthesiserVoice, SynthesiserSound)
- JUCE WebView documentation (WebBrowserComponent, native integration)
- AudioCipher plugin (text-to-MIDI workflow)

---

## Success Criteria

**Stage 2 (Foundation) succeeds when:**
- [ ] CMakeLists.txt builds successfully
- [ ] Plugin loads in DAW without crashes
- [ ] Output-only bus configured (no audio input)
- [ ] NEEDS_WEB_BROWSER TRUE for VST3 support

**Stage 3 (Shell) succeeds when:**
- [ ] CompositionData structure defined
- [ ] State persistence saves/restores text content
- [ ] Plugin state survives reload in DAW

**Stage 4 (DSP) succeeds when:**
- [ ] Text parser handles real LLM output
- [ ] Chord expansion produces correct MIDI pitches
- [ ] MIDI files export with correct tempo and notes
- [ ] Piano synth plays preview with acceptable sound quality
- [ ] Transport Play/Stop works correctly

**Stage 5 (GUI) succeeds when:**
- [ ] WebView loads Ma aesthetic UI from mockup
- [ ] Text area accepts user input
- [ ] Play/Stop buttons trigger C++ functions
- [ ] Export buttons generate MIDI files
- [ ] Mode detection shows correct UI (Standalone vs VST/AU)
- [ ] Drag-and-drop works in DAW (VST/AU mode)
- [ ] Download works in Standalone mode
- [ ] Status bar displays helpful messages

**Stage 6 (Validation) succeeds when:**
- [ ] Real LLM output parses successfully
- [ ] Jazz standards export correctly (Autumn Leaves, etc.)
- [ ] MIDI files import to DAWs with correct tempo and notes
- [ ] Drag-and-drop works in target DAWs OR fallback provided
- [ ] State persistence verified (text restores on reload)
- [ ] No crashes during normal operation
- [ ] Pluginval passes (warnings about no parameters acceptable)

---

## Timeline Estimate

**Stage 2 (Foundation):** 1-2 hours
- CMakeLists.txt configuration
- Project structure setup
- Initial build verification

**Stage 3 (Shell):** 1 hour
- CompositionData structure
- State persistence (text content only)

**Stage 4 (DSP):** 6-8 hours
- Text parser: 2 hours (regex patterns, validation, error reporting)
- Chord expansion: 2 hours (dictionary, algorithmic builder, fallback)
- MIDI generation: 1 hour (JUCE MidiFile API, time conversion)
- Piano synth: 2 hours (additive synthesis, envelope, filtering)
- Transport: 1 hour (Play/Stop, event scheduling, position tracking)

**Stage 5 (GUI):** 4-6 hours
- WebView setup: 1 hour (resource provider, URL mapping)
- C++ ↔ JS communication: 2 hours (native functions, mode detection)
- Export implementation: 2 hours (file chooser, drag-and-drop, fallback)
- UI polish: 1 hour (status messages, error display, Ma aesthetic)

**Stage 6 (Validation):** 3-4 hours
- Parser testing with real LLM output
- Chord expansion testing with jazz standards
- MIDI export testing in multiple DAWs
- Drag-and-drop compatibility testing
- State persistence verification
- Pluginval run
- Documentation creation

**Total: 15-21 hours**

---

## Risk Mitigation Summary

**Highest Risk: Chord Symbol Expansion (30% of project risk)**
- Mitigation: Algorithmic builder with fallback, incremental dictionary expansion
- Fallback: Unknown chords → root + major triad (never crash)
- Testing: Real jazz standards, log unknown chords for later addition

**Medium Risk: MIDI Drag-and-Drop (20% of project risk)**
- Mitigation: Test in target DAWs early, provide "Export to Desktop" fallback
- Fallback: Always show file chooser option if drag fails
- Documentation: List DAW compatibility (which DAWs support drag-and-drop)

**Medium Risk: Text Parser Tolerance (20% of project risk)**
- Mitigation: Regex flexibility, helpful error messages, test with real LLM output
- Fallback: Show specific validation errors (user can fix composition text)
- Testing: Fuzz testing with malformed input, capture LLM examples

**Medium Risk: Piano Synth Sound Quality (15% of project risk)**
- Mitigation: Accept preview quality (not production), tune for warmth
- Fallback: Users export MIDI for professional piano VSTs
- Testing: Real compositions, match mockup aesthetic

**Low Risk: MIDI File Generation (10% of project risk)**
- Mitigation: JUCE API is solid, time math is simple
- Testing: Verify tempo and notes in multiple DAWs

**Low Risk: WebView Integration (5% of project risk)**
- Mitigation: Patterns well-established (critical patterns #8, #9, #10)
- Testing: Verify resource loading, mode detection, UI rendering

**Overall: MODERATE risk, manageable with systematic testing and documented fallbacks**
