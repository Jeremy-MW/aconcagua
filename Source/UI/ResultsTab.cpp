#include "ResultsTab.h"

ResultsTab::ResultsTab()
{
    latestStatsLabel.setJustificationType(juce::Justification::topLeft);
    latestStatsLabel.setText("No results yet.", juce::dontSendNotification);
    addAndMakeVisible(latestStatsLabel);

    table.getHeader().addColumn("Run",        RunCol,         40);
    table.getHeader().addColumn("Plugin",      PluginCol,     120);
    table.getHeader().addColumn("Block",       BlockSizeCol,   55);
    table.getHeader().addColumn("Rate",        SampleRateCol,  60);
    table.getHeader().addColumn("Ch",          ChannelsCol,    35);
    table.getHeader().addColumn("Notes",       NotesCol,       45);
    table.getHeader().addColumn("Total (ms)",  TotalCol,       75);
    table.getHeader().addColumn("Avg (us)",    AvgCol,         70);
    table.getHeader().addColumn("Min (us)",    MinCol,         65);
    table.getHeader().addColumn("Max (us)",    MaxCol,         65);
    table.getHeader().addColumn("StdDev",      StdDevCol,      65);
    table.getHeader().addColumn("Spikes",      SpikesCol,      50);
    table.getHeader().addColumn("Avg %",       AvgBudgetCol,   55);
    table.getHeader().addColumn("Peak %",      MaxBudgetCol,   55);
    table.getHeader().addColumn("Over",        OverBudgetCol,  45);

    table.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1e1e2e));
    addAndMakeVisible(table);

    retestButton.onClick = [this] { if (retestCallback) retestCallback(); };
    addAndMakeVisible(retestButton);

    exportRunButton.onClick = [this] { exportRunClicked(); };
    addAndMakeVisible(exportRunButton);

    exportAllButton.onClick = [this] { exportAllClicked(); };
    addAndMakeVisible(exportAllButton);
}

void ResultsTab::resized()
{
    auto area = getLocalBounds().reduced(10);
    latestStatsLabel.setBounds(area.removeFromTop(80));
    area.removeFromTop(5);

    auto buttonRow = area.removeFromBottom(35);
    buttonRow.removeFromTop(5);
    retestButton.setBounds(buttonRow.removeFromLeft(80));
    buttonRow.removeFromLeft(10);
    exportRunButton.setBounds(buttonRow.removeFromLeft(100));
    buttonRow.removeFromLeft(10);
    exportAllButton.setBounds(buttonRow.removeFromLeft(100));

    table.setBounds(area);
}

void ResultsTab::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void ResultsTab::addResult(BenchmarkResult result)
{
    results.push_back(std::move(result));
    table.updateContent();
    table.selectRow(static_cast<int>(results.size()) - 1);
    updateLatestStats();
}

const BenchmarkResult* ResultsTab::getSelectedResult() const
{
    auto row = table.getSelectedRow();
    if (row >= 0 && row < static_cast<int>(results.size()))
        return &results[static_cast<size_t>(row)];
    return nullptr;
}

int ResultsTab::getNumRows()
{
    return static_cast<int>(results.size());
}

void ResultsTab::paintRowBackground(juce::Graphics& g, int /*rowNumber*/, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colour(0xff45475a));
    else
        g.fillAll(juce::Colour(0xff1e1e2e));

    g.setColour(juce::Colour(0xff313244));
    g.drawLine(0.0f, static_cast<float>(height) - 0.5f,
               static_cast<float>(width), static_cast<float>(height) - 0.5f);
}

