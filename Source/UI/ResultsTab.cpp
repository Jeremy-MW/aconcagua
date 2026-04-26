#include "ResultsTab.h"
#include <optional>

namespace
{
    constexpr const char* kCsvDirKey = "lastResultsCsvDir";

    juce::File getStartDir(juce::ApplicationProperties& props)
    {
        if (auto* settings = props.getUserSettings())
        {
            const auto saved = settings->getValue(kCsvDirKey, "");
            if (saved.isNotEmpty())
            {
                juce::File f(saved);
                if (f.isDirectory())
                    return f;
            }
        }
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    }

    void rememberDir(juce::ApplicationProperties& props, const juce::File& file)
    {
        if (file == juce::File{}) return;
        if (auto* settings = props.getUserSettings())
        {
            settings->setValue(kCsvDirKey, file.getParentDirectory().getFullPathName());
            settings->saveIfNeeded();
        }
    }

    juce::int64 parseDateTimeMs(const juce::String& s)
    {
        if (s.length() < 19) return 0;
        const int year   = s.substring(0, 4).getIntValue();
        const int month  = s.substring(5, 7).getIntValue();
        const int day    = s.substring(8, 10).getIntValue();
        const int hour   = s.substring(11, 13).getIntValue();
        const int minute = s.substring(14, 16).getIntValue();
        const int second = s.substring(17, 19).getIntValue();
        if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31)
            return 0;
        return juce::Time(year, month - 1, day, hour, minute, second, 0, true).toMilliseconds();
    }

    InputType parseInputType(const juce::String& s)
    {
        if (s == "MIDI") return InputType::Midi;
        if (s == "Both") return InputType::Both;
        return InputType::Noise;
    }

    // Comparison CSV (Export All) — one row per result, summary fields only.
    // Returns possibly-empty vector. Handles legacy "Channels" header.
    std::vector<BenchmarkResult> parseExportedComparisonCsv(const juce::File& file)
    {
        std::vector<BenchmarkResult> out;
        const auto text = file.loadFileAsString();
        if (text.isEmpty()) return out;

        juce::StringArray lines;
        lines.addLines(text);
        if (lines.isEmpty()) return out;

        juce::StringArray headers;
        headers.addTokens(lines[0], ",", "");
        for (auto& h : headers) h = h.trim();

        auto col = [&](juce::StringRef name)
        {
            for (int i = 0; i < headers.size(); ++i)
                if (headers[i] == name) return i;
            return -1;
        };

        const int cName       = col("Name");
        const int cDateTime   = col("Date/Time");
        const int cPlugin     = col("Plugin");
        const int cBlockSize  = col("Block Size");
        const int cSampleRate = col("Sample Rate");
        const int cInCh       = col("In Ch");
        const int cOutCh      = col("Out Ch");
        const int cChannels   = col("Channels"); // legacy single-channel field
        const int cMidiNotes  = col("MIDI Notes");
        const int cInputType  = col("Input Type");
        const int cTotal      = col("Total (ms)");
        const int cAvg        = col("Avg (us)");
        const int cMin        = col("Min (us)");
        const int cMax        = col("Max (us)");
        const int cStdDev     = col("StdDev (us)");
        const int cSpikes     = col("Spikes");
        const int cAvgBudget  = col("Avg Budget %");
        const int cPeakBudget = col("Peak Budget %");
        const int cOver       = col("Over Budget");

        auto cell = [](const juce::StringArray& row, int idx) -> juce::String
        {
            return (idx >= 0 && idx < row.size()) ? row[idx].trim() : juce::String{};
        };

        for (int i = 1; i < lines.size(); ++i)
        {
            const auto row = lines[i].trim();
            if (row.isEmpty()) continue;

            juce::StringArray cells;
            cells.addTokens(row, ",", "");

            BenchmarkResult r;
            r.config.name = cell(cells, cName);
            r.pluginName  = cell(cells, cPlugin);

            r.completedAtMsSinceEpoch = parseDateTimeMs(cell(cells, cDateTime));
            if (r.completedAtMsSinceEpoch == 0)
                r.completedAtMsSinceEpoch = juce::Time::currentTimeMillis();

            r.config.blockSize  = cell(cells, cBlockSize).getIntValue();
            r.config.sampleRate = cell(cells, cSampleRate).getDoubleValue();

            const int inCh  = cell(cells, cInCh).getIntValue();
            const int outCh = cell(cells, cOutCh).getIntValue();
            if (inCh > 0 || outCh > 0)
            {
                r.config.numInputChannels  = inCh;
                r.config.numOutputChannels = outCh;
            }
            else
            {
                const int legacy = cell(cells, cChannels).getIntValue();
                r.config.numInputChannels  = legacy;
                r.config.numOutputChannels = legacy;
            }
            r.config.numMidiNotes = cell(cells, cMidiNotes).getIntValue();
            r.config.inputType    = parseInputType(cell(cells, cInputType));

            // Summary fields straight from the row — no per-block timings to recompute from.
            r.totalMs          = cell(cells, cTotal).getDoubleValue();
            r.avgUs            = cell(cells, cAvg).getDoubleValue();
            r.minUs            = cell(cells, cMin).getDoubleValue();
            r.maxUs            = cell(cells, cMax).getDoubleValue();
            r.stdDevUs         = cell(cells, cStdDev).getDoubleValue();
            r.spikeCount       = cell(cells, cSpikes).getIntValue();
            r.avgBudgetPercent = cell(cells, cAvgBudget).getDoubleValue();
            r.maxBudgetPercent = cell(cells, cPeakBudget).getDoubleValue();
            r.overBudgetCount  = cell(cells, cOver).getIntValue();

            // Derived fields.
            if (r.config.sampleRate > 0)
                r.budgetUs = (double) r.config.blockSize / r.config.sampleRate * 1.0e6;
            r.spikeThresholdUs = r.avgUs + 2.0 * r.stdDevUs;

            // Approximate block count from total/avg (best-effort; only used for
            // the Latest Stats panel's "/N blocks" line).
            if (r.avgUs > 0.0)
                r.config.numBlocks = (int) std::round(r.totalMs * 1000.0 / r.avgUs);

            out.push_back(std::move(r));
        }

        return out;
    }

    std::optional<BenchmarkResult> parseExportedRunCsv(const juce::File& file)
    {
        const auto text = file.loadFileAsString();
        if (text.isEmpty())
            return std::nullopt;

        juce::StringArray lines;
        lines.addLines(text);

        BenchmarkResult result;
        bool inTimings = false;
        bool foundTimingsHeader = false;
        juce::String dateTimeStr;

        for (auto rawLine : lines)
        {
            auto line = rawLine.trim();
            if (line.isEmpty())
                continue;

            if (! inTimings)
            {
                if (line.startsWith("Block,Time"))
                {
                    inTimings = true;
                    foundTimingsHeader = true;
                    continue;
                }

                const int comma = line.indexOfChar(',');
                if (comma < 0)
                    continue;

                const auto key = line.substring(0, comma).trim();
                const auto val = line.substring(comma + 1).trim();

                if      (key == "Plugin")          result.pluginName = val;
                else if (key == "Name")            result.config.name = val;
                else if (key == "Date/Time")       dateTimeStr = val;
                else if (key == "Block Size")      result.config.blockSize = val.getIntValue();
                else if (key == "Sample Rate")     result.config.sampleRate = val.getDoubleValue();
                else if (key == "Input Channels")  result.config.numInputChannels = val.getIntValue();
                else if (key == "Output Channels") result.config.numOutputChannels = val.getIntValue();
                else if (key == "Channels")
                {
                    // Legacy single-channel field (pre-input/output split).
                    const auto n = val.getIntValue();
                    result.config.numInputChannels  = n;
                    result.config.numOutputChannels = n;
                }
                else if (key == "MIDI Notes")      result.config.numMidiNotes = val.getIntValue();
                else if (key == "Input Type")
                {
                    if      (val == "MIDI") result.config.inputType = InputType::Midi;
                    else if (val == "Both") result.config.inputType = InputType::Both;
                    else                    result.config.inputType = InputType::Noise;
                }
            }
            else
            {
                // Block,Time(us),Spike,OverBudget — only the second field matters.
                const int firstComma = line.indexOfChar(',');
                if (firstComma < 0) continue;
                const int secondComma = line.indexOfChar(firstComma + 1, ',');
                const auto timingStr = secondComma > 0
                                          ? line.substring(firstComma + 1, secondComma)
                                          : line.substring(firstComma + 1);
                result.blockTimingsMicroseconds.push_back(timingStr.getDoubleValue());
            }
        }

        if (! foundTimingsHeader || result.blockTimingsMicroseconds.empty())
            return std::nullopt;

        result.config.numBlocks = (int) result.blockTimingsMicroseconds.size();

        // Parse "YYYY-MM-DD HH:MM:SS" — fall back to current time on failure.
        if (dateTimeStr.length() >= 19)
        {
            const int year   = dateTimeStr.substring(0, 4).getIntValue();
            const int month  = dateTimeStr.substring(5, 7).getIntValue();
            const int day    = dateTimeStr.substring(8, 10).getIntValue();
            const int hour   = dateTimeStr.substring(11, 13).getIntValue();
            const int minute = dateTimeStr.substring(14, 16).getIntValue();
            const int second = dateTimeStr.substring(17, 19).getIntValue();
            if (year > 1970 && month >= 1 && month <= 12 && day >= 1 && day <= 31)
                result.completedAtMsSinceEpoch =
                    juce::Time(year, month - 1, day, hour, minute, second, 0, true).toMilliseconds();
        }
        if (result.completedAtMsSinceEpoch == 0)
            result.completedAtMsSinceEpoch = juce::Time::currentTimeMillis();

        result.computeStats();
        return result;
    }
}

