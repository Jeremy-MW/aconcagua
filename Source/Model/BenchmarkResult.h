#pragma once
#include <juce_core/juce_core.h>
#include "BenchmarkConfig.h"
#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>

struct BenchmarkResult
{
    juce::String pluginName;
    juce::int64 completedAtMsSinceEpoch = 0;
    BenchmarkConfig config;
    std::vector<double> blockTimingsMicroseconds;

    double totalMs = 0.0;
    double avgUs = 0.0;
    double minUs = 0.0;
    double maxUs = 0.0;
    double stdDevUs = 0.0;
    double budgetUs = 0.0;       // Real-time budget per block in microseconds
    double avgBudgetPercent = 0.0;  // Average % of real-time budget used
    double maxBudgetPercent = 0.0;  // Peak % of real-time budget used
    int spikeCount = 0;
    double spikeThresholdUs = 0.0;
    std::vector<int> spikeIndices;
    int overBudgetCount = 0;
    std::vector<int> overBudgetIndices;

    juce::XmlElement* toXml() const
    {
        auto* xml = new juce::XmlElement("Result");
        xml->setAttribute("pluginName", pluginName);
        xml->setAttribute("completedAtMsSinceEpoch", juce::String(completedAtMsSinceEpoch));
        xml->setAttribute("name", config.name);
        xml->setAttribute("blockSize", config.blockSize);
        xml->setAttribute("numBlocks", config.numBlocks);
        xml->setAttribute("sampleRate", config.sampleRate);
        xml->setAttribute("numInputChannels", config.numInputChannels);
        xml->setAttribute("numOutputChannels", config.numOutputChannels);
        xml->setAttribute("numMidiNotes", config.numMidiNotes);
        xml->setAttribute("inputType", static_cast<int>(config.inputType));

        // Store timings as a comma-separated string
        juce::String timingsStr;
        for (size_t i = 0; i < blockTimingsMicroseconds.size(); ++i)
        {
            if (i > 0) timingsStr << ",";
            timingsStr << juce::String(blockTimingsMicroseconds[i], 6);
        }
        xml->setAttribute("timings", timingsStr);

        return xml;
    }

    static BenchmarkResult fromXml(const juce::XmlElement& xml)
    {
        BenchmarkResult r;
        r.pluginName = xml.getStringAttribute("pluginName");
        r.completedAtMsSinceEpoch = xml.getStringAttribute("completedAtMsSinceEpoch").getLargeIntValue();
        r.config.name = xml.getStringAttribute("name");
        r.config.blockSize = xml.getIntAttribute("blockSize", 512);
        r.config.numBlocks = xml.getIntAttribute("numBlocks", 10000);
        r.config.sampleRate = xml.getDoubleAttribute("sampleRate", 44100.0);
        const int legacyNumChannels = xml.getIntAttribute("numChannels", 2);
        r.config.numInputChannels  = xml.getIntAttribute("numInputChannels",  legacyNumChannels);
        r.config.numOutputChannels = xml.getIntAttribute("numOutputChannels", legacyNumChannels);
        r.config.numMidiNotes = xml.getIntAttribute("numMidiNotes", 0);
        r.config.inputType = static_cast<InputType>(xml.getIntAttribute("inputType", 0));

        auto timingsStr = xml.getStringAttribute("timings");
        juce::StringArray tokens;
        tokens.addTokens(timingsStr, ",", "");
        r.blockTimingsMicroseconds.reserve(static_cast<size_t>(tokens.size()));
        for (auto& t : tokens)
            if (t.isNotEmpty())
                r.blockTimingsMicroseconds.push_back(t.getDoubleValue());

        r.computeStats();
        return r;
    }

    juce::String getCompletedAtDisplayString() const
    {
        if (completedAtMsSinceEpoch <= 0)
            return {};

        return juce::Time(completedAtMsSinceEpoch).formatted("%Y-%m-%d %H:%M:%S");
    }

    void computeStats()
    {
        if (blockTimingsMicroseconds.empty())
            return;

        auto& t = blockTimingsMicroseconds;
        const auto n = static_cast<double>(t.size());

        totalMs = std::accumulate(t.begin(), t.end(), 0.0) / 1000.0;
        avgUs = std::accumulate(t.begin(), t.end(), 0.0) / n;
        minUs = *std::min_element(t.begin(), t.end());
        maxUs = *std::max_element(t.begin(), t.end());

        double variance = 0.0;
        for (auto val : t)
        {
            double diff = val - avgUs;
            variance += diff * diff;
        }
        stdDevUs = std::sqrt(variance / n);

        budgetUs = (static_cast<double>(config.blockSize) / config.sampleRate) * 1000000.0;
        avgBudgetPercent = (avgUs / budgetUs) * 100.0;
        maxBudgetPercent = (maxUs / budgetUs) * 100.0;

        spikeThresholdUs = avgUs + 2.0 * stdDevUs;
        spikeCount = 0;
        spikeIndices.clear();
        overBudgetCount = 0;
        overBudgetIndices.clear();
        for (int i = 0; i < static_cast<int>(t.size()); ++i)
        {
            auto timing = t[static_cast<size_t>(i)];
            if (timing > spikeThresholdUs)
            {
                ++spikeCount;
                spikeIndices.push_back(i);
            }
            if (timing > budgetUs)
            {
                ++overBudgetCount;
                overBudgetIndices.push_back(i);
            }
        }
    }
};
