#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Model/BenchmarkConfig.h"
#include "../Model/BenchmarkResult.h"

class BenchmarkEngine : public juce::Thread
{
public:
    using CompletionCallback = std::function<void(BenchmarkResult)>;

    BenchmarkEngine();
    ~BenchmarkEngine() override;

    void startBenchmark(juce::AudioPluginInstance* plugin,
                        const juce::String& pluginName,
                        const BenchmarkConfig& config,
                        CompletionCallback onComplete);

    bool isRunning() const { return isThreadRunning(); }

private:
    void run() override;

    juce::AudioPluginInstance* pluginInstance = nullptr;
    juce::String pluginName;
    BenchmarkConfig config;
    CompletionCallback completionCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BenchmarkEngine)
};
