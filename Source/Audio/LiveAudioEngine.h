#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <functional>

/**
    Routes hardware audio I/O (and live MIDI input) through a hosted plugin.

    Channel mismatches between the hardware and the plugin's bus arrangement
    are handled by cyclic mapping:
      - hwIn → pluginIn:   pluginIn[ch]  = hwIn[ch % numHwIn]
      - pluginOut → hwOut: hwOut[ch] += pluginOut[i] for every i with i % numHwOut == ch
        (unity gain, no normalization).

    LiveAudioEngine is a pure router: it does NOT prepare or release the plugin.
    The caller is responsible for calling prepareToPlay before setPlugin() and
    releaseResources() after setPlugin(nullptr, ...).
*/
class LiveAudioEngine : public juce::AudioIODeviceCallback,
                        private juce::MidiInputCallback
{
public:
    LiveAudioEngine();
    ~LiveAudioEngine() override;

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

    // Initialize default device with optional saved XML state. Returns whether
    // a device opened.
    bool initialise(const juce::String& savedStateXml);

    juce::String getStateAsXmlString() const;

    // Attach our IO+MIDI callbacks to the device manager. Idempotent; safe to
    // call once at startup.
    void attach();
    void detach();

    // Set or clear the plugin pointer. Caller must have called prepareToPlay
    // before passing a non-null plugin, and must NOT call releaseResources
    // until after a setPlugin(nullptr, ...) call (which will make the IO
    // callback output silence).
    //
    // Pass numIn == 0 && numOut == 0 alongside nullptr to clear.
    void setPlugin(juce::AudioPluginInstance* plugin, int numIn, int numOut);

    bool isDeviceOpen() const;
    double getDeviceSampleRate() const   noexcept { return currentSampleRate; }
    int    getDeviceBlockSize() const    noexcept { return currentBlockSize;  }
    int    getDeviceNumInputChannels()  const noexcept { return currentDeviceIn;  }
    int    getDeviceNumOutputChannels() const noexcept { return currentDeviceOut; }

    // Fires on the message thread (via callAsync) whenever the live device
    // sample rate / block size changes. ConfigTab uses this to re-seed UI
    // and re-prepare the plugin.
    std::function<void(double sampleRate, int blockSize)> onDeviceConfigChanged;

    // AudioIODeviceCallback
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

private:
    void handleIncomingMidiMessage(juce::MidiInput* source,
                                   const juce::MidiMessage& message) override;

    juce::AudioDeviceManager deviceManager;

    juce::CriticalSection pluginLock;
    juce::AudioPluginInstance* plugin = nullptr;
    int pluginNumIn  = 0;
    int pluginNumOut = 0;

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 0;
    int    currentDeviceIn   = 0;
    int    currentDeviceOut  = 0;

    bool callbackAttached = false;

    juce::MidiMessageCollector midiCollector;
    juce::AudioBuffer<float> pluginBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveAudioEngine)
};
