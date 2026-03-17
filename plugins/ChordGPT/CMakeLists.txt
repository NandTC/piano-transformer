cmake_minimum_required(VERSION 3.22)

# Parent CMakeLists.txt sets up JUCE and calls add_subdirectory on this folder.
# Do NOT add project() or add_subdirectory(JUCE) here.

juce_add_plugin(ChordGPT
    COMPANY_NAME "NobodyAndTheComputer"
    BUNDLE_ID "com.NobodyAndTheComputer.ChordGPT"
    PLUGIN_MANUFACTURER_CODE Naac
    PLUGIN_CODE Cgpt
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ChordGPT"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    NEEDS_WEB_BROWSER TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# WebView UI Resources
juce_add_binary_data(ChordGPT_UIResources
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/css/styles.css
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
)

# Source files
target_sources(ChordGPT
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# Include paths
target_include_directories(ChordGPT
    PRIVATE
        Source
)

# Required JUCE modules
target_link_libraries(ChordGPT
    PRIVATE
        ChordGPT_UIResources
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

# CRITICAL: juce_generate_juce_header MUST come after target_link_libraries (JUCE 8 requirement)
juce_generate_juce_header(ChordGPT)

# Compile definitions
target_compile_definitions(ChordGPT
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
)
