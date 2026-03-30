cmake_minimum_required(VERSION 3.24)
project(Aconcagua VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(C:/JUCE ${CMAKE_BINARY_DIR}/JUCE)

juce_add_gui_app(Aconcagua
    PRODUCT_NAME "Aconcagua"
    COMPANY_NAME "Aconcagua"
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
)

target_sources(Aconcagua PRIVATE
    Source/Main.cpp
    Source/MainWindow.cpp
    Source/PluginHost/PluginLoader.cpp
    Source/PluginHost/BenchmarkEngine.cpp
    Source/Model/ResultsExporter.cpp
    Source/UI/AconcaguaLookAndFeel.cpp
    Source/UI/ConfigTab.cpp
    Source/UI/ResultsTab.cpp
    Source/UI/HistogramTab.cpp
)

target_compile_definitions(Aconcagua PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Aconcagua,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Aconcagua,VERSION>"
)

target_link_libraries(Aconcagua PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
