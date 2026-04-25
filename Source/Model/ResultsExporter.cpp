#include "ResultsExporter.h"

bool ResultsExporter::exportRun(const BenchmarkResult& result, const juce::File& file)
{
    juce::String csv;

    // Summary header
    csv << "Plugin," << result.pluginName << "\n";
    csv << "Date/Time," << result.getCompletedAtDisplayString() << "\n";
    csv << "Block Size," << result.config.blockSize << "\n";
    csv << "Sample Rate," << static_cast<int>(result.config.sampleRate) << "\n";
    csv << "Input Channels," << result.config.numInputChannels << "\n";
    csv << "Output Channels," << result.config.numOutputChannels << "\n";
    csv << "MIDI Notes," << result.config.numMidiNotes << "\n";
    csv << "Input Type," << inputTypeToString(result.config.inputType) << "\n";
    csv << "Total (ms)," << juce::String(result.totalMs, 4) << "\n";
    csv << "Avg (us)," << juce::String(result.avgUs, 4) << "\n";
    csv << "Min (us)," << juce::String(result.minUs, 4) << "\n";
    csv << "Max (us)," << juce::String(result.maxUs, 4) << "\n";
    csv << "StdDev (us)," << juce::String(result.stdDevUs, 4) << "\n";
    csv << "Spike Threshold (us)," << juce::String(result.spikeThresholdUs, 4) << "\n";
    csv << "Spike Count," << result.spikeCount << "\n";
    csv << "Budget (us)," << juce::String(result.budgetUs, 4) << "\n";
    csv << "Avg Budget %," << juce::String(result.avgBudgetPercent, 2) << "\n";
    csv << "Peak Budget %," << juce::String(result.maxBudgetPercent, 2) << "\n";
    csv << "Over-Budget Blocks," << result.overBudgetCount << "\n";
    csv << "\n";

    // Per-block timings
    csv << "Block,Time (us),Spike,Over Budget\n";
    for (size_t i = 0; i < result.blockTimingsMicroseconds.size(); ++i)
    {
        auto timing = result.blockTimingsMicroseconds[i];
        bool isSpike = timing > result.spikeThresholdUs;
        bool isOverBudget = timing > result.budgetUs;
        csv << static_cast<int>(i + 1) << ","
            << juce::String(timing, 4) << ","
            << (isSpike ? "YES" : "") << ","
            << (isOverBudget ? "YES" : "") << "\n";
    }

    return file.replaceWithText(csv);
}

bool ResultsExporter::exportComparison(const std::vector<BenchmarkResult>& results, const juce::File& file)
{
    juce::String csv;
    csv << "Run,Date/Time,Plugin,Block Size,Sample Rate,In Ch,Out Ch,MIDI Notes,Input Type,"
        << "Total (ms),Avg (us),Min (us),Max (us),StdDev (us),Spikes,Avg Budget %,Peak Budget %,Over Budget\n";

    for (size_t i = 0; i < results.size(); ++i)
    {
        const auto& r = results[i];
        csv << static_cast<int>(i + 1) << ","
            << r.getCompletedAtDisplayString() << ","
            << r.pluginName << ","
            << r.config.blockSize << ","
            << static_cast<int>(r.config.sampleRate) << ","
            << r.config.numInputChannels << ","
            << r.config.numOutputChannels << ","
            << r.config.numMidiNotes << ","
            << inputTypeToString(r.config.inputType) << ","
            << juce::String(r.totalMs, 4) << ","
            << juce::String(r.avgUs, 4) << ","
            << juce::String(r.minUs, 4) << ","
            << juce::String(r.maxUs, 4) << ","
            << juce::String(r.stdDevUs, 4) << ","
            << r.spikeCount << ","
            << juce::String(r.avgBudgetPercent, 2) << ","
            << juce::String(r.maxBudgetPercent, 2) << ","
            << r.overBudgetCount << "\n";
    }

    return file.replaceWithText(csv);
}

juce::String ResultsExporter::inputTypeToString(InputType type)
{
    switch (type)
    {
        case InputType::Noise: return "White Noise";
        case InputType::Midi:  return "MIDI";
        case InputType::Both:  return "Both";
        default:               return "Unknown";
    }
}