ResultsTab::ResultsTab(juce::ApplicationProperties& properties)
    : appProperties(properties)
{
    latestStatsLabel.setJustificationType(juce::Justification::topLeft);
    latestStatsLabel.setText("No results yet.", juce::dontSendNotification);
    addAndMakeVisible(latestStatsLabel);

    table.getHeader().addColumn("Run",         RunCol,         40);
    table.getHeader().addColumn("Name",        NameCol,       140);
    table.getHeader().addColumn("Date/Time",   DateTimeCol,   140);
    table.getHeader().addColumn("Plugin",      PluginCol,     120);
    table.getHeader().addColumn("Block",       BlockSizeCol,   55);
    table.getHeader().addColumn("Rate",        SampleRateCol,  60);
    table.getHeader().addColumn("In/Out",      ChannelsCol,    60);
    table.getHeader().addColumn("Notes",       NotesCol,       45);
    table.getHeader().addColumn("Total (ms)",  TotalCol,       75);
    table.getHeader().addColumn("Avg (us)",    AvgCol,         70);
    table.getHeader().addColumn("Min (us)",    MinCol,         65);
    table.getHeader().addColumn("Max (us)",    MaxCol,         65);
    table.getHeader().addColumn("Budget (us)", BudgetCol,      80);
    table.getHeader().addColumn("StdDev",      StdDevCol,      65);
    table.getHeader().addColumn("Spikes",      SpikesCol,      50);
    table.getHeader().addColumn("Avg %",       AvgBudgetCol,   55);
    table.getHeader().addColumn("Peak %",      MaxBudgetCol,   55);
    table.getHeader().addColumn("Over",        OverBudgetCol,  45);

    table.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1e1e2e));
    table.addMouseListener(this, true);
    addAndMakeVisible(table);

    retestButton.onClick = [this] { if (retestCallback) retestCallback(); };
    addAndMakeVisible(retestButton);

    clearButton.onClick = [this]
    {
        if (results.empty())
            return;

        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::QuestionIcon,
            "Clear all results?",
            "This will discard all " + juce::String((int) results.size())
                + " benchmark result" + (results.size() == 1 ? "" : "s")
                + ". This cannot be undone.",
            "Clear",
            "Cancel",
            this,
            juce::ModalCallbackFunction::create([this](int result)
            {
                if (result == 1)
                    clearResults();
            }));
    };
    addAndMakeVisible(clearButton);

    importButton.onClick = [this] { importClicked(); };
    addAndMakeVisible(importButton);

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
    clearButton.setBounds(buttonRow.removeFromLeft(80));
    buttonRow.removeFromLeft(10);
    importButton.setBounds(buttonRow.removeFromLeft(80));
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

