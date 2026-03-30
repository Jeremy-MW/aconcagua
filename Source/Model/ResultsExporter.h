#pragma once
#include <juce_core/juce_core.h>
#include "BenchmarkResult.h"
#include <vector>

class ResultsExporter
{
public:
    static bool exportRun(const BenchmarkResult& result, const juce::File& file);
    static bool exportComparison(const std::vector<BenchmarkResult>& results, const juce::File& file);

private:
    static juce::String inputTypeToString(InputType type);
};
