#include "PluginLoader.h"

PluginLoader::PluginLoader()
{
    formatManager.addFormat(new juce::VST3PluginFormat());
}

void PluginLoader::loadPlugin(const juce::File& vst3File, Callback onComplete)
{
    unloadPlugin();

    juce::OwnedArray<juce::PluginDescription> descriptions;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile(descriptions, vst3File.getFullPathName());

    if (descriptions.isEmpty())
    {
        onComplete("Failed to find any plugins in: " + vst3File.getFileName());
        return;
    }

    pluginDescription = *descriptions[0];
    juce::String errorMessage;

    pluginInstance = formatManager.createPluginInstance(
        pluginDescription, 44100.0, 512, errorMessage);

    if (pluginInstance == nullptr)
    {
        onComplete("Failed to load plugin: " + errorMessage);
        return;
    }

    onComplete({});
}

void PluginLoader::unloadPlugin()
{
    releasePlugin();
    pluginInstance.reset();
    pluginDescription = {};
}

juce::String PluginLoader::getPluginName() const
{
    if (pluginInstance != nullptr)
        return pluginInstance->getName();

    return "No plugin loaded";
}

juce::AudioProcessorEditor* PluginLoader::createEditor() const
{
    if (pluginInstance != nullptr && pluginInstance->hasEditor())
        return pluginInstance->createEditor();

    return nullptr;
}

bool PluginLoader::acceptsMidi() const
{
    if (pluginInstance != nullptr)
        return pluginInstance->acceptsMidi();
    return false;
}

void PluginLoader::preparePlugin(double sampleRate, int blockSize,
                                 int numInputChannels, int numOutputChannels)
{
    if (pluginInstance == nullptr)
        return;

    pluginInstance->setPlayConfigDetails(numInputChannels, numOutputChannels,
                                         sampleRate, blockSize);
    pluginInstance->prepareToPlay(sampleRate, blockSize);
}

void PluginLoader::releasePlugin()
{
    if (pluginInstance != nullptr)
        pluginInstance->releaseResources();
}
