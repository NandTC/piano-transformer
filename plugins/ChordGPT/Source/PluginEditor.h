#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
// ChordGPTEditor
//
// Hosts a juce::WebBrowserComponent that serves the cinematic HTML/CSS/JS UI.
// All UI <-> processor communication uses withNativeFunction() callbacks.
// No APVTS — state is managed via custom ValueTree in the processor.
//==============================================================================

class ChordGPTEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChordGPTEditor(ChordGPTProcessor&);
    ~ChordGPTEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ChordGPTProcessor& processorRef;

    // WebView — std::unique_ptr (Pattern #11: correct initialization order)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // Resource provider — maps URL paths to BinaryData resources (Pattern #8)
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordGPTEditor)
};
