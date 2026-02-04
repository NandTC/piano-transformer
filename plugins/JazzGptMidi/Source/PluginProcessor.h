#pragma once
#include <JuceHeader.h>
#include <vector>
#include <unordered_map>

//==============================================================================
// Data Structures for Parsed Composition
//==============================================================================

struct ChordData
{
    juce::String symbol;    // e.g., "Ebmaj7"
    double startBeat;       // absolute beat position
    double duration;        // in beats
    int bar;                // original bar number (for error reporting)
};

struct NoteData
{
    juce::String pitch;     // e.g., "Eb5"
    int midiNote;           // MIDI note number (0-127)
    double startBeat;       // absolute beat position
    double duration;        // in beats
    int bar;                // original bar number
    double beat;            // original beat position
};

struct CompositionData
{
    juce::String title;
    int tempo = 120;
    juce::String key;
    juce::String timeSignature = "4/4";
    juce::String swing = "none";
    int bars = 0;
    int beatsPerBar = 4;

    std::vector<ChordData> chords;
    std::vector<NoteData> melody;

    bool isValid = false;
    juce::String errorMessage;
};

//==============================================================================
// Simple Piano Synth Voice
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
    SimplePianoVoice()
    {
        // Configure ADSR envelope
        adsr.setSampleRate(44100.0);
        adsr.setParameters({ 0.008f, 0.8f, 0.4f, 0.5f }); // Attack, Decay, Sustain, Release
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SimplePianoSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int) override
    {
        frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        level = velocity * 0.15f;

        phase1 = phase2 = phase3 = 0.0;

        adsr.noteOn();

        // Configure filters based on frequency range
        float cutoff = (midiNoteNumber > 60) ? 2000.0f : 1200.0f;
        setupFilters(cutoff);
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff)
            adsr.noteOff();
        else
        {
            clearCurrentNote();
            adsr.reset();
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                        int startSample, int numSamples) override
    {
        if (!adsr.isActive())
            return;

        while (--numSamples >= 0)
        {
            // Generate fundamental and harmonics
            double fundamental = std::sin(phase1);
            double harmonic2 = std::sin(phase2) * 0.3;
            double harmonic3 = std::sin(phase3) * 0.12;

            float sample = static_cast<float>(fundamental + harmonic2 + harmonic3);

            // Apply envelope
            sample *= adsr.getNextSample();

            // Apply filters
            sample = lowpassFilter.processSingleSampleRaw(sample);
            sample = highShelfFilter.processSingleSampleRaw(sample);

            // Apply level
            sample *= level;

            // Write to output
            for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
                outputBuffer.addSample(channel, startSample, sample);

            // Increment phases
            double phaseIncrement = frequency * juce::MathConstants<double>::twoPi / currentSampleRate;
            phase1 += phaseIncrement;
            phase2 += phaseIncrement * 2.0;
            phase3 += phaseIncrement * 3.0;

            // Wrap phases
            if (phase1 > juce::MathConstants<double>::twoPi) phase1 -= juce::MathConstants<double>::twoPi;
            if (phase2 > juce::MathConstants<double>::twoPi) phase2 -= juce::MathConstants<double>::twoPi;
            if (phase3 > juce::MathConstants<double>::twoPi) phase3 -= juce::MathConstants<double>::twoPi;

            ++startSample;
        }
    }

    void setCurrentPlaybackSampleRate(double newRate) override
    {
        currentSampleRate = newRate;
        adsr.setSampleRate(newRate);
    }

private:
    void setupFilters(float cutoff)
    {
        lowpassFilter.setCoefficients(
            juce::IIRCoefficients::makeLowPass(currentSampleRate, cutoff, 0.5));

        highShelfFilter.setCoefficients(
            juce::IIRCoefficients::makeHighShelf(currentSampleRate, 3000.0, 0.5, 0.5f));
    }

    double frequency = 440.0;
    double phase1 = 0.0, phase2 = 0.0, phase3 = 0.0;
    float level = 0.0f;
    double currentSampleRate = 44100.0;

    juce::ADSR adsr;
    juce::IIRFilter lowpassFilter;
    juce::IIRFilter highShelfFilter;
};

//==============================================================================
// Main Processor
//==============================================================================

class JazzGptMidiAudioProcessor : public juce::AudioProcessor
{
public:
    JazzGptMidiAudioProcessor();
    ~JazzGptMidiAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "JazzGptMidi"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Public API for UI
    juce::String compositionText;

    // Parse composition from text
    CompositionData parseComposition(const juce::String& text);

    // Play/Stop transport controls
    void playComposition(const CompositionData& data);
    void stopPlayback();
    bool isPlaying() const { return playing; }

    // Export MIDI files
    enum class ExportMode { Melody, Chords, Full };
    juce::File exportMIDI(const CompositionData& data, ExportMode mode, const juce::File& targetFile);

private:
    // Text Parser
    struct ParsedHeader
    {
        juce::String title, key, timeSignature, swing;
        int tempo = 120;
        int bars = 0;
        int beatsPerBar = 4;
    };

    ParsedHeader parseHeader(const juce::StringArray& lines, int& lineIndex);
    std::vector<ChordData> parseChords(const juce::StringArray& lines, int& lineIndex, int beatsPerBar);
    std::vector<NoteData> parseMelody(const juce::StringArray& lines, int& lineIndex, int beatsPerBar);

    // Chord Symbol Expander
    std::vector<int> expandChord(const juce::String& symbol);
    int parseNoteName(const juce::String& noteName);

    // MIDI File Generator
    juce::MidiFile createMidiFile(const CompositionData& data, ExportMode mode);
    int beatsToTicks(double beats) const { return static_cast<int>(beats * 480); }

    // Piano Synth
    juce::Synthesiser pianoSynth;

    // Transport System
    struct ScheduledNote
    {
        int midiNote;
        double startBeat;
        double duration;
        float velocity;
    };

    std::vector<ScheduledNote> scheduledNotes;
    double playbackStartTime = 0.0;
    double currentBeat = 0.0;
    int currentTempo = 120;
    bool playing = false;
    juce::CriticalSection playbackLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JazzGptMidiAudioProcessor)
};
