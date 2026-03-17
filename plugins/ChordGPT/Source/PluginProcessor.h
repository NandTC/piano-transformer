#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <atomic>
#include <array>
#include <vector>
#include <thread>
#include <functional>

//==============================================================================
// Forward declarations for stub piano synth classes
//==============================================================================

class SimplePianoSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class SimplePianoVoice : public juce::SynthesiserVoice
{
public:
    SimplePianoVoice();

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample, int numSamples) override;
    void setCurrentPlaybackSampleRate(double newRate) override;

private:
    double frequency = 440.0;
    double phase1 = 0.0, phase2 = 0.0, phase3 = 0.0;
    float level = 0.0f;
    double currentSampleRate = 44100.0;

    juce::ADSR adsr;
    juce::IIRFilter lowpassFilter;
    juce::IIRFilter highShelfFilter;

    void setupFilters(float cutoff);
};

//==============================================================================
// ChordGPT AudioProcessor
//
// Plugin type:  MIDI Generator + Instrument
// Bus config:   Output-only stereo (IS_SYNTH TRUE, no input bus)
// State:        Custom juce::ValueTree (NO APVTS)
// API key:      juce::ApplicationProperties / PropertiesFile (separate from DAW state)
//==============================================================================

class ChordGPTProcessor : public juce::AudioProcessor,
                          public juce::AsyncUpdater
{
public:
    ChordGPTProcessor();
    ~ChordGPTProcessor() override;

    //==========================================================================
    // AudioProcessor overrides
    //==========================================================================

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ChordGPT"; }

    // ChordGPT generates MIDI output; it does NOT receive MIDI input
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==========================================================================
    // AsyncUpdater override (called on message thread after API response)
    //==========================================================================

    void handleAsyncUpdate() override;

    //==========================================================================
    // Public API — called from PluginEditor / WebView native functions
    //==========================================================================

    /** Send a text prompt to OpenAI and request a chord. Non-blocking — uses background thread. */
    void requestChord(const juce::String& prompt);

    /** Clear the conversation history and start a new scene. */
    void clearConversation();

    /** Immediately queue a chord (MIDI note list) for playback. Thread-safe via atomic flag. */
    void triggerChord(const std::vector<int>& midiNotes);

    /** Set GPT creativity level (0.0–1.5). Stored in ValueTree state. */
    void setTemperature(float temperature);

    /** Set chord voicing (0=Close, 1=Open, 2=Spread, 3=Drop2). Stored in ValueTree state. */
    void setVoicing(int voicingIndex);

    /** Store the OpenAI API key via ApplicationProperties (separate from DAW project). */
    void setApiKey(const juce::String& apiKey);

    /** Retrieve the OpenAI API key from ApplicationProperties. */
    juce::String getApiKey() const;

    /** Returns true if an API call is currently in progress. */
    bool isApiCallInProgress() const { return apiCallInProgress.load(); }

    /** Returns current temperature setting. */
    float getTemperature() const { return temperature; }

    /** Returns current voicing index. */
    int getVoicing() const { return voicing; }

    /** Returns the current conversation history as a JSON string. */
    juce::String getConversationHistoryJson() const;

    /** Callback set by editor to receive chord results on message thread. */
    std::function<void(const juce::String& chordName, const std::vector<int>& notes)> onChordReceived;

    /** Callback set by editor to receive error messages on message thread. */
    std::function<void(const juce::String& errorMessage)> onApiError;

private:
    //==========================================================================
    // State — custom ValueTree (NO APVTS)
    //==========================================================================

    juce::ValueTree pluginState { "ChordGPTState" };

    float temperature = 0.7f;     // GPT creativity (0.0–1.5)
    int   voicing     = 0;        // 0=Close, 1=Open, 2=Spread, 3=Drop2

    // Conversation history stored as JSON string array
    // e.g. [{"role":"user","content":"..."}, {"role":"assistant","content":"..."}]
    juce::String conversationHistoryJson { "[]" };

    //==========================================================================
    // API key — stored via ApplicationProperties (NOT in DAW project)
    //==========================================================================

    mutable juce::ApplicationProperties appProperties;

    //==========================================================================
    // Lock-free MIDI queue (audio-thread safe)
    //==========================================================================

    // Set to true by message thread when a new chord is ready to inject
    std::atomic<bool> newChordReady { false };

    // MIDI notes of the pending chord (up to 8 notes, e.g. spread voicings)
    std::array<int, 8> pendingChordNotes {};
    std::atomic<int>   pendingNoteCount  { 0 };

    // MIDI note-off tracking: notes currently sounding (injected via processBlock)
    std::array<int, 8> activeNotes {};
    std::atomic<int>   activeNoteCount { 0 };
    std::atomic<bool>  noteOffPending  { false };

    //==========================================================================
    // API state
    //==========================================================================

    std::atomic<bool> apiCallInProgress { false };

    // Background thread for OpenAI HTTP requests
    std::unique_ptr<std::thread> apiThread;

    // Pending chord result — written by background thread, read by handleAsyncUpdate()
    // Protected by resultLock (message thread only reads after triggerAsyncUpdate)
    juce::CriticalSection resultLock;
    juce::String pendingChordName;
    std::vector<int> pendingChordNotesResult;
    juce::String pendingErrorMessage;
    bool pendingIsError = false;

    //==========================================================================
    // Piano synth engine
    //==========================================================================

    juce::Synthesiser pianoSynth;

    static constexpr int kNumVoices = 16;

    //==========================================================================
    // Internal helpers
    //==========================================================================

    void initAppProperties();
    void initPianoSynth();

    // Called on background thread — performs HTTP request to OpenAI
    void performApiCall(const juce::String& prompt);

    // Parse GPT response JSON and extract chord name + MIDI notes
    bool parseGptResponse(const juce::String& responseJson,
                          juce::String& outChordName,
                          std::vector<int>& outNotes,
                          juce::String& outError);

    // Build chord voicing from root + quality
    std::vector<int> buildChordNotes(int rootMidi, const juce::String& quality) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordGPTProcessor)
};
