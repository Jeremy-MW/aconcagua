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

    // Plugin is assumed to already be prepared with the current config by the
    // caller (ConfigTab manages prepareToPlay/releaseResources lifecycle so the
    // IdlePump can run continuously between benchmarks).

    const int maxCh = juce::jmax(config.numInputChannels, config.numOutputChannels, 1);
    juce::AudioBuffer<float> audioBuffer(maxCh, config.blockSize);

    juce::MidiBuffer midiBuffer;
    if (config.inputType == InputType::Midi || config.inputType == InputType::Both)
    {
        for (int i = 0; i < config.numMidiNotes; ++i)
        {
            int noteNumber = 36 + (i % 88);
            int channel = 1 + (i % 16);
            midiBuffer.addEvent(juce::MidiMessage::noteOn(channel, noteNumber, 0.8f), 0);
        }
    }

    juce::Random random;
    bool useNoise = (config.inputType == InputType::Noise || config.inputType == InputType::Both);

    BenchmarkResult result;
    result.pluginName = pluginName;
    result.completedAtMsSinceEpoch = juce::Time::currentTimeMillis();
    result.config = config;
    result.blockTimingsMicroseconds.reserve(static_cast<size_t>(config.numBlocks));

    for (int block = 0; block < config.numBlocks; ++block)
    {
        if (threadShouldExit())
            break;

        if (useNoise)
        {
            for (int ch = 0; ch < config.numInputChannels && ch < maxCh; ++ch)
            {
                auto* data = audioBuffer.getWritePointer(ch);
                for (int s = 0; s < config.blockSize; ++s)
                    data[s] = random.nextFloat() * 2.0f - 1.0f;
            }
            for (int ch = config.numInputChannels; ch < maxCh; ++ch)
                audioBuffer.clear(ch, 0, config.blockSize);
        }
        else
        {
            audioBuffer.clear();
        }

        auto start = std::chrono::high_resolution_clock::now();
        pluginInstance->processBlock(audioBuffer, midiBuffer);
        auto end = std::chrono::high_resolution_clock::now();

        auto durationUs = std::chrono::duration<double, std::micro>(end - start).count();
        result.blockTimingsMicroseconds.push_back(durationUs);
    }

    result.computeStats();

    auto callback = std::move(completionCallback);
    auto resultCopy = std::move(result);

    juce::MessageManager::callAsync([callback = std::move(callback),
                                     resultCopy = std::move(resultCopy)]()
    {
        if (callback)
            callback(std::move(resultCopy));
    });
}