void ResultsTab::clearResults()
{
    results.clear();
    table.deselectAllRows();
    table.updateContent();
    latestStatsLabel.setText("No results yet.", juce::dontSendNotification);

    if (selectionCallback)
        selectionCallback(nullptr);
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
        case NameCol:       text = r.config.name; break;
        case DateTimeCol:   text = r.getCompletedAtDisplayString(); break;
        case PluginCol:     text = r.pluginName; break;
        case BlockSizeCol:  text = juce::String(r.config.blockSize); break;
        case SampleRateCol: text = juce::String(static_cast<int>(r.config.sampleRate)); break;
        case ChannelsCol:   text = juce::String(r.config.numInputChannels) + "/"
                                 + juce::String(r.config.numOutputChannels); break;
        case NotesCol:      text = juce::String(r.config.numMidiNotes); break;
        case TotalCol:      text = juce::String(r.totalMs, 1); break;
        case AvgCol:        text = juce::String(r.avgUs, 1); break;
        case MinCol:        text = juce::String(r.minUs, 1); break;
        case MaxCol:        text = juce::String(r.maxUs, 1); break;
        case BudgetCol:     text = juce::String(r.budgetUs, 1); break;
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

void ResultsTab::mouseDown(const juce::MouseEvent& event)
{
    if (! (event.mods.isPopupMenu() || event.mods.isRightButtonDown()))
        return;

    if (! table.isParentOf(event.eventComponent) && event.eventComponent != &table)
        return;

    const auto localEvent = event.getEventRelativeTo(&table);
    const auto rowNumber = table.getRowContainingPosition(localEvent.x, localEvent.y);

    if (rowNumber < 0 || rowNumber >= getNumRows())
        return;

    table.selectRow(rowNumber);
    showDeleteMenuForRow(rowNumber, event.eventComponent);
}

void ResultsTab::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&)
{
    if (columnId == NameCol)
        editNameAtRow(rowNumber);
}

