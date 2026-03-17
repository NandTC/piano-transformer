#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// SimplePianoVoice implementation
// Full DSP (filters, ADSR) will be refined in Stage 2; this stub compiles cleanly.
//==============================================================================

SimplePianoVoice::SimplePianoVoice()
{
    adsr.setSampleRate(44100.0);
    adsr.setParameters({ 0.008f, 0.8f, 0.4f, 0.5f }); // Attack, Decay, Sustain, Release
}

bool SimplePianoVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SimplePianoSound*>(sound) != nullptr;
}

void SimplePianoVoice::startNote(int midiNoteNumber, float velocity,
                                  juce::SynthesiserSound*, int /*currentPitchWheelPosition*/)
{
    frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    level     = velocity * 0.15f;
    phase1 = phase2 = phase3 = 0.0;
    adsr.noteOn();

    float cutoff = (midiNoteNumber > 60) ? 2000.0f : 1200.0f;
    setupFilters(cutoff);
}

void SimplePianoVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
        adsr.noteOff();
    else
    {
        clearCurrentNote();
        adsr.reset();
    }
}

void SimplePianoVoice::pitchWheelMoved(int) {}
void SimplePianoVoice::controllerMoved(int, int) {}

void SimplePianoVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                        int startSample, int numSamples)
{
    if (!adsr.isActive())
        return;

    while (--numSamples >= 0)
    {
        double fundamental = std::sin(phase1);
        double harmonic2   = std::sin(phase2) * 0.3;
        double harmonic3   = std::sin(phase3) * 0.12;

        float sample = static_cast<float>(fundamental + harmonic2 + harmonic3);
        sample *= adsr.getNextSample();
        sample  = lowpassFilter.processSingleSampleRaw(sample);
        sample  = highShelfFilter.processSingleSampleRaw(sample);
        sample *= level;

        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            outputBuffer.addSample(ch, startSample, sample);

        const double phaseInc = frequency * juce::MathConstants<double>::twoPi / currentSampleRate;
        phase1 += phaseInc;
        phase2 += phaseInc * 2.0;
        phase3 += phaseInc * 3.0;

        if (phase1 > juce::MathConstants<double>::twoPi) phase1 -= juce::MathConstants<double>::twoPi;
        if (phase2 > juce::MathConstants<double>::twoPi) phase2 -= juce::MathConstants<double>::twoPi;
        if (phase3 > juce::MathConstants<double>::twoPi) phase3 -= juce::MathConstants<double>::twoPi;

        ++startSample;
    }
}

void SimplePianoVoice::setCurrentPlaybackSampleRate(double newRate)
{
    currentSampleRate = newRate;
    adsr.setSampleRate(newRate);
}

void SimplePianoVoice::setupFilters(float cutoff)
{
    lowpassFilter.setCoefficients(
        juce::IIRCoefficients::makeLowPass(currentSampleRate, static_cast<double>(cutoff), 0.5));
    highShelfFilter.setCoefficients(
        juce::IIRCoefficients::makeHighShelf(currentSampleRate, 3000.0, 0.5, 0.5f));
}

//==============================================================================
// ChordGPTProcessor constructor / destructor
//==============================================================================

ChordGPTProcessor::ChordGPTProcessor()
    // IS_SYNTH TRUE — output-only bus, no input bus (Pattern #4 / #22)
    : AudioProcessor(BusesProperties()
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    initAppProperties();
    initPianoSynth();

    // Restore state fields to their defaults (ValueTree starts empty)
    pluginState.setProperty("temperature",          temperature,             nullptr);
    pluginState.setProperty("voicing",              voicing,                 nullptr);
    pluginState.setProperty("conversationHistory",  conversationHistoryJson, nullptr);
}

ChordGPTProcessor::~ChordGPTProcessor()
{
    // Join background thread if running
    if (apiThread && apiThread->joinable())
    {
        // Signal the thread to stop (in Stage 2 we will add a cancellation token).
        apiThread->join();
    }
}

//==============================================================================
// Initialisation helpers
//==============================================================================

