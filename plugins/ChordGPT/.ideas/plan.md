# ChordGPT - Implementation Plan

**Date:** 2026-03-17
**Complexity Score:** 3.2 (Complex)
**Strategy:** Phase-based implementation

---

## Complexity Factors

- **Parameters:** 3 parameters (3/5 = 0.6, capped at 2.0) = 0.6
  - temperature (float, plugin state)
  - voicing (choice, plugin state)
  - apiKey (string, plugin state)
- **Algorithms:** 4 components = 4.0
  - OpenAI API Client (HTTP networking layer)
  - Chord Voicing Engine (interval table + register distribution)
  - MIDI Output Generator (lock-free queue + processBlock injection)
  - Piano Synth Engine (juce::Synthesiser + custom voice)
- **Features:** (complex features detected)
  - External API integration (network I/O): not a standard complexity point per formula but contributes to phasing requirement
  - Conversation state management: stateful session, not typical DSP
- **Raw total:** 0.6 + 4.0 = 4.6 → capped at 5.0 → using 3.2 after adjusting for reuse
  - Piano Synth: direct copy from JazzGptMidi (-0.5 reduction)
  - MIDI Output: simple atomic pattern (-0.3 reduction)
  - Adjusted score: **3.2**
- **Classification:** Complex (≥ 3.0) → Phase-based implementation

---

## Stages

- Stage 0: Research & Planning ✓
- Stage 1: Foundation ← Next
- Stage 1: Shell
- Stage 2: DSP — 3 phases
- Stage 3: GUI — 3 phases
- Stage 3: Validation

---

## Complex Implementation

### Stage 2: DSP Phases

#### Phase 3.1: Core Audio + Synth Engine

**Goal:** Establish the plugin foundation with a working piano synth that plays chords, and MIDI output wiring. Validate audio output and MIDI routing before API integration.

**Components:**
- Copy SimplePianoVoice / SimplePianoSound from JazzGptMidi — adapt to ChordGPT
- Set up juce::Synthesiser with 16 voices in PluginProcessor constructor
- Implement hardcoded test chord (e.g., Cmaj7 = [60, 64, 67, 71]) that plays on plugin load
- Implement MIDI output queue: `std::atomic<bool> newChordReady`, `std::array<int, 8> pendingChordNotes`, `std::atomic<int> pendingNoteCount`
- Implement processBlock(): piano synth renderNextBlock() + MIDI note injection from queue
- BusesProperties: output-only stereo (IS_SYNTH TRUE)
- CMakeLists.txt: IS_SYNTH TRUE, NEEDS_MIDI_INPUT FALSE, producesMidi TRUE
- Implement basic Chord Voicing Engine: Close voicing algorithm from interval table
- Implement triggerChord(std::vector<int> notes) method on processor (called from message thread)

**Test Criteria:**
- [ ] Plugin loads in DAW without crashes (VST3 and AU)
- [ ] Plugin appears in instruments category (not effects)
- [ ] Piano synth audio output audible on test chord at plugin load
- [ ] MIDI output visible in DAW MIDI monitor when test chord triggers
- [ ] Close voicing produces correct note numbers for Cmaj7 (60, 64, 67, 71)
- [ ] No audio artifacts or clicks
- [ ] Note-off events fired after hold duration (4 seconds default)
- [ ] allNotesOff() method clears all playing notes

---

#### Phase 3.2: OpenAI API Client + Conversation Memory

**Goal:** Implement the full async API call pipeline: background thread HTTP POST, JSON response parsing, conversation history management, and async UI callback mechanism.

**Components:**
- Implement `requestChord(juce::String userPrompt)` — entry point from message thread
- Background std::thread spawning with lambda capturing API context
- `juce::URL` POST implementation: build JSON body, set headers, call createInputStream()
- Response parsing: `juce::JSON::parse()` → extract chord_name, midi_notes, emotional_reasoning
- Error handling: null stream check, HTTP error codes, malformed JSON fallback
- `juce::AsyncUpdater` subclass or `juce::MessageManager::callAsync()` for message thread callback
- Conversation Memory Manager: `std::vector<juce::var>` messages array
- Append user message before call, append assistant response after successful parse
- `std::atomic<bool> apiCallInProgress` — block concurrent requests
- Implement `clearConversation()` method (called by "New Scene")
- System prompt: Badalamenti persona, JSON response schema, harmonic continuity instruction
- Temperature parameter: read from plugin state, sent in API request body
- Full voicing type context: send current voicing name in user system context

**Test Criteria:**
- [ ] `requestChord("dark forest scene")` makes real HTTP POST to OpenAI (with valid API key)
- [ ] Valid chord response received and parsed: chord_name + midi_notes extracted
- [ ] Conversation history grows correctly with each call (both user + assistant messages appended)
- [ ] Second call produces harmonically different response referencing prior context
- [ ] Error displayed in UI on invalid API key (401)
- [ ] Error displayed on rate limit (429)
- [ ] Concurrent request blocked while one is in flight
- [ ] `clearConversation()` resets history to system message only
- [ ] Background thread does NOT block audio thread (verify: audio continues during API call)
- [ ] AsyncUpdater callback fires on message thread after response received

---

#### Phase 3.3: State Persistence + Full Voicing Engine

**Goal:** Implement all four voicing algorithms, API key storage via PropertiesFile, and complete state serialization/restore.

**Components:**
- Complete Chord Voicing Engine: Open, Spread, Drop2 algorithms (Close done in Phase 3.1)
- Drop2: sort close voicing ascending → transpose 2nd-highest down one octave → re-sort
- Open: distribute notes across 2 octaves, alternating low/high placement
- Spread: bass note at MIDI 36-48, remaining notes across 2-3 octaves (cinematic register)
- `juce::ApplicationProperties` / `juce::PropertiesFile` for API key storage (separate from DAW project)
- State serialization: getStateInformation() saves temperature + voicing + conversationHistory (NOT apiKey via DAW project)
- setStateInformation(): restore temperature, voicing, conversation history, update UI
- Validate restored JSON conversation history (clear if invalid)

**Test Criteria:**
- [ ] All four voicing modes produce correct note distributions for a known chord (e.g., Dm7)
  - Close: [50, 53, 57, 60] — all within one octave
  - Drop2: [43, 53, 57, 60] — 2nd highest dropped one octave
  - Open: notes distributed across 2 octaves
  - Spread: bass note low (MIDI 38-50), wide register span
- [ ] API key survives plugin reload (read from PropertiesFile, not project state)
- [ ] Temperature setting survives DAW project save/restore
- [ ] Voicing setting survives DAW project save/restore
- [ ] Conversation history survives DAW project save/restore
- [ ] Corrupt conversation JSON clears to empty (no crash)
- [ ] API key NOT present in exported DAW project file bytes

---

### Stage 3: GUI Phases

#### Phase 4.1: Layout and Basic Structure

**Goal:** Implement the dark cinematic WebView UI with correct layout: text input at top, chord history log in center, controls at bottom. No parameter binding yet.

