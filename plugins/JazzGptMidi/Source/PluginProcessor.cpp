#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <regex>

JazzGptMidiAudioProcessor::JazzGptMidiAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Initialize piano synth with voices
    for (int i = 0; i < 16; ++i)
        pianoSynth.addVoice(new SimplePianoVoice());

    pianoSynth.addSound(new SimplePianoSound());
}

JazzGptMidiAudioProcessor::~JazzGptMidiAudioProcessor()
{
}

void JazzGptMidiAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    pianoSynth.setCurrentPlaybackSampleRate(sampleRate);
}

void JazzGptMidiAudioProcessor::releaseResources()
{
    pianoSynth.allNotesOff(0, false);
}

void JazzGptMidiAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Process playback if active
    if (playing)
    {
        juce::ScopedLock lock(playbackLock);

        // Calculate current playback position
        double currentTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
        double elapsedTime = currentTime - playbackStartTime;
        double nextBeat = (elapsedTime / 60.0) * currentTempo;

        // Calculate block duration in beats
        int numSamples = buffer.getNumSamples();
        double blockDuration = (numSamples / getSampleRate()) * (currentTempo / 60.0);

        // Trigger scheduled notes
        juce::MidiBuffer tempMidi;

        for (auto& note : scheduledNotes)
        {
            // Check if note should start in this block (between currentBeat and nextBeat)
            if (note.startBeat >= currentBeat && note.startBeat < nextBeat + blockDuration)
            {
                // Calculate sample position within block
                double beatOffset = note.startBeat - currentBeat;
                int sampleOffset = juce::jmax(0, static_cast<int>((beatOffset / (currentTempo / 60.0)) * getSampleRate()));
                sampleOffset = juce::jmin(sampleOffset, numSamples - 1);

                tempMidi.addEvent(juce::MidiMessage::noteOn(1, note.midiNote, static_cast<juce::uint8>(note.velocity * 127)), sampleOffset);
            }

            // Check if note should stop
            double endBeat = note.startBeat + note.duration;
            if (endBeat >= currentBeat && endBeat < nextBeat + blockDuration)
            {
                // Calculate sample position within block
                double beatOffset = endBeat - currentBeat;
                int sampleOffset = juce::jmax(0, static_cast<int>((beatOffset / (currentTempo / 60.0)) * getSampleRate()));
                sampleOffset = juce::jmin(sampleOffset, numSamples - 1);

                tempMidi.addEvent(juce::MidiMessage::noteOff(1, note.midiNote), sampleOffset);
            }
        }

        // Render synth
        pianoSynth.renderNextBlock(buffer, tempMidi, 0, numSamples);

        // Update current beat for next block
        currentBeat = nextBeat;

        // Check if playback is complete (all notes have finished)
        if (!scheduledNotes.empty())
        {
            // Find the end beat of the last note
            double lastNoteBeat = 0.0;
            for (const auto& note : scheduledNotes)
            {
                double endBeat = note.startBeat + note.duration;
                if (endBeat > lastNoteBeat)
                    lastNoteBeat = endBeat;
            }

            // Stop playback if we've passed the last note
            if (currentBeat >= lastNoteBeat + 0.5) // Add 0.5 beat buffer for release
            {
                stopPlayback();
            }
        }
    }
}

juce::AudioProcessorEditor* JazzGptMidiAudioProcessor::createEditor()
{
    return new JazzGptMidiAudioProcessorEditor(*this);
}

void JazzGptMidiAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save composition text to plugin state
    juce::ValueTree state("JazzGptMidiState");
    state.setProperty("compositionText", compositionText, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void JazzGptMidiAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Restore composition text from plugin state
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName("JazzGptMidiState"))
    {
        auto state = juce::ValueTree::fromXml(*xmlState);
        compositionText = state.getProperty("compositionText").toString();
        // Note: Text will be sent to WebView after editor is created
    }
}

// Factory function
//==============================================================================
// Text Parser Implementation
//==============================================================================