void ChordGPTProcessor::initAppProperties()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName     = "ChordGPT";
    opts.filenameSuffix      = ".settings";
    opts.osxLibrarySubFolder = "Application Support";
    opts.folderName          = "ChordGPT";
    appProperties.setStorageParameters(opts);
}

void ChordGPTProcessor::initPianoSynth()
{
    pianoSynth.addSound(new SimplePianoSound());
    for (int i = 0; i < kNumVoices; ++i)
        pianoSynth.addVoice(new SimplePianoVoice());
}

//==============================================================================
// AudioProcessor overrides
//==============================================================================

void ChordGPTProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    pianoSynth.setCurrentPlaybackSampleRate(sampleRate);
}

void ChordGPTProcessor::releaseResources()
{
    pianoSynth.allNotesOff(0, false);
}

void ChordGPTProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Clear any input (instrument — output only)
    buffer.clear();

    // --- Lock-free MIDI injection ---
    // If a new chord is ready (set by message thread via triggerChord()), inject MIDI note-on
    // messages at the start of this block, then clear the flag.
    if (newChordReady.load(std::memory_order_acquire))
    {
        // Stop currently sounding notes first
        const int prevCount = activeNoteCount.load(std::memory_order_relaxed);
        for (int i = 0; i < prevCount; ++i)
        {
            midiMessages.addEvent(juce::MidiMessage::noteOff(1, activeNotes[i]), 0);
            pianoSynth.noteOff(1, activeNotes[i], 1.0f, true);
        }

        // Inject new notes
        const int noteCount = pendingNoteCount.load(std::memory_order_relaxed);
        for (int i = 0; i < noteCount; ++i)
        {
            const int note = pendingChordNotes[static_cast<std::size_t>(i)];
            midiMessages.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(80)), 0);
            pianoSynth.noteOn(1, note, 0.8f);
            activeNotes[static_cast<std::size_t>(i)] = note;
        }
        activeNoteCount.store(noteCount, std::memory_order_relaxed);

        newChordReady.store(false, std::memory_order_release);
    }

    // Render piano synth audio into buffer
    pianoSynth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
// State persistence — custom ValueTree (NO APVTS)
// API key stored separately via appProperties (not saved into DAW project).
//==============================================================================

void ChordGPTProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Snapshot current values into the ValueTree before serialising
    pluginState.setProperty("temperature",         temperature,             nullptr);
    pluginState.setProperty("voicing",             voicing,                 nullptr);
    pluginState.setProperty("conversationHistory", conversationHistoryJson, nullptr);

    std::unique_ptr<juce::XmlElement> xml(pluginState.createXml());
    if (xml != nullptr)
        copyXmlToBinary(*xml, destData);
}

void ChordGPTProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(pluginState.getType()))
    {
        pluginState = juce::ValueTree::fromXml(*xmlState);

        temperature             = static_cast<float>(pluginState.getProperty("temperature",         0.7f));
        voicing                 = static_cast<int>  (pluginState.getProperty("voicing",             0));
        conversationHistoryJson = pluginState.getProperty("conversationHistory", juce::String("[]")).toString();
    }
}

//==============================================================================
// AsyncUpdater — called on message thread after background API call completes
//==============================================================================

void ChordGPTProcessor::handleAsyncUpdate()
{
    juce::String chordName;
    std::vector<int> notes;
    juce::String errorMsg;
    bool isError = false;

    {
        juce::ScopedLock lock(resultLock);
        chordName = pendingChordName;
        notes     = pendingChordNotesResult;
        errorMsg  = pendingErrorMessage;
        isError   = pendingIsError;
    }

    apiCallInProgress.store(false, std::memory_order_release);

    if (isError)
    {
        if (onApiError)
            onApiError(errorMsg);
    }
    else
    {
        // Trigger chord playback (lock-free MIDI injection in processBlock)
        triggerChord(notes);

        if (onChordReceived)
            onChordReceived(chordName, notes);
    }
}

//==============================================================================
// Public API
//==============================================================================

