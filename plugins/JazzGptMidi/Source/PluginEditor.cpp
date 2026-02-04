#include "PluginEditor.h"
#include "BinaryData.h"

JazzGptMidiAudioProcessorEditor::JazzGptMidiAudioProcessorEditor(JazzGptMidiAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // Create WebView with native integration
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withResourceProvider([this](const auto& url) { return getResource(url); })
            .withNativeFunction("playComposition", [this](auto& var, auto /*args*/) -> juce::var {
                // Extract text from arguments
                if (var.size() > 0)
                {
                    juce::String text = var[0].toString();

                    // Parse composition
                    auto data = processorRef.parseComposition(text);

                    if (data.isValid)
                    {
                        // Send preview data to JavaScript
                        juce::String jsCode = juce::String::formatted(
                            R"(window.updatePreview({
                                tempo: %d,
                                key: '%s',
                                time: '%s',
                                bars: %d,
                                notesCount: %d,
                                chordsCount: %d
                            });)",
                            data.tempo,
                            data.key.toRawUTF8(),
                            data.timeSignature.toRawUTF8(),
                            data.bars,
                            (int)data.melody.size(),
                            (int)data.chords.size()
                        );
                        webView->evaluateJavascript(jsCode);

                        // Start playback
                        processorRef.playComposition(data);

                        // Update UI to playing state
                        webView->evaluateJavascript("window.setPlayingState(true);");
                        webView->evaluateJavascript("window.showStatus('Playing...', 'playing');");
                    }
                    else
                    {
                        // Show error
                        juce::String errorJs = juce::String::formatted(
                            "window.showStatus('%s', 'error');",
                            data.errorMessage.replace("'", "\\'").toRawUTF8()
                        );
                        webView->evaluateJavascript(errorJs);
                    }
                }
                return juce::var();
            })
            .withNativeFunction("stopPlayback", [this](auto& /*var*/, auto /*args*/) -> juce::var {
                processorRef.stopPlayback();
                webView->evaluateJavascript("window.setPlayingState(false);");
                webView->evaluateJavascript("window.showStatus('Ready', 'success');");
                return juce::var();
            })
            .withNativeFunction("exportMIDI", [this](auto& var, auto /*args*/) -> juce::var {
                if (var.size() < 2)
                    return juce::var();

                juce::String mode = var[0].toString();
                juce::String text = var[1].toString();

                // Parse composition
                auto data = processorRef.parseComposition(text);

                if (!data.isValid)
                {
                    juce::String errorJs = juce::String::formatted(
                        "window.showStatus('%s', 'error');",
                        data.errorMessage.replace("'", "\\'").toRawUTF8()
                    );
                    webView->evaluateJavascript(errorJs);
                    return juce::var();
                }

                // Determine export mode
                JazzGptMidiAudioProcessor::ExportMode exportMode = JazzGptMidiAudioProcessor::ExportMode::Full;
                juce::String filename = data.title.isEmpty() ? "composition" : data.title;
                filename = filename.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-");

                if (mode == "melody")
                {
                    exportMode = JazzGptMidiAudioProcessor::ExportMode::Melody;
                    filename += "_melody.mid";
                }
                else if (mode == "chords")
                {
                    exportMode = JazzGptMidiAudioProcessor::ExportMode::Chords;
                    filename += "_chords.mid";
                }
                else
                {
                    exportMode = JazzGptMidiAudioProcessor::ExportMode::Full;
                    filename += "_full.mid";
                }

                // Use file chooser for all modes (JUCE 8 async API)
                auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

                fileChooser = std::make_unique<juce::FileChooser>(
                    "Export MIDI File",
                    juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile(filename),
                    "*.mid"
                );

                fileChooser->launchAsync(chooserFlags, [this, data, exportMode](const juce::FileChooser& chooser)
                {
                    auto result = chooser.getResult();

                    if (result == juce::File())
                        return; // User cancelled

                    try
                    {
                        auto exportedFile = processorRef.exportMIDI(data, exportMode, result);

                        if (exportedFile.existsAsFile())
                        {
                            juce::String successJs = juce::String::formatted(
                                "window.showStatus('Exported %s', 'success');",
                                exportedFile.getFileName().toRawUTF8()
                            );
                            webView->evaluateJavascript(successJs);
                        }
                        else
                        {
                            webView->evaluateJavascript("window.showStatus('Export failed', 'error');");
                        }
                    }
                    catch (const std::exception& e)
                    {
                        juce::String errorJs = juce::String::formatted(
                            "window.showStatus('Export error: %s', 'error');",
                            juce::String(e.what()).replace("'", "\\'").toRawUTF8()
                        );
                        webView->evaluateJavascript(errorJs);
                    }
                });

                return juce::var();
            })
            .withNativeFunction("updateCompositionText", [this](auto& var, auto /*args*/) -> juce::var {
                if (var.size() > 0)
                {
                    processorRef.compositionText = var[0].toString();
                }
                return juce::var();
            })
            .withNativeFunction("saveSong", [this](auto& var, auto /*args*/) -> juce::var {
                if (var.size() == 0)
                    return juce::var();

                juce::String text = var[0].toString();

                // Extract title from composition text for default filename
                juce::String filename = "composition";
                auto lines = juce::StringArray::fromLines(text);
                for (const auto& line : lines)
                {
                    if (line.startsWithIgnoreCase("TITLE:"))
                    {
                        filename = line.substring(6).trim();
                        filename = filename.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ");
                        break;
                    }
                }
                filename += ".txt";

                // Open file chooser dialog
                auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

                fileChooser = std::make_unique<juce::FileChooser>(
                    "Save Song",
                    juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile(filename),
                    "*.txt"
                );

                fileChooser->launchAsync(chooserFlags, [this, text](const juce::FileChooser& chooser)
                {
                    auto result = chooser.getResult();
                    if (result == juce::File())
                    {
                        webView->evaluateJavascript("window.showStatus('Save cancelled', 'success');");
                        return;
                    }

                    // Write text to file
                    if (result.replaceWithText(text))
                    {
                        webView->evaluateJavascript("window.showStatus('Song saved!', 'success');");
                    }
                    else
                    {
                        webView->evaluateJavascript("window.showStatus('Save failed', 'error');");
                    }
                });

                return juce::var();
            })
            .withNativeFunction("openURL", [this](auto& var, auto /*args*/) -> juce::var {
                if (var.size() > 0)
                {
                    juce::String url = var[0].toString();
                    juce::URL(url).launchInDefaultBrowser();
                }
                return juce::var();
            })
            .withNativeFunction("loadSong", [this](auto& /*var*/, auto /*args*/) -> juce::var {
                // Open file chooser dialog
                auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

                fileChooser = std::make_unique<juce::FileChooser>(
                    "Load Song",
                    juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
                    "*.txt"
                );

                fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
                {
                    auto result = chooser.getResult();
                    if (result == juce::File())
                    {
                        webView->evaluateJavascript("window.showStatus('Load cancelled', 'success');");
                        return;
                    }

                    // Read text from file
                    juce::String text = result.loadFileAsString();

                    if (text.isNotEmpty())
                    {
                        // Update processor state
                        processorRef.compositionText = text;

                        // Send text to JavaScript to update text area
                        juce::String escapedText = juce::JSON::toString(text);
                        juce::String jsCode = juce::String::formatted(
                            "document.getElementById('composition').value = %s; window.showStatus('Song loaded!', 'success');",
                            escapedText.toRawUTF8()
                        );
                        webView->evaluateJavascript(jsCode);
                    }
                    else
                    {
                        webView->evaluateJavascript("window.showStatus('Load failed - empty file', 'error');");
                    }
                });

                return juce::var();
            })
    );

    addAndMakeVisible(*webView);

    // Navigate to UI
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    // Set editor size
    setSize(480, 540);

    // Restore saved text after a short delay (allow WebView to initialize)
    juce::Timer::callAfterDelay(500, [this]() {
        if (webView != nullptr && !processorRef.compositionText.isEmpty())
        {
            juce::String jsCode = juce::String::formatted(
                "window.restoreText(%s);",
                juce::JSON::toString(processorRef.compositionText).toRawUTF8()
            );
            webView->evaluateJavascript(jsCode);
        }
    });
}