CompositionData JazzGptMidiAudioProcessor::parseComposition(const juce::String& text)
{
    CompositionData data;

    if (text.trim().isEmpty())
    {
        data.errorMessage = "Composition text is empty";
        return data;
    }

    auto lines = juce::StringArray::fromLines(text);
    int lineIndex = 0;

    try
    {
        // Parse header
        auto header = parseHeader(lines, lineIndex);
        data.title = header.title;
        data.tempo = header.tempo;
        data.key = header.key;
        data.timeSignature = header.timeSignature;
        data.swing = header.swing;
        data.bars = header.bars;
        data.beatsPerBar = header.beatsPerBar;

        // Parse chords
        data.chords = parseChords(lines, lineIndex, data.beatsPerBar);

        // Parse melody
        data.melody = parseMelody(lines, lineIndex, data.beatsPerBar);

        data.isValid = true;
    }
    catch (const std::exception& e)
    {
        data.errorMessage = e.what();
        data.isValid = false;
    }

    return data;
}

JazzGptMidiAudioProcessor::ParsedHeader JazzGptMidiAudioProcessor::parseHeader(
    const juce::StringArray& lines, int& lineIndex)
{
    ParsedHeader header;

    std::regex headerPattern(R"(^(\w+):\s*(.+)$)");

    while (lineIndex < lines.size())
    {
        auto line = lines[lineIndex++].trim();
        if (line.isEmpty()) continue;

        std::string lineStr = line.toStdString();
        std::smatch match;

        if (std::regex_match(lineStr, match, headerPattern))
        {
            juce::String key = juce::String(match[1].str()).toUpperCase();
            juce::String value = juce::String(match[2].str()).trim();

            if (key == "TITLE") header.title = value;
            else if (key == "TEMPO") header.tempo = value.getIntValue();
            else if (key == "KEY") header.key = value;
            else if (key == "TIME")
            {
                header.timeSignature = value;
                auto parts = juce::StringArray::fromTokens(value, "/", "");
                if (parts.size() == 2)
                    header.beatsPerBar = parts[0].getIntValue();
            }
            else if (key == "SWING") header.swing = value;
            else if (key == "BARS") header.bars = value.getIntValue();
        }
        else if (line.toUpperCase().startsWith("CHORDS"))
        {
            break;
        }
    }

    if (header.tempo <= 0) header.tempo = 120;
    if (header.beatsPerBar <= 0) header.beatsPerBar = 4;

    return header;
}

std::vector<ChordData> JazzGptMidiAudioProcessor::parseChords(
    const juce::StringArray& lines, int& lineIndex, int beatsPerBar)
{
    std::vector<ChordData> chords;

    std::regex barPattern(R"(^bar\s+(\d+):\s*(.+)$)", std::regex::icase);
    std::regex chordWithDuration(R"(([A-G][#b]?(?:maj|min|m|dim|aug|sus)?[0-9#b]*)\(([0-9.]+)\))");

    while (lineIndex < lines.size())
    {
        auto line = lines[lineIndex++].trim();
        if (line.isEmpty()) continue;

        if (line.toUpperCase().startsWith("MELODY"))
            break;

        std::string lineStr = line.toStdString();
        std::smatch match;

        if (std::regex_match(lineStr, match, barPattern))
        {
            int bar = std::stoi(match[1].str());
            std::string chordStr = match[2].str();

            double barStart = (bar - 1) * beatsPerBar;

            // Try to match chords with durations
            std::sregex_iterator iter(chordStr.begin(), chordStr.end(), chordWithDuration);
            std::sregex_iterator end;

            if (iter != end)
            {
                // Multiple chords with durations
                double currentBeat = barStart;
                while (iter != end)
                {
                    ChordData chord;
                    chord.symbol = juce::String((*iter)[1].str());
                    chord.duration = std::stod((*iter)[2].str());
                    chord.startBeat = currentBeat;
                    chord.bar = bar;

                    chords.push_back(chord);
                    currentBeat += chord.duration;
                    ++iter;
                }
            }
            else
            {
                // Single chord, full bar duration
                ChordData chord;
                chord.symbol = juce::String(chordStr).trim();
                chord.duration = beatsPerBar;
                chord.startBeat = barStart;
                chord.bar = bar;
                chords.push_back(chord);
            }
        }
    }

    return chords;
}

