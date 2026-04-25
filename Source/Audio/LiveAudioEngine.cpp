#include "LiveAudioEngine.h"

LiveAudioEngine::LiveAudioEngine() = default;

LiveAudioEngine::~LiveAudioEngine()
{
    detach();
    deviceManager.closeAudioDevice();
}

bool LiveAudioEngine::initialise(const juce::String& savedStateXml)
{
    std::unique_ptr<juce::XmlElement> xml;
    if (savedStateXml.isNotEmpty())
        xml = juce::XmlDocument::parse(savedStateXml);

    auto err = deviceManager.initialise(/*numInputs*/ 256,
                                        /*numOutputs*/ 256,
                                        xml.get(),
                                        /*selectDefault*/ true);

    return err.isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr;
}

juce::String LiveAudioEngine::getStateAsXmlString() const
{
    if (auto state = deviceManager.createStateXml())
        return state->toString();
    return {};
}

bool LiveAudioEngine::isDeviceOpen() const
{
    return deviceManager.getCurrentAudioDevice() != nullptr;
}

void LiveAudioEngine::attach()
{
    if (! callbackAttached)
    {
        deviceManager.addAudioCallback(this);
        deviceManager.addMidiInputDeviceCallback({}, this);
        callbackAttached = true;
    }
}

void LiveAudioEngine::detach()
{
    if (callbackAttached)
    {
        deviceManager.removeAudioCallback(this);
        deviceManager.removeMidiInputDeviceCallback({}, this);
        callbackAttached = false;
    }

    // Make sure no future callback can dereference the plugin pointer.
    const juce::ScopedLock sl(pluginLock);
    plugin = nullptr;
    pluginNumIn = pluginNumOut = 0;
}

void LiveAudioEngine::setPlugin(juce::AudioPluginInstance* p, int numIn, int numOut)
{
    // Atomically swap the plugin pointer; the audio callback uses a ScopedTryLock,
    // so we only block the audio thread for the duration of one block at most.
    const juce::ScopedLock sl(pluginLock);
    plugin       = p;
    pluginNumIn  = numIn;
    pluginNumOut = numOut;

    // Pre-size the working buffer for the largest channel count we'll need.
    if (currentBlockSize > 0)
    {
        const int chBuf = juce::jmax(numIn, numOut, 1);
        pluginBuffer.setSize(chBuf, currentBlockSize, false, true, true);
    }
}

void LiveAudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    currentSampleRate = device->getCurrentSampleRate();
    currentBlockSize  = device->getCurrentBufferSizeSamples();
    currentDeviceIn   = device->getActiveInputChannels().countNumberOfSetBits();
    currentDeviceOut  = device->getActiveOutputChannels().countNumberOfSetBits();

    midiCollector.reset(currentSampleRate);

    {
        const juce::ScopedLock sl(pluginLock);
        const int chBuf = juce::jmax(pluginNumIn, pluginNumOut, 1);
        pluginBuffer.setSize(chBuf, currentBlockSize, false, true, true);
    }

    if (onDeviceConfigChanged != nullptr)
    {
        const auto sr = currentSampleRate;
        const auto bs = currentBlockSize;
        juce::MessageManager::callAsync([cb = onDeviceConfigChanged, sr, bs]
        {
            if (cb) cb(sr, bs);
        });
    }
}

void LiveAudioEngine::audioDeviceStopped()
{
    // Don't release the plugin here — the caller owns its lifecycle. Just stop
    // referencing it from the audio thread.
    const juce::ScopedLock sl(pluginLock);
    plugin = nullptr;
}

void LiveAudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                       int numInputChannels,
                                                       float* const* outputChannelData,
                                                       int numOutputChannels,
                                                       int numSamples,
                                                       const juce::AudioIODeviceCallbackContext&)
{
    // Always start with silent outputs.
    for (int i = 0; i < numOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);

    const juce::ScopedTryLock sl(pluginLock);
    if (! sl.isLocked() || plugin == nullptr)
        return;

    const int pIn  = pluginNumIn;
    const int pOut = pluginNumOut;
    const int chBuf = juce::jmax(pIn, pOut, 1);

    if (pluginBuffer.getNumChannels() < chBuf || pluginBuffer.getNumSamples() < numSamples)
        return; // buffer not yet sized for this device — bail safely

    // hwIn → pluginIn (cyclic replication)
    if (numInputChannels > 0)
    {
        for (int ch = 0; ch < pIn; ++ch)
        {
            const int srcIdx = ch % numInputChannels;
            auto* src = inputChannelData[srcIdx];
            if (src != nullptr)
                pluginBuffer.copyFrom(ch, 0, src, numSamples);
            else
                pluginBuffer.clear(ch, 0, numSamples);
        }
    }
    else
    {
        for (int ch = 0; ch < pIn; ++ch)
            pluginBuffer.clear(ch, 0, numSamples);
    }
    // Output-only channels start clean each block.
    for (int ch = pIn; ch < pOut; ++ch)
        pluginBuffer.clear(ch, 0, numSamples);

    // Drain MIDI for this block.
    juce::MidiBuffer midi;
    midiCollector.removeNextBlockOfMessages(midi, numSamples);

    plugin->processBlock(pluginBuffer, midi);

    // pluginOut → hwOut (cyclic summation, unity gain)
    if (numOutputChannels > 0)
    {
        for (int ch = 0; ch < pOut; ++ch)
        {
            const int dstIdx = ch % numOutputChannels;
            auto* dst = outputChannelData[dstIdx];
            if (dst != nullptr)
                juce::FloatVectorOperations::add(dst, pluginBuffer.getReadPointer(ch), numSamples);
        }
    }
}

void LiveAudioEngine::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& m)
{
    midiCollector.addMessageToQueue(m);
}
