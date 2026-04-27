#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/BenchmarkResult.h"
#include <optional>

class HistogramTab : public juce::Component
{
public:
    HistogramTab();
    void resized() override;
    void paint(juce::Graphics& g) override;

    void setResult(const BenchmarkResult* result);

private:
    std::optional<BenchmarkResult> currentResult;

    static constexpr int numBins = 50;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HistogramTab)
};