std::vector<NoteData> JazzGptMidiAudioProcessor::parseMelody(
    const juce::StringArray& lines, int& lineIndex, int beatsPerBar)
{
    std::vector<NoteData> melody;

    std::regex notePattern(R"(^bar\s+(\d+)\s+beat\s+([\d.]+):\s*([A-G][#b]?\d)\s+duration\s+([\d.]+)$)", std::regex::icase);

    while (lineIndex < lines.size())
    {
        auto line = lines[lineIndex++].trim();
        if (line.isEmpty()) continue;

        std::string lineStr = line.toStdString();
        std::smatch match;

        if (std::regex_match(lineStr, match, notePattern))
        {
            NoteData note;
            note.bar = std::stoi(match[1].str());
            note.beat = std::stod(match[2].str());
            note.pitch = juce::String(match[3].str());
            note.duration = std::stod(match[4].str());

            // Calculate absolute beat position
            double barStart = (note.bar - 1) * beatsPerBar;
            note.startBeat = barStart + (note.beat - 1.0);

            // Parse MIDI note number
            note.midiNote = parseNoteName(note.pitch);

            if (note.midiNote >= 0 && note.duration > 0)
                melody.push_back(note);
        }
    }

    return melody;
}

//==============================================================================
// Chord Symbol Expander
//==============================================================================

int JazzGptMidiAudioProcessor::parseNoteName(const juce::String& noteName)
{
    if (noteName.length() < 2) return -1;

    // Map note names to semitones (C=0, C#=1, D=2, etc.)
    std::unordered_map<char, int> noteMap = {
        {'C', 0}, {'D', 2}, {'E', 4}, {'F', 5}, {'G', 7}, {'A', 9}, {'B', 11}
    };

    char noteLetter = noteName[0];
    if (noteMap.find(noteLetter) == noteMap.end()) return -1;

    int semitone = noteMap[noteLetter];
    int index = 1;

    // Check for accidental
    if (index < noteName.length())
    {
        if (noteName[index] == '#')
        {
            semitone++;
            index++;
        }
        else if (noteName[index] == 'b')
        {
            semitone--;
            index++;
        }
    }

    // Parse octave
    int octave = noteName.substring(index).getIntValue();

    // Calculate MIDI note (Middle C = C4 = 60)
    int midiNote = (octave + 1) * 12 + semitone;

    return (midiNote >= 0 && midiNote <= 127) ? midiNote : -1;
}

std::vector<int> JazzGptMidiAudioProcessor::expandChord(const juce::String& symbol)
{
    std::vector<int> pitches;

    // Parse root note (first 1-2 characters)
    int rootIndex = 1;
    if (symbol.length() > 1 && (symbol[1] == '#' || symbol[1] == 'b'))
        rootIndex = 2;

    juce::String rootStr = symbol.substring(0, rootIndex) + "3"; // Default octave 3
    int rootMidi = parseNoteName(rootStr);

    if (rootMidi < 0) return pitches;

    // Parse quality
    juce::String quality = symbol.substring(rootIndex).toLowerCase();

    // Interval map for common jazz chords
    std::unordered_map<std::string, std::vector<int>> intervalMap = {
        {"maj", {0, 4, 7}},
        {"", {0, 4, 7}},
        {"min", {0, 3, 7}},
        {"m", {0, 3, 7}},
        {"dim", {0, 3, 6}},
        {"aug", {0, 4, 8}},
        {"7", {0, 4, 7, 10}},
        {"maj7", {0, 4, 7, 11}},
        {"m7", {0, 3, 7, 10}},
        {"m7b5", {0, 3, 6, 10}},
        {"dim7", {0, 3, 6, 9}},
        {"sus4", {0, 5, 7}},
        {"sus2", {0, 2, 7}},
        {"9", {0, 4, 7, 10, 14}},
        {"maj9", {0, 4, 7, 11, 14}},
        {"m9", {0, 3, 7, 10, 14}},
        {"11", {0, 4, 7, 10, 14, 17}},
        {"13", {0, 4, 7, 10, 14, 21}}
    };

    std::vector<int> intervals;

    // Try to find exact match
    auto it = intervalMap.find(quality.toStdString());
    if (it != intervalMap.end())
    {
        intervals = it->second;
    }
    else
    {
        // Fallback: try to parse basic quality
        if (quality.contains("maj7"))
            intervals = {0, 4, 7, 11};
        else if (quality.contains("m7") || quality.contains("min7"))
            intervals = {0, 3, 7, 10};
        else if (quality.contains("7"))
            intervals = {0, 4, 7, 10};
        else if (quality.contains("m") || quality.contains("min"))
            intervals = {0, 3, 7};
        else
            intervals = {0, 4, 7}; // Default to major triad
    }

    // Build MIDI pitches
    for (int interval : intervals)
        pitches.push_back(rootMidi + interval);

    return pitches;
}