JazzGptMidiAudioProcessorEditor::~JazzGptMidiAudioProcessorEditor()
{
}

void JazzGptMidiAudioProcessorEditor::paint(juce::Graphics& g)
{
    // WebView handles all rendering
    juce::ignoreUnused(g);
}

void JazzGptMidiAudioProcessorEditor::resized()
{
    // WebView fills entire editor
    if (webView != nullptr)
        webView->setBounds(getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource>
JazzGptMidiAudioProcessorEditor::getResource(const juce::String& url)
{
    // Helper to convert binary data to vector
    auto makeVector = [](const char* data, int size) {
        return std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(data),
            reinterpret_cast<const std::byte*>(data) + size
        );
    };

    // Explicit URL mapping (Pattern #8)
    if (url == "/" || url == "/index.html")
    {
        return juce::WebBrowserComponent::Resource{
            makeVector(BinaryData::index_html, BinaryData::index_htmlSize),
            juce::String("text/html")
        };
    }

    if (url == "/css/styles.css")
    {
        return juce::WebBrowserComponent::Resource{
            makeVector(BinaryData::styles_css, BinaryData::styles_cssSize),
            juce::String("text/css")
        };
    }

    if (url == "/js/app.js")
    {
        return juce::WebBrowserComponent::Resource{
            makeVector(BinaryData::app_js, BinaryData::app_jsSize),
            juce::String("application/javascript")
        };
    }

    if (url == "/js/juce/index.js")
    {
        return juce::WebBrowserComponent::Resource{
            makeVector(BinaryData::index_js, BinaryData::index_jsSize),
            juce::String("application/javascript")
        };
    }

    if (url == "/js/juce/check_native_interop.js")
    {
        return juce::WebBrowserComponent::Resource{
            makeVector(BinaryData::check_native_interop_js, BinaryData::check_native_interop_jsSize),
            juce::String("application/javascript")
        };
    }

    // Resource not found
    juce::Logger::writeToLog("Resource not found: " + url);
    return std::nullopt;
}

bool JazzGptMidiAudioProcessorEditor::isStandalone() const
{
    #if JUCE_STANDALONE_APPLICATION
        return true;
    #else
        return false;
    #endif
}