void ChordGPTProcessor::requestChord(const juce::String& prompt)
{
    // Prevent concurrent calls
    if (apiCallInProgress.exchange(true))
        return;

    // Join any previous thread
    if (apiThread && apiThread->joinable())
        apiThread->join();

    // Copy prompt for lambda capture
    juce::String capturedPrompt = prompt;

    apiThread = std::make_unique<std::thread>([this, capturedPrompt]()
    {
        performApiCall(capturedPrompt);
    });
}

void ChordGPTProcessor::clearConversation()
{
    conversationHistoryJson = "[]";
    pluginState.setProperty("conversationHistory", conversationHistoryJson, nullptr);
    DBG("ChordGPT: conversation cleared");
}

void ChordGPTProcessor::triggerChord(const std::vector<int>& midiNotes)
{
    // Write notes into lock-free queue; processBlock polls newChordReady flag
    const int count = juce::jmin(static_cast<int>(midiNotes.size()), 8);
    for (int i = 0; i < count; ++i)
        pendingChordNotes[static_cast<std::size_t>(i)] = midiNotes[static_cast<std::size_t>(i)];
    pendingNoteCount.store(count, std::memory_order_relaxed);

    // Release store — processBlock will see this on next block
    newChordReady.store(true, std::memory_order_release);
}

void ChordGPTProcessor::setTemperature(float temp)
{
    temperature = juce::jlimit(0.0f, 1.5f, temp);
    pluginState.setProperty("temperature", temperature, nullptr);
    DBG("ChordGPT: temperature set to " << temperature);
}

void ChordGPTProcessor::setVoicing(int voicingIndex)
{
    voicing = juce::jlimit(0, 3, voicingIndex);
    pluginState.setProperty("voicing", voicing, nullptr);
    DBG("ChordGPT: voicing set to " << voicing);
}

void ChordGPTProcessor::setApiKey(const juce::String& apiKey)
{
    if (auto* props = appProperties.getUserSettings())
    {
        props->setValue("openai_api_key", apiKey);
        props->saveIfNeeded();
    }
    DBG("ChordGPT: API key stored");
}

juce::String ChordGPTProcessor::getApiKey() const
{
    if (auto* props = appProperties.getUserSettings())
        return props->getValue("openai_api_key", {});
    return {};
}

juce::String ChordGPTProcessor::getConversationHistoryJson() const
{
    return conversationHistoryJson;
}

//==============================================================================
// Internal — background API call (stub — full implementation in Stage 2 DSP)
//==============================================================================

void ChordGPTProcessor::performApiCall(const juce::String& prompt)
{
    // Stage 1 stub: log the call and return an empty result.
    // Stage 2 DSP agent will implement the actual juce::URL POST to OpenAI.
    DBG("ChordGPT: performApiCall called with prompt: " << prompt);

    {
        juce::ScopedLock lock(resultLock);
        pendingChordName         = "Cmaj7";
        pendingChordNotesResult  = { 60, 64, 67, 71 };
        pendingErrorMessage      = {};
        pendingIsError           = false;
    }

    // Notify message thread
    triggerAsyncUpdate();
}

bool ChordGPTProcessor::parseGptResponse(const juce::String& responseJson,
                                          juce::String& outChordName,
                                          std::vector<int>& outNotes,
                                          juce::String& outError)
{
    // Stage 1 stub — full JSON parsing in Stage 2 DSP
    juce::ignoreUnused(responseJson);
    outChordName = "Cmaj7";
    outNotes     = { 60, 64, 67, 71 };
    outError     = {};
    return true;
}

std::vector<int> ChordGPTProcessor::buildChordNotes(int rootMidi,
                                                      const juce::String& quality) const
{
    // Stage 1 stub — chord voicing engine implemented in Stage 2 DSP
    juce::ignoreUnused(quality);
    return { rootMidi, rootMidi + 4, rootMidi + 7, rootMidi + 11 };
}

//==============================================================================
// Editor creation
//==============================================================================

juce::AudioProcessorEditor* ChordGPTProcessor::createEditor()
{
    return new ChordGPTEditor(*this);
}

//==============================================================================
// Factory function (required by JUCE)
//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordGPTProcessor();
}
