cmake_minimum_required(VERSION 3.22)
project(JWHorror VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
# Avoid dependency on a separately installed MSVC runtime on Windows.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
set(JW_FORMATS VST3 Standalone)
if(APPLE)
 list(APPEND JW_FORMATS AU)
endif()
juce_add_plugin(JWHorror COMPANY_NAME "JW Audio Labs"
 BUNDLE_ID "com.jwaudiolabs.horror" PLUGIN_MANUFACTURER_CODE Jwal PLUGIN_CODE Jhor
 PRODUCT_NAME "JW Horror" IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE
 NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
 COPY_PLUGIN_AFTER_BUILD FALSE FORMATS ${JW_FORMATS}
 VST3_CATEGORIES Instrument Synth)
juce_generate_juce_header(JWHorror)
target_sources(JWHorror PRIVATE Source/Plugin.cpp)
target_compile_definitions(JWHorror PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(JWHorror PRIVATE juce::juce_audio_utils juce::juce_dsp
 PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
