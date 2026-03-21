cmake_minimum_required(VERSION 3.20)
project(CyberDenoiserPro VERSION 1.4.0)

# Configure caching for FetchContent
set(FETCHCONTENT_QUIET FALSE)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
)
FetchContent_MakeAvailable(juce)

# Create the Stereo plugin target
juce_add_plugin(CyberDenoiserPro
    COMPANY_NAME "Danny Marshall"
    BUNDLE_ID "com.dannymarshall.cyberdenoiserpro"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE DMrs
    PLUGIN_CODE CyDn
    FORMATS VST3 Standalone CLAP AU
    PRODUCT_NAME "Cyber-Denoiser PRO"
)

# Create the Mono plugin target
juce_add_plugin(CyberDenoiserProMono
    COMPANY_NAME "Danny Marshall"
    BUNDLE_ID "com.dannymarshall.cyberdenoiserpromono"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE DMrs
    PLUGIN_CODE CyD1
    FORMATS VST3 Standalone CLAP AU
    PRODUCT_NAME "Cyber-Denoiser PRO Mono"
)

juce_generate_juce_header(CyberDenoiserPro)
juce_generate_juce_header(CyberDenoiserProMono)

# Common source files
set(PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

# Stereo Target setup
target_sources(CyberDenoiserPro PRIVATE ${PLUGIN_SOURCES})
target_compile_features(CyberDenoiserPro PRIVATE cxx_std_20)
target_compile_definitions(CyberDenoiserPro PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(CyberDenoiserPro
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Mono Target setup
target_sources(CyberDenoiserProMono PRIVATE ${PLUGIN_SOURCES})
target_compile_features(CyberDenoiserProMono PRIVATE cxx_std_20)
target_compile_definitions(CyberDenoiserProMono PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0 CYBER_DENOISER_MONO_ONLY=1)
target_link_libraries(CyberDenoiserProMono
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)