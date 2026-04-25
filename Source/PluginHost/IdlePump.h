#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

/**
    Continuously feeds the loaded plugin with -18 dBFS white noise (and an empty
    MIDI buffer) at real-time rate, so it stays in a warmed-up steady state
    between benchmark runs. The plugin must already be prepared by the caller
    before start() is called; the pump never calls prepareToPlay/releaseResources
    itself.
*/
class IdlePump : public juce::Thread
{
public:
    IdlePump();
    ~IdlePump() override;

    void start(juce::AudioPluginInstance* plugin,
               double sampleRate,
               int blockSize,
               int numInputChannels,
               int numOutputChannels);

    void stop();

    bool isPumping() const { return isThreadRunning(); }

private:
    void run() override;

    juce::AudioPluginInstance* pluginInstance = nullptr;
    double sampleRate = 44100.0;
    int blockSize = 512;
    int numInputChannels = 2;
    int numOutputChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IdlePump)
};
