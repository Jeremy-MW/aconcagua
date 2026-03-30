#include "BenchmarkEngine.h"
#include <chrono>

BenchmarkEngine::BenchmarkEngine()
    : Thread("BenchmarkThread")
{
}

BenchmarkEngine::~BenchmarkEngine()
{
    stopThread(5000);
}

void BenchmarkEngine::startBenchmark(juce::AudioPluginInstance* plugin,
                                     const juce::String& name,
                                     const BenchmarkConfig& cfg,
                                     CompletionCallback onComplete)
{
    if (isThreadRunning())
        return;

    pluginInstance = plugin;
    pluginName = name;
    config = cfg;
    completionCallback = std::move(onComplete);

    startThread(juce::Thread::Priority::highest);
}

void BenchmarkEngine::run()
{
    if (pluginInstance == nullptr)
        return;

    // Prepare the plugin
    pluginInstance->setPlayConfigDetails(config.numChannels, config.numChannels,
                                        config.sampleRate, config.blockSize);
    pluginInstance->prepareToPlay(config.sampleRate, config.blockSize);

    // Allocate audio buffer
    juce::AudioBuffer<float> audioBuffer(config.numChannels, config.blockSize);

    // Prepare MIDI buffer
    juce::MidiBuffer midiBuffer;
    if (config.inputType == InputType::Midi || config.inputType == InputType::Both)
    {
        for (int i = 0; i < config.numMidiNotes; ++i)
        {
            int noteNumber = 36 + (i % 88);          // spread across keyboard
            int channel = 1 + (i % 16);               // spread across channels
            midiBuffer.addEvent(juce::MidiMessage::noteOn(channel, noteNumber, 0.8f), 0);
        }
    }

    // Prepare noise generator
    juce::Random random;
    bool useNoise = (config.inputType == InputType::Noise || config.inputType == InputType::Both);

    // Pre-allocate results
    BenchmarkResult result;
    result.pluginName = pluginName;
    result.config = config;
    result.blockTimingsMicroseconds.reserve(static_cast<size_t>(config.numBlocks));

    // Benchmark loop
    for (int block = 0; block < config.numBlocks; ++block)
    {
        if (threadShouldExit())
            break;

        // Fill buffer with noise if needed
        if (useNoise)
        {
            for (int ch = 0; ch < config.numChannels; ++ch)
            {
                auto* data = audioBuffer.getWritePointer(ch);
                for (int s = 0; s < config.blockSize; ++s)
                    data[s] = random.nextFloat() * 2.0f - 1.0f;
            }
        }
        else
        {
            audioBuffer.clear();
        }

        // Time the processBlock call
        auto start = std::chrono::high_resolution_clock::now();
        pluginInstance->processBlock(audioBuffer, midiBuffer);
        auto end = std::chrono::high_resolution_clock::now();

        auto durationUs = std::chrono::duration<double, std::micro>(end - start).count();
        result.blockTimingsMicroseconds.push_back(durationUs);
    }

    pluginInstance->releaseResources();

    // Compute stats
    result.computeStats();

    // Post result back to message thread
    auto callback = std::move(completionCallback);
    auto resultCopy = std::move(result);

    juce::MessageManager::callAsync([callback = std::move(callback),
                                     resultCopy = std::move(resultCopy)]()
    {
        if (callback)
            callback(std::move(resultCopy));
    });
}