void ResultsTab::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                           int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(results.size()))
        return;

    const auto& r = results[static_cast<size_t>(rowNumber)];
    g.setColour(juce::Colour(0xffcdd6f4));
    g.setFont(13.0f);

    juce::String text;
    switch (columnId)
    {
        case RunCol:        text = juce::String(rowNumber + 1); break;
        case PluginCol:     text = r.pluginName; break;
        case BlockSizeCol:  text = juce::String(r.config.blockSize); break;
        case SampleRateCol: text = juce::String(static_cast<int>(r.config.sampleRate)); break;
        case ChannelsCol:   text = juce::String(r.config.numChannels); break;
        case NotesCol:      text = juce::String(r.config.numMidiNotes); break;
        case TotalCol:      text = juce::String(r.totalMs, 1); break;
        case AvgCol:        text = juce::String(r.avgUs, 1); break;
        case MinCol:        text = juce::String(r.minUs, 1); break;
        case MaxCol:        text = juce::String(r.maxUs, 1); break;
        case StdDevCol:     text = juce::String(r.stdDevUs, 1); break;
        case SpikesCol:     text = juce::String(r.spikeCount); break;
        case AvgBudgetCol:  text = juce::String(r.avgBudgetPercent, 1) + "%"; break;
        case MaxBudgetCol:  text = juce::String(r.maxBudgetPercent, 1) + "%"; break;
        case OverBudgetCol: text = juce::String(r.overBudgetCount); break;
        default: break;
    }

    // Color budget columns red when over budget
    if (columnId == AvgBudgetCol && r.avgBudgetPercent > 100.0)
        g.setColour(juce::Colour(0xfff38ba8));
    else if (columnId == MaxBudgetCol && r.maxBudgetPercent > 100.0)
        g.setColour(juce::Colour(0xfff38ba8));
    else if (columnId == OverBudgetCol && r.overBudgetCount > 0)
        g.setColour(juce::Colour(0xfff38ba8));

    g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft);
}

void ResultsTab::selectedRowsChanged(int /*lastRowSelected*/)
{
    if (selectionCallback)
        selectionCallback(getSelectedResult());
}

void ResultsTab::updateLatestStats()
{
    if (results.empty())
        return;

    const auto& r = results.back();
    juce::String stats;
    stats << "Latest: " << r.pluginName
          << "  |  Total: " << juce::String(r.totalMs, 2) << " ms"
          << "  |  Avg: " << juce::String(r.avgUs, 2) << " us"
          << "  |  Min: " << juce::String(r.minUs, 2) << " us"
          << "  |  Max: " << juce::String(r.maxUs, 2) << " us"
          << "\nStdDev: " << juce::String(r.stdDevUs, 2) << " us"
          << "  |  Spikes (>" << juce::String(r.spikeThresholdUs, 1) << " us): " << r.spikeCount
          << " / " << static_cast<int>(r.blockTimingsMicroseconds.size()) << " blocks"
          << "\nBudget: " << juce::String(r.avgBudgetPercent, 1) << "% avg"
          << "  |  " << juce::String(r.maxBudgetPercent, 1) << "% peak"
          << "  |  Over-budget blocks: " << r.overBudgetCount
          << "  (budget: " << juce::String(r.budgetUs, 1) << " us per block)";

    latestStatsLabel.setText(stats, juce::dontSendNotification);
}

void ResultsTab::exportRunClicked()
{
    auto* selected = getSelectedResult();
    if (selected == nullptr)
        return;

    fileChooser = std::make_unique<juce::FileChooser>(
        "Export Run to CSV", juce::File{}, "*.csv");

    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode, [this, selected](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
            ResultsExporter::exportRun(*selected, file);
    });
}

void ResultsTab::exportAllClicked()
{
    if (results.empty())
        return;

    fileChooser = std::make_unique<juce::FileChooser>(
        "Export All Runs to CSV", juce::File{}, "*.csv");

    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
            ResultsExporter::exportComparison(results, file);
    });
}

void ResultsTab::saveResults(juce::ApplicationProperties& props)
{
    if (auto* settings = props.getUserSettings())
    {
        auto xml = std::make_unique<juce::XmlElement>("BenchmarkResults");
        for (const auto& r : results)
            xml->addChildElement(r.toXml());

        settings->setValue("savedResults", xml.get());
        settings->saveIfNeeded();
    }
}

void ResultsTab::restoreResults(juce::ApplicationProperties& props)
{
    if (auto* settings = props.getUserSettings())
    {
        auto xml = settings->getXmlValue("savedResults");
        if (xml != nullptr)
        {
            for (auto* child : xml->getChildIterator())
            {
                auto result = BenchmarkResult::fromXml(*child);
                results.push_back(std::move(result));
            }

            table.updateContent();
            if (!results.empty())
            {
                table.selectRow(static_cast<int>(results.size()) - 1);
                updateLatestStats();
            }
        }
    }
}
