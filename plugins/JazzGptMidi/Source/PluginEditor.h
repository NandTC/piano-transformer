#pragma once
#include "PluginProcessor.h"
#include <optional>

class JazzGptMidiAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit JazzGptMidiAudioProcessorEditor(JazzGptMidiAudioProcessor&);
    ~JazzGptMidiAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    JazzGptMidiAudioProcessor& processorRef;

    // WebView component (no unique_ptr needed for simple ownership)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // File chooser for MIDI export (must persist for async callback)
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Resource provider for serving HTML/CSS/JS
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    // Mode detection helper
    bool isStandalone() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JazzGptMidiAudioProcessorEditor)
};