//==============================================================================
// MIDI File Generator
//==============================================================================

juce::MidiFile JazzGptMidiAudioProcessor::createMidiFile(const CompositionData& data, ExportMode mode)
{
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(480);

    juce::MidiMessageSequence track;

    // Add tempo meta event
    double microsecondsPerQuarterNote = 60000000.0 / data.tempo;
    track.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(microsecondsPerQuarterNote)), 0.0);

    // Add time signature
    auto timeParts = juce::StringArray::fromTokens(data.timeSignature, "/", "");
    int numerator = timeParts.size() > 0 ? timeParts[0].getIntValue() : 4;
    int denominator = timeParts.size() > 1 ? timeParts[1].getIntValue() : 4;
    track.addEvent(juce::MidiMessage::timeSignatureMetaEvent(numerator, denominator), 0.0);

    // Add melody notes
    if (mode == ExportMode::Melody || mode == ExportMode::Full)
    {
        for (const auto& note : data.melody)
        {
            int startTick = beatsToTicks(note.startBeat);
            int endTick = beatsToTicks(note.startBeat + note.duration);

            track.addEvent(juce::MidiMessage::noteOn(1, note.midiNote, static_cast<juce::uint8>(90)), startTick);
            track.addEvent(juce::MidiMessage::noteOff(1, note.midiNote), endTick);
        }
    }

    // Add chord notes
    if (mode == ExportMode::Chords || mode == ExportMode::Full)
    {
        for (const auto& chord : data.chords)
        {
            auto pitches = expandChord(chord.symbol);
            int startTick = beatsToTicks(chord.startBeat);
            int endTick = beatsToTicks(chord.startBeat + chord.duration);

            for (int pitch : pitches)
            {
                if (pitch >= 0 && pitch <= 127)
                {
                    track.addEvent(juce::MidiMessage::noteOn(1, pitch, static_cast<juce::uint8>(70)), startTick);
                    track.addEvent(juce::MidiMessage::noteOff(1, pitch), endTick);
                }
            }
        }
    }

    track.updateMatchedPairs();
    midiFile.addTrack(track);

    return midiFile;
}

juce::File JazzGptMidiAudioProcessor::exportMIDI(const CompositionData& data, ExportMode mode, const juce::File& targetFile)
{
    if (!data.isValid)
        return juce::File();

    auto midiFile = createMidiFile(data, mode);

    if (targetFile != juce::File())
    {
        juce::FileOutputStream stream(targetFile);
        if (stream.openedOk())
        {
            midiFile.writeTo(stream);
            return targetFile;
        }
    }

    return juce::File();
}

//==============================================================================
// Transport System
//==============================================================================

void JazzGptMidiAudioProcessor::playComposition(const CompositionData& data)
{
    if (!data.isValid) return;

    juce::ScopedLock lock(playbackLock);

    // Clear previous notes
    scheduledNotes.clear();
    pianoSynth.allNotesOff(0, false);

    // Schedule melody notes
    for (const auto& note : data.melody)
    {
        ScheduledNote sn;
        sn.midiNote = note.midiNote;
        sn.startBeat = note.startBeat;
        sn.duration = note.duration;
        sn.velocity = 0.8f;
        scheduledNotes.push_back(sn);
    }

    // Schedule chord notes
    for (const auto& chord : data.chords)
    {
        auto pitches = expandChord(chord.symbol);
        for (int pitch : pitches)
        {
            if (pitch >= 0 && pitch <= 127)
            {
                ScheduledNote sn;
                sn.midiNote = pitch;
                sn.startBeat = chord.startBeat;
                sn.duration = chord.duration;
                sn.velocity = 0.6f;
                scheduledNotes.push_back(sn);
            }
        }
    }

    // Start playback
    currentTempo = data.tempo;
    playbackStartTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    currentBeat = 0.0;
    playing = true;
}

void JazzGptMidiAudioProcessor::stopPlayback()
{
    juce::ScopedLock lock(playbackLock);

    playing = false;
    scheduledNotes.clear();
    pianoSynth.allNotesOff(0, false);
    currentBeat = 0.0;
}

//==============================================================================
// Factory
//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JazzGptMidiAudioProcessor();
}
