#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginHost/PluginLoader.h"
#include "../PluginHost/BenchmarkEngine.h"
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
    void updateMidiControls();

    PluginLoader& pluginLoader;
    BenchmarkEngine& benchmarkEngine;
    juce::ApplicationProperties& appProperties;

    // Plugin loading
    juce::TextButton loadButton { "Load Plugin" };
    juce::Label pluginNameLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Parameters
    juce::Label blockSizeLabel       { {}, "Block Size" };
    juce::Slider blockSizeSlider;

    juce::Label numBlocksLabel       { {}, "Number of Blocks" };
    juce::Slider numBlocksSlider;

    juce::Label sampleRateLabel      { {}, "Sample Rate" };
    juce::ComboBox sampleRateBox;

    juce::Label channelConfigLabel   { {}, "Channels" };
    juce::ComboBox channelConfigBox;

    juce::Label numMidiNotesLabel    { {}, "MIDI Notes" };
    juce::Slider numMidiNotesSlider;

    juce::Label inputTypeLabel       { {}, "Input Type" };
    juce::ComboBox inputTypeBox;

    // Plugin editor
    juce::TextButton showEditorButton { "Show Plugin GUI" };
    std::unique_ptr<juce::DocumentWindow> editorWindow;
    void toggleEditorClicked();

    // About
    juce::TextButton aboutButton { "About" };
    void aboutClicked();

    // Go
    juce::TextButton goButton { "Go" };
    juce::Label statusLabel;

    BenchmarkCompleteCallback benchmarkCompleteCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConfigTab)
};
