cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

if(APPLE)
    project(CyberDenoiserPro VERSION 1.4 LANGUAGES CXX C OBJC OBJCXX)
else()
    project(CyberDenoiserPro VERSION 1.4 LANGUAGES CXX C)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Fetch JUCE automatically
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12  # Fixed for macOS Sequoia+ compatibility
)
FetchContent_MakeAvailable(JUCE)

# 2. Add the Plugin Target
juce_add_plugin(CyberDenoiserPro
    COMPANY_NAME "Danny Marshall"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_AFTER_BUILD TRUE
    BUNDLE_ID com.dannymarshall.cyberdenoiserpro
    PLUGIN_MANUFACTURER_CODE DANM
    PLUGIN_CODE CDBP
    FORMATS AU VST3
    PRODUCT_NAME "Cyber-Denoiser PRO"
    AU_DESCRIPTION "Professional 10-Band Forensic Denoiser"
)

set_target_properties(CyberDenoiserPro PROPERTIES 
    JUCE_VST3_CAN_REPLACE_VST2 OFF
    JUCE_WEB_BROWSER OFF
)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(CyberDenoiserPro PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(CyberDenoiserPro PRIVATE ${GTK3_LIBRARIES})
endif()

# 3. Add Source Files
target_sources(CyberDenoiserPro PRIVATE
    Source/CyberDenoiserProcessor.h
    Source/CyberDenoiserProcessor.cpp
    Source/CyberDenoiserEditor.h
    Source/CyberDenoiserEditor.cpp
)

# 4. Link JUCE Modules
target_link_libraries(CyberDenoiserPro PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_core
)

# 5. Universal Binary & Logic Pro Stability (M1 - M5 + Rosetta)
if(APPLE)
    # Use -O3 and standard optimizations. 
    target_compile_options(CyberDenoiserPro PRIVATE -O3)
    
    # Link Accelerate for vDSP and hardware-specific M1-M5 optimizations
    target_link_libraries(CyberDenoiserPro PRIVATE "-framework Accelerate")
    
    message(STATUS "Cyber-Denoiser PRO configured as Universal Binary (arm64 + x86_64) with Accelerate")
endif()