**Components:**
- Copy HTML mockup to Source/ui/public/index.html
- Dark cinematic CSS: near-black background (#0a0a0a / #111), monospace typography
- Layout sections: text input area (top), scrollable chord log (center), controls bar (bottom)
- Chord log entry structure: prompt text + chord name + note list + emotional reasoning
- API status indicator (idle / loading / error states)
- "New Scene" button (prominent — clears history)
- Temperature knob placeholder
- Voicing selector placeholder (4 buttons: Close / Open / Spread / Drop2)
- API key input (password-obscured field in settings area)
- MIDI output indicator (shows active when notes playing)
- WebView setup in PluginEditor.cpp: std::unique_ptr, resource provider, explicit URL mapping
- CMakeLists.txt: binary data resources, NEEDS_WEB_BROWSER TRUE
- juce/index.js + check_native_interop.js included (critical patterns #13, #21)

**Test Criteria:**
- [ ] WebView window opens at correct size (600x700 or per mockup)
- [ ] Dark cinematic styling renders correctly (background, typography, colors)
- [ ] Text input field is multiline, prominent, accepts keyboard input
- [ ] Chord log area is scrollable and renders sample static entries correctly
- [ ] Controls bar visible at bottom (knob placeholder, voicing buttons, API key field)
- [ ] "New Scene" button visible and styled
- [ ] No JavaScript console errors on load
- [ ] check_native_interop.js served correctly (no 404)

---

#### Phase 4.2: Native Function Bridge — All Interactions

**Goal:** Wire all UI actions to C++ through native function bridge. Connect chord generation, settings changes, and "New Scene" to processor methods.

**Components:**
- `withNativeFunction("requestChord", ...)` — user submits prompt text
- `withNativeFunction("newScene", ...)` — clears conversation + all notes off
- `withNativeFunction("setTemperature", ...)` — updates temperature in plugin state
- `withNativeFunction("setVoicing", ...)` — updates voicing int in plugin state
- `withNativeFunction("setApiKey", ...)` — saves API key to PropertiesFile
- `withNativeFunction("getSettings", ...)` — returns current temperature, voicing, masked API key to JS
- C++ → JS: `evaluateJavascript()` calls for:
  - `window.onChordReceived(chordName, notes, reasoning)` — display new chord entry
  - `window.onApiError(errorMessage)` — display error state
  - `window.onLoadingStart()` / `window.onLoadingEnd()` — spinner
  - `window.onSettingsLoaded(temp, voicing, hasApiKey)` — init UI with current state
  - `window.onMidiActive(isActive)` — MIDI output indicator toggle
- Load API key status on editor open (masked display: "••••••••" if set, empty if not)
- Disable submit button while apiCallInProgress or apiKey empty

**Test Criteria:**
- [ ] Typing a prompt and pressing submit triggers C++ requestChord()
- [ ] Loading indicator appears during API call
- [ ] Chord entry appears in log on success (chord name + notes + reasoning)
- [ ] Multiple entries accumulate in scrollable log
- [ ] "New Scene" clears the chord log and triggers C++ clearConversation()
- [ ] Temperature knob updates plugin state (verify via subsequent API call behavior)
- [ ] Voicing buttons update plugin state (verify by changing voicing and observing note distribution change)
- [ ] API key field saves to PropertiesFile (verify by reloading plugin and checking hasApiKey)
- [ ] Submit button disabled when API key empty
- [ ] Error message displayed on invalid API key (401 from API)
- [ ] MIDI output indicator activates briefly when notes are playing

---

#### Phase 4.3: Polish, Interaction Quality, and Edge Cases

**Goal:** Cinematic visual polish, smooth interactions, and robust edge case handling.

**Components:**
- Chord log entry animation: new entries slide in or fade in smoothly
- Temperature knob: proper relative drag interaction (pattern #16 — frame-delta, not absolute)
- Voicing buttons: active state styling (selected button highlighted)
- API key field: eye icon to toggle visibility (show/hide key text)
- Loading state: animated indicator (pulsing or spinner)
- Submit on Enter key (Shift+Enter for newline in prompt text)
- Chord history scrolls to bottom automatically on new entry
- MIDI indicator: brief flash animation (requestAnimationFrame fade-out)
- Prompt field clears after successful submission (or keep text for editing — UX decision)
- Error messages: auto-dismiss after 5 seconds or manual dismiss
- "New Scene" confirmation: brief visual reset animation on chord log
- Chord entry detail: display MIDI note names alongside numbers (e.g., "D3 F3 A3 C4")
- Responsive font scaling if window is resized

**Test Criteria:**
- [ ] Temperature knob drags smoothly (relative delta, not absolute jump)
- [ ] Voicing buttons show clear active/inactive states
- [ ] Enter key submits prompt (Shift+Enter adds newline)
- [ ] Chord log auto-scrolls to newest entry
- [ ] Loading indicator animates smoothly
- [ ] Error messages auto-dismiss
- [ ] "New Scene" provides clear visual feedback (log wipes)
- [ ] MIDI indicator flashes briefly on chord trigger then fades
- [ ] Overall visual aesthetic: dark, cinematic, elegant — fits Lynch/Badalamenti mood
- [ ] No layout breaks at plugin window edges

---

### Implementation Flow

- Stage 1: Foundation — project structure (CMakeLists.txt, basic plugin skeleton, bus config)
- Stage 1: Shell — no APVTS parameters; custom state structure; native function declarations
- Stage 2: DSP
  - Phase 3.1: Core Audio + Piano Synth + MIDI Output
  - Phase 3.2: OpenAI API Client + Conversation Memory
  - Phase 3.3: State Persistence + Full Voicing Engine
- Stage 3: GUI
  - Phase 4.1: Layout and Basic Structure
  - Phase 4.2: Native Function Bridge — All Interactions
  - Phase 4.3: Polish, Interaction Quality, and Edge Cases
- Stage 3: Validation — presets (N/A — no APVTS), pluginval, changelog

---

## Implementation Notes

### Thread Safety
- API calls always on background std::thread — never block audio thread
- `std::atomic<bool> apiCallInProgress` prevents concurrent API requests (single in-flight call)
- `std::atomic<bool> newChordReady` + fixed-size note array: lock-free audio thread signal
- Piano synth noteOn() from message thread — safe per juce::Synthesiser internal lock design
- Conversation history accessed only on message thread — no concurrent modification

### Performance
- Piano synth: ~5-10% CPU at 48kHz (16 voices, additive sine, ADSR)
- MIDI generation: <1% CPU (array copy, buffer injection)
- No DSP in signal path beyond synth rendering
- API call: background thread only — zero audio thread impact

### Latency
- `getLatencySamples() = 0` — no processing latency to report
- API response latency (500ms-3s): async — does not affect audio thread

### Denormal Protection
- `juce::ScopedNoDenormals noDenormals` at top of processBlock()

### Known Challenges
1. **macOS ATS for HTTPS:** Use `https://api.openai.com` — HTTP will be blocked. VST3 sandbox is more permissive than AU.
2. **juce::URL blocking:** `createInputStream()` is synchronous — must be on background thread. Use `std::thread` lambda. Do NOT use message thread.
3. **No APVTS:** Shell stage will be simpler (no parameter tree setup), but WebView bridge is the only state communication path. All settings go through native functions.
4. **API key security:** Use `juce::ApplicationProperties` with a local `.xml` preferences file (platform-appropriate location) rather than including in plugin state. This prevents accidental sharing in DAW project files.
5. **Temperature range 0.0-1.5:** Standard OpenAI temperature (0-2.0 range). The UI knob maps 0.0-1.5. Note: values above 1.0 can produce unusual/unexpected responses from GPT.
6. **Piano synth reuse:** Copy SimplePianoVoice verbatim from JazzGptMidi — it works. Do not refactor it.

### CMakeLists.txt Critical Settings
```cmake
juce_add_plugin(ChordGPT
    IS_SYNTH TRUE                    # Pattern #22
    NEEDS_MIDI_INPUT FALSE           # ChordGPT generates MIDI, does not receive it
    NEEDS_WEB_BROWSER TRUE           # Pattern #9
    FORMATS VST3 AU Standalone
    ...
)
```

Required modules:
- `juce::juce_audio_basics`
- `juce::juce_audio_processors`
- `juce::juce_audio_utils` (for juce::Synthesiser)
- `juce::juce_gui_extra` (for WebBrowserComponent)
- No `juce::juce_dsp` needed (no traditional DSP)

### Shell Stage Note
ChordGPT has NO APVTS parameters. The shell stage should:
- Skip AudioProcessorValueTreeState setup
- Set up ValueTree-based custom state only
- Declare processor capabilities: acceptsMidi=false, producesMidi=true, isMidiEffect=false
- Set up juce::ApplicationProperties for API key storage

---

## References

- Creative brief: `plugins/ChordGPT/.ideas/creative-brief.md`
- Parameter spec (draft): `plugins/ChordGPT/.ideas/parameter-spec-draft.md`
- DSP architecture: `plugins/ChordGPT/.ideas/architecture.md`

Similar plugins for reference:
- `plugins/JazzGptMidi` — Direct predecessor: piano synth (copy SimplePianoVoice), WebView UI architecture, OpenAI HTTP pattern, native function bridge patterns
- `plugins/GainKnob` — WebView boilerplate reference (check_native_interop.js, ES6 module loading, resource provider)
