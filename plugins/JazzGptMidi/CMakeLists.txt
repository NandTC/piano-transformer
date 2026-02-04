cmake_minimum_required(VERSION 3.15)

# Plugin formats: VST3, AU, Standalone
juce_add_plugin(JazzGptMidi
    COMPANY_NAME "Nobody And The Computer"
    BUNDLE_ID "com.NobodyAndTheComputer.JazzGptMidi"
    PLUGIN_MANUFACTURER_CODE Namc
    PLUGIN_CODE Jgpm
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "JazzGptMidi"
    NEEDS_WEB_BROWSER TRUE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# Generate JuceHeader.h
juce_generate_juce_header(JazzGptMidi)

# WebView UI Resources
juce_add_binary_data(JazzGptMidi_UIResources
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/css/styles.css
        Source/ui/public/js/app.js
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
)

# Source files
target_sources(JazzGptMidi
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# Required JUCE modules
target_link_libraries(JazzGptMidi
    PRIVATE
        JazzGptMidi_UIResources
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compile definitions
target_compile_definitions(JazzGptMidi
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
)
