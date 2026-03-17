# Parameter Specification: ChordGPT

**Plugin Type:** MIDI Generator + Instrument
**APVTS Parameters:** None

---

## Parameter Summary

This plugin has **NO APVTS parameters**.

ChordGPT is an AI-driven compositional tool:
- Freeform text input (mood/scene description)
- Submit button (triggers OpenAI API call)
- "New Scene" button (clears conversation history)
- Temperature control (GPT creativity — stored in plugin state)
- Voicing selector (chord note distribution — stored in plugin state)
- API key input (OpenAI key — stored in PropertiesFile, NOT DAW project)

All interaction happens through the WebView UI native function bridge, not through automatable parameters.

---

## Plugin State (Non-Automatable)

| State | Type | Storage | Description |
|-------|------|---------|-------------|
| temperature | float (0.0–1.5) | ValueTree | GPT creativity level — sent in API request body |
| voicing | int (0–3) | ValueTree | 0=Close, 1=Open, 2=Spread, 3=Drop2 — chord register distribution |
| conversationHistory | JSON string | ValueTree | Full message array (all prior user prompts + assistant chord responses) |
| apiKey | string | PropertiesFile | OpenAI API key — stored separately from DAW project state for security |
| apiCallInProgress | bool (runtime) | std::atomic | Guards against concurrent API requests — not persisted |

---

## Why No APVTS Parameters?

Traditional audio plugins expose APVTS parameters for:
- Real-time audio DSP control
- DAW automation
- Preset saving/loading

ChordGPT doesn't do traditional audio processing. It:
1. Accepts text input describing a mood or scene
2. Sends full conversation context to OpenAI API
3. Receives a chord (chord name + MIDI notes)
4. Plays the chord via built-in piano synth
5. Outputs MIDI notes to DAW routing

The "parameters" are:
- **temperature**: A float that configures AI behavior, not DSP — no DAW automation needed
- **voicing**: An enum that configures chord register — not a real-time continuous parameter
- **apiKey**: A string credential — completely incompatible with APVTS float system
- **conversationHistory**: A complex JSON array — far too complex for APVTS

This pattern mirrors JazzGptMidi (existing plugin in this codebase), which also uses custom state persistence with no APVTS.

---

## Implementation Notes

- No `juce::AudioProcessorValueTreeState` needed
- State persistence via `getStateInformation()` / `setStateInformation()` using custom `juce::ValueTree`
- API key stored via `juce::ApplicationProperties` / `juce::PropertiesFile` (separate from DAW project)
- WebView communicates with C++ via `.withNativeFunction()` bridge for all state changes
- C++ communicates back to WebView via `evaluateJavascript()` calls
- Conversation history: full `std::vector<juce::var>` messages array, serialized to JSON string for persistence
