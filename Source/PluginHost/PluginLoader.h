#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class PluginLoader
{
public:
    PluginLoader();

    using Callback = std::function<void(const juce::String& /*errorOrEmpty*/)>;

    void loadPlugin(const juce::File& vst3File, Callback onComplete);
    void unloadPlugin();

    bool isPluginLoaded() const { return pluginInstance != nullptr; }
    juce::String getPluginName() const;
    juce::AudioPluginInstance* getPluginInstance() const { return pluginInstance.get(); }
    juce::AudioProcessorEditor* createEditor() const;

    bool acceptsMidi() const;

    void preparePlugin(double sampleRate, int blockSize, int numChannels);
    void releasePlugin();

private:
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::PluginDescription pluginDescription;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginLoader)
};
