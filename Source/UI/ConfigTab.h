#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginHost/PluginLoader.h"
#include "../PluginHost/BenchmarkEngine.h"
#include "../PluginHost/IdlePump.h"
#include "../Audio/LiveAudioEngine.h"
#include "../Model/BenchmarkConfig.h"
#include "../Model/BenchmarkResult.h"
#include <functional>

class ConfigTab : public juce::Component
{
public:
    using BenchmarkCompleteCallback = std::function<void(BenchmarkResult)>;

    ConfigTab(PluginLoader& loader, BenchmarkEngine& engine, juce::ApplicationProperties& properties);
    ~ConfigTab() override;
    void resized() override;
    void paint(juce::Graphics& g) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void onBenchmarkComplete(BenchmarkCompleteCallback cb) { benchmarkCompleteCallback = std::move(cb); }
    void triggerBenchmark() { goClicked(); }

private:
    void loadPluginClicked();
    void goClicked();
    BenchmarkConfig gatherConfig() const;

    void loadPluginFromFile(const juce::File& file);
    void saveParameters();
    void restoreParameters();
    void saveCurrentPluginState();
    void restorePluginStateForFile(const juce::File& file);
    void updateMidiControls();

    // Re-prepares the loaded plugin with current UI settings and (re)starts
    // either the live audio engine (if the master toggle is on) or the idle
    // pump fallback. Safe to call when no plugin is loaded (does nothing).
    void applyPlayConfigAndResumeProcessing();
    void stopProcessingAndRelease();

    void onLiveAudioToggled();
    void audioConfigClicked();
    void seedConfigFromDevice(double sampleRate, int blockSize);

    PluginLoader& pluginLoader;
    BenchmarkEngine& benchmarkEngine;
    juce::ApplicationProperties& appProperties;

    IdlePump idlePump;
    LiveAudioEngine liveAudio;
    bool pluginIsPrepared = false;

    // Plugin loading
    juce::TextButton loadButton { "Load Plugin" };
    juce::Label pluginNameLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File currentPluginFile;

    // Parameters
    juce::Label nameLabel            { {}, "Name" };
    juce::TextEditor nameEditor;
    juce::TextButton nameClearButton { juce::String::charToString(0x00D7) };

    juce::Label blockSizeLabel       { {}, "Block Size" };
    juce::Slider blockSizeSlider;

    juce::Label numBlocksLabel       { {}, "Number of Blocks" };
    juce::Slider numBlocksSlider;

    juce::Label sampleRateLabel      { {}, "Sample Rate" };
    juce::ComboBox sampleRateBox;

    juce::Label inputChannelsLabel   { {}, "Input Channels" };
    juce::ComboBox inputChannelsBox;

    juce::Label outputChannelsLabel  { {}, "Output Channels" };
    juce::ComboBox outputChannelsBox;

    juce::Label numMidiNotesLabel    { {}, "MIDI Notes" };
    juce::Slider numMidiNotesSlider;

    juce::Label inputTypeLabel       { {}, "Input Type" };
    juce::ComboBox inputTypeBox;

    // Plugin editor
    juce::TextButton showEditorButton { "Show Plugin GUI" };
    std::unique_ptr<juce::DocumentWindow> editorWindow;
    void toggleEditorClicked();

    // Live audio
    juce::ToggleButton liveAudioToggle { "Audio Active" };
    juce::TextButton audioConfigButton { "Audio Config" };

    // About
    juce::TextButton aboutButton { "About" };
    void aboutClicked();

    // Go
    juce::TextButton goButton { "Go" };
    juce::Label statusLabel;

    BenchmarkCompleteCallback benchmarkCompleteCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConfigTab)
};
