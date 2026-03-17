#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
// ChordGPTEditor constructor
//==============================================================================

ChordGPTEditor::ChordGPTEditor(ChordGPTProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // No APVTS relays needed — all communication uses withNativeFunction() callbacks.

    // Create WebBrowserComponent with native integration and resource provider (Pattern #11)
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withResourceProvider(
                [this](const juce::String& url) { return getResource(url); },
                juce::WebBrowserComponent::getResourceProviderRoot())

            //------------------------------------------------------------------
            // Native function: requestChord(prompt)
            // Called from JS: requestChord("A dark forest, tense and foggy")
            //------------------------------------------------------------------
            .withNativeFunction(
                "requestChord",
                [this](const juce::Array<juce::var>& args,
                       juce::WebBrowserComponent::NativeFunctionCompletion completion)
                {
                    if (args.size() >= 1)
                    {
                        const juce::String prompt = args[0].toString();
                        processorRef.requestChord(prompt);
                    }
                    completion("ok");
                })

            //------------------------------------------------------------------
            // Native function: newScene()
            // Clears conversation history and resets chord log in UI.
            //------------------------------------------------------------------
            .withNativeFunction(
                "newScene",
                [this](const juce::Array<juce::var>& /*args*/,
                       juce::WebBrowserComponent::NativeFunctionCompletion completion)
                {
                    processorRef.clearConversation();
                    completion("ok");
                })

            //------------------------------------------------------------------
            // Native function: setTemperature(value)
            //------------------------------------------------------------------
            .withNativeFunction(
                "setTemperature",
                [this](const juce::Array<juce::var>& args,
                       juce::WebBrowserComponent::NativeFunctionCompletion completion)
                {
                    if (args.size() >= 1)
                        processorRef.setTemperature(static_cast<float>(args[0]));
                    completion("ok");
                })

            //------------------------------------------------------------------
            // Native function: setVoicing(index)
            //------------------------------------------------------------------
            .withNativeFunction(
                "setVoicing",
                [this](const juce::Array<juce::var>& args,
                       juce::WebBrowserComponent::NativeFunctionCompletion completion)
                {
                    if (args.size() >= 1)
                        processorRef.setVoicing(static_cast<int>(args[0]));
                    completion("ok");
                })

            //------------------------------------------------------------------
            // Native function: setApiKey(key)
            // Stores key via ApplicationProperties (not in DAW project state).
            //------------------------------------------------------------------
            .withNativeFunction(
                "setApiKey",
                [this](const juce::Array<juce::var>& args,
                       juce::WebBrowserComponent::NativeFunctionCompletion completion)
                {
                    if (args.size() >= 1)
                        processorRef.setApiKey(args[0].toString());
                    completion("ok");
                })

            //------------------------------------------------------------------
            // Native function: getSettings()
            // Returns current temperature, voicing, and whether an API key is set.
            // Used by UI on load to restore control state.
            //------------------------------------------------------------------
            .withNativeFunction(
                "getSettings",
                [this](const juce::Array<juce::var>& /*args*/,
                       juce::WebBrowserComponent::NativeFunctionCompletion completion)
                {
                    const juce::String apiKey = processorRef.getApiKey();
                    const bool hasApiKey      = apiKey.isNotEmpty();

                    juce::DynamicObject* obj = new juce::DynamicObject();
                    obj->setProperty("temperature", processorRef.getTemperature());
                    obj->setProperty("voicing",     processorRef.getVoicing());
                    obj->setProperty("hasApiKey",   hasApiKey);

                    completion(juce::JSON::toString(juce::var(obj)));
                })
    );

    addAndMakeVisible(*webView);

    // Navigate to resource provider root — serves index.html
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    setSize(600, 700);
}

ChordGPTEditor::~ChordGPTEditor()
{
    // webView destroyed by unique_ptr destructor
}

//==============================================================================
// paint / resized
//==============================================================================

void ChordGPTEditor::paint(juce::Graphics& g)
{
    // WebView fills the entire area; paint only provides a background fallback.
    g.fillAll(juce::Colour(0xff0a0a0a));
}

void ChordGPTEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds(getLocalBounds());
}

//==============================================================================
// Resource provider — explicit URL mapping (Pattern #8)
//
// BinaryData flattens directory paths to valid C++ identifiers:
//   index.html               -> index_html
//   css/styles.css           -> styles_css
//   js/juce/index.js         -> index_js
//   js/juce/check_native_interop.js -> check_native_interop_js
//==============================================================================

std::optional<juce::WebBrowserComponent::Resource>
ChordGPTEditor::getResource(const juce::String& url)
{
    // Helper: wrap BinaryData pointer + size into the Resource vector<byte> type
    auto makeResource = [](const char* data, int size, const juce::String& mimeType)
        -> std::optional<juce::WebBrowserComponent::Resource>
    {
        return juce::WebBrowserComponent::Resource {
            std::vector<std::byte>(
                reinterpret_cast<const std::byte*>(data),
                reinterpret_cast<const std::byte*>(data) + size),
            mimeType
        };
    };

    // Strip query strings if present
    const juce::String path = url.upToFirstOccurrenceOf("?", false, false);

    if (path == "/" || path == "/index.html")
        return makeResource(BinaryData::index_html, BinaryData::index_htmlSize,
                            "text/html");

    if (path == "/css/styles.css")
        return makeResource(BinaryData::styles_css, BinaryData::styles_cssSize,
                            "text/css");

    if (path == "/js/juce/index.js")
        return makeResource(BinaryData::index_js, BinaryData::index_jsSize,
                            "text/javascript");

    if (path == "/js/juce/check_native_interop.js")
        return makeResource(BinaryData::check_native_interop_js,
                            BinaryData::check_native_interop_jsSize,
                            "text/javascript");

    // 404 — resource not found
    return std::nullopt;
}
