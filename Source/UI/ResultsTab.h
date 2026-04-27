#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Model/BenchmarkResult.h"
#include "../Model/ResultsExporter.h"
#include <vector>
#include <functional>

class ResultsTab : public juce::Component,
                   public juce::TableListBoxModel
{
public:
    ResultsTab(juce::ApplicationProperties& properties);
    void resized() override;
    void paint(juce::Graphics& g) override;

    void addResult(BenchmarkResult result);
    void clearResults();
    const BenchmarkResult* getSelectedResult() const;

    using SelectionCallback = std::function<void(const BenchmarkResult*)>;
    void onSelectionChanged(SelectionCallback cb) { selectionCallback = std::move(cb); }

    using RetestCallback = std::function<void()>;
    void onRetest(RetestCallback cb) { retestCallback = std::move(cb); }
    void setRetestEnabled(bool enabled) { retestButton.setEnabled(enabled); }

    const std::vector<BenchmarkResult>& getResults() const { return results; }

    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void sortOrderChanged(int newSortColumnId, bool isForwards) override;

private:
    void mouseDown(const juce::MouseEvent& event) override;

    enum ColumnIds
    {
        RunCol = 1,
        NameCol,
        DateTimeCol,
        PluginCol,
        BlockSizeCol,
        SampleRateCol,
        ChannelsCol,
        NotesCol,
        TotalCol,
        AvgCol,
        MinCol,
        MaxCol,
        BudgetCol,
        StdDevCol,
        SpikesCol,
        AvgBudgetCol,
        MaxBudgetCol,
        OverBudgetCol
    };

    juce::ApplicationProperties& appProperties;
    std::vector<BenchmarkResult> results;
    std::vector<size_t> displayOrder;

    // Latest run stats panel
    juce::Label latestStatsLabel;

    // Comparison table
    juce::TableListBox table { "Results", this };

    SelectionCallback selectionCallback;

    juce::TextButton retestButton { "Re-test" };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton importButton { "Import" };
    juce::TextButton exportRunButton { "Export Run" };
    juce::TextButton exportAllButton { "Export All" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    RetestCallback retestCallback;

    void updateLatestStats();
    void rebuildDisplayOrder();
    void applyCurrentSortToDisplayOrder();
    size_t getResultIndexForRow(int rowNumber) const;
    int getRowForResultIndex(size_t resultIndex) const;
    void selectResultIndex(size_t resultIndex);
    void editPositionAtRow(int rowNumber);
    void deleteResultAtRow(int rowNumber);
    void editNameAtRow(int rowNumber);
    void showDeleteMenuForRow(int rowNumber, juce::Component* targetComponent);
    void importClicked();
    void exportRunClicked();
    void exportAllClicked();

public:
    void saveResults(juce::ApplicationProperties& props);
    void restoreResults(juce::ApplicationProperties& props);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResultsTab)
};