void ResultsTab::editNameAtRow(int rowNumber)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(results.size()))
        return;

    auto* aw = new juce::AlertWindow("Rename run", "Enter a new name:",
                                     juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor("name", results[static_cast<size_t>(rowNumber)].config.name);
    aw->addButton("OK",     1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (auto* ed = aw->getTextEditor("name"))
    {
        ed->selectAll();
        juce::MessageManager::callAsync([weak = juce::Component::SafePointer<juce::TextEditor>(ed)]
        {
            if (weak != nullptr) weak->grabKeyboardFocus();
        });
    }

    aw->enterModalState(true,
        juce::ModalCallbackFunction::create([this, rowNumber, aw](int result)
        {
            std::unique_ptr<juce::AlertWindow> owner(aw);
            if (result != 1) return;
            if (rowNumber < 0 || rowNumber >= static_cast<int>(results.size())) return;

            results[static_cast<size_t>(rowNumber)].config.name = aw->getTextEditorContents("name");
            table.repaintRow(rowNumber);
            if (rowNumber == static_cast<int>(results.size()) - 1)
                updateLatestStats();
            saveResults(appProperties);
        }), false);
}

void ResultsTab::selectedRowsChanged(int /*lastRowSelected*/)
{
    if (selectionCallback)
        selectionCallback(getSelectedResult());
}

void ResultsTab::updateLatestStats()
{
    if (results.empty())
    {
        latestStatsLabel.setText("No results yet.", juce::dontSendNotification);
        return;
    }

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

void ResultsTab::deleteResultAtRow(int rowNumber)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(results.size()))
        return;

    results.erase(results.begin() + rowNumber);
    table.updateContent();

    if (results.empty())
    {
        table.deselectAllRows();
        latestStatsLabel.setText("No results yet.", juce::dontSendNotification);

        if (selectionCallback)
            selectionCallback(nullptr);

        return;
    }

    const auto rowToSelect = juce::jlimit(0, static_cast<int>(results.size()) - 1, rowNumber);
    table.selectRow(rowToSelect);
    updateLatestStats();
}

