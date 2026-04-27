#include "HistogramTab.h"
#include <algorithm>

HistogramTab::HistogramTab()
{
}

void HistogramTab::setResult(const BenchmarkResult* result)
{
    if (result != nullptr)
        currentResult = *result;
    else
        currentResult.reset();

    repaint();
}

void HistogramTab::resized()
{
}

void HistogramTab::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e2e));

    if (! currentResult.has_value() || currentResult->blockTimingsMicroseconds.empty())
    {
        g.setColour(juce::Colour(0xffa6adc8));
        g.setFont(16.0f);
        g.drawText("Select a run from the Results tab to view its histogram.",
                   getLocalBounds(), juce::Justification::centred);
        return;
    }

    const auto& timings = currentResult->blockTimingsMicroseconds;
    const double minVal = currentResult->minUs;
    const double maxVal = currentResult->maxUs;
    const double spikeThreshold = currentResult->spikeThresholdUs;

    if (maxVal <= minVal)
        return;

    // Compute histogram bins
    const double binWidth = (maxVal - minVal) / numBins;
    std::vector<int> bins(numBins, 0);
    int maxBinCount = 0;

    for (auto val : timings)
    {
        int bin = static_cast<int>((val - minVal) / binWidth);
        if (bin >= numBins) bin = numBins - 1;
        if (bin < 0) bin = 0;
        bins[static_cast<size_t>(bin)]++;
        maxBinCount = std::max(maxBinCount, bins[static_cast<size_t>(bin)]);
    }

    if (maxBinCount == 0)
        return;

    // Drawing area
    const int margin = 60;
    const int bottomMargin = 50;
    const int topMargin = 30;
    auto chartArea = getLocalBounds().reduced(margin, topMargin);
    chartArea.removeFromBottom(bottomMargin - topMargin);

    const float chartX = static_cast<float>(chartArea.getX());
    const float chartY = static_cast<float>(chartArea.getY());
    const float chartW = static_cast<float>(chartArea.getWidth());
    const float chartH = static_cast<float>(chartArea.getHeight());
    const float barWidth = chartW / static_cast<float>(numBins);

    // Title
    g.setColour(juce::Colour(0xffcdd6f4));
    g.setFont(14.0f);
    g.drawText(currentResult->pluginName + " - Block Timing Distribution",
               getLocalBounds().removeFromTop(topMargin + 5),
               juce::Justification::centred);

    // Draw bars
    for (int i = 0; i < numBins; ++i)
    {
        float barHeight = (static_cast<float>(bins[static_cast<size_t>(i)]) /
                           static_cast<float>(maxBinCount)) * chartH;
        float x = chartX + static_cast<float>(i) * barWidth;
        float y = chartY + chartH - barHeight;

        double binCenter = minVal + (static_cast<double>(i) + 0.5) * binWidth;

        if (binCenter > spikeThreshold)
            g.setColour(juce::Colour(0xfff38ba8)); // Red for spikes
        else
            g.setColour(juce::Colour(0xff89b4fa)); // Blue for normal

        g.fillRect(x + 1.0f, y, barWidth - 2.0f, barHeight);
    }

    // Draw spike threshold line
    if (spikeThreshold >= minVal && spikeThreshold <= maxVal)
    {
        float threshX = chartX + static_cast<float>((spikeThreshold - minVal) / (maxVal - minVal)) * chartW;
        g.setColour(juce::Colour(0xfff38ba8));
        g.drawVerticalLine(static_cast<int>(threshX), chartY, chartY + chartH);
        g.setFont(11.0f);
        g.drawText("spike: " + juce::String(spikeThreshold, 1) + " us",
                   static_cast<int>(threshX) - 40, static_cast<int>(chartY) - 15, 80, 14,
                   juce::Justification::centred);
    }

    // Draw budget line
    double budgetUs = currentResult->budgetUs;
    if (budgetUs >= minVal && budgetUs <= maxVal)
    {
        float budgetX = chartX + static_cast<float>((budgetUs - minVal) / (maxVal - minVal)) * chartW;
        g.setColour(juce::Colour(0xffa6e3a1)); // Green for budget line
        float dashLengths[] = { 4.0f, 4.0f };
        g.drawDashedLine(juce::Line<float>(budgetX, chartY, budgetX, chartY + chartH),
                         dashLengths, 2);
        g.setFont(11.0f);
        g.drawText("budget: " + juce::String(budgetUs, 1) + " us",
                   static_cast<int>(budgetX) - 45, static_cast<int>(chartY + chartH + 2), 90, 14,
                   juce::Justification::centred);
    }

    // Axes labels
    g.setColour(juce::Colour(0xffa6adc8));
    g.setFont(11.0f);

    // X-axis
    g.drawText(juce::String(minVal, 1) + " us",
               static_cast<int>(chartX), static_cast<int>(chartY + chartH + 5), 60, 15,
               juce::Justification::centredLeft);
    g.drawText(juce::String(maxVal, 1) + " us",
               static_cast<int>(chartX + chartW) - 60, static_cast<int>(chartY + chartH + 5), 60, 15,
               juce::Justification::centredRight);
    g.drawText("Time per block (us)",
               chartArea.getX(), static_cast<int>(chartY + chartH + 20), chartArea.getWidth(), 15,
               juce::Justification::centred);

    // Y-axis
    g.drawText(juce::String(maxBinCount), static_cast<int>(chartX) - 45,
               static_cast<int>(chartY) - 7, 40, 14, juce::Justification::centredRight);
    g.drawText("0", static_cast<int>(chartX) - 45,
               static_cast<int>(chartY + chartH) - 7, 40, 14, juce::Justification::centredRight);

    // Axes lines
    g.setColour(juce::Colour(0xff585b70));
    g.drawLine(chartX, chartY + chartH, chartX + chartW, chartY + chartH);
    g.drawLine(chartX, chartY, chartX, chartY + chartH);
}