void ResultsTab::showDeleteMenuForRow(int rowNumber, juce::Component* targetComponent)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Delete test entry");

    auto options = juce::PopupMenu::Options()
                       .withTargetComponent(targetComponent != nullptr ? targetComponent : &table)
                       .withParentComponent(this)
                       .withMinimumWidth(160);

    menu.showMenuAsync(options,
                       [this, rowNumber](int result)
                       {
                           if (result == 1)
                               deleteResultAtRow(rowNumber);
                       });
}

void ResultsTab::importClicked()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Import benchmark results",
        getStartDir(appProperties),
        "*.csv");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectMultipleItems;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        const auto files = fc.getResults();
        if (files.isEmpty())
            return;

        rememberDir(appProperties, files[0]);

        int imported = 0;
        juce::StringArray failures;

        for (const auto& file : files)
        {
            // Detect format by first non-empty line: comparison CSVs (Export All)
            // start with "Run,..."; per-run CSVs start with "Plugin,..." or "Name,...".
            juce::StringArray probe;
            probe.addLines(file.loadFileAsString());
            juce::String firstLine;
            for (auto& l : probe) { auto t = l.trim(); if (t.isNotEmpty()) { firstLine = t; break; } }

            int countFromThisFile = 0;

            if (firstLine.startsWith("Run,"))
            {
                auto runs = parseExportedComparisonCsv(file);
                for (auto& r : runs)
                    addResult(std::move(r));
                countFromThisFile = (int) runs.size();
            }
            else
            {
                if (auto parsed = parseExportedRunCsv(file))
                {
                    addResult(std::move(*parsed));
                    countFromThisFile = 1;
                }
            }

            if (countFromThisFile == 0)
                failures.add(file.getFileName());
            else
                imported += countFromThisFile;
        }

        if (! failures.isEmpty())
        {
            juce::String msg;
            msg << "Imported " << imported << " result"
                << (imported == 1 ? "" : "s") << ".\n\n"
                << "Could not parse:\n  " << failures.joinIntoString("\n  ");

            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::WarningIcon)
                    .withTitle("Import")
                    .withMessage(msg)
                    .withButton("OK")
                    .withAssociatedComponent(this),
                nullptr);
        }
    });
}

void ResultsTab::exportRunClicked()
{
    auto* selected = getSelectedResult();
    if (selected == nullptr)
        return;

    fileChooser = std::make_unique<juce::FileChooser>(
        "Export Run to CSV", getStartDir(appProperties), "*.csv");

    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode, [this, selected](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            rememberDir(appProperties, file);
            ResultsExporter::exportRun(*selected, file);
        }
    });
}

void ResultsTab::exportAllClicked()
{
    if (results.empty())
        return;

    fileChooser = std::make_unique<juce::FileChooser>(
        "Export All Runs to CSV", getStartDir(appProperties), "*.csv");

    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            rememberDir(appProperties, file);
            ResultsExporter::exportComparison(results, file);
        }
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
