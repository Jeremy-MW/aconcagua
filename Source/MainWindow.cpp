#include "MainWindow.h"

MainWindow::MainWindow(const juce::String& name, juce::ApplicationProperties& properties)
    : DocumentWindow(name, juce::Colour(0xff1e1e2e), DocumentWindow::allButtons),
      mainContent(properties),
      appProperties(properties)
{
    setUsingNativeTitleBar(true);
    setContentNonOwned(&mainContent, true);
    setResizable(true, true);

    auto savedState = appProperties.getUserSettings() != nullptr
                        ? appProperties.getUserSettings()->getValue("mainWindowState", "")
                        : juce::String{};

    if (savedState.isNotEmpty() && restoreWindowStateFromString(savedState))
    {
        // restored
    }
    else
    {
        centreWithSize(900, 600);
    }

    setVisible(true);
}

void MainWindow::closeButtonPressed()
{
    if (auto* props = appProperties.getUserSettings())
    {
        props->setValue("mainWindowState", getWindowStateAsString());
        props->saveIfNeeded();
    }

    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

MainWindow::MainContent::MainContent(juce::ApplicationProperties& properties)
    : appProperties(properties),
      configTab(pluginLoader, benchmarkEngine, properties),
      resultsTab(properties)
{
    auto tabColour = juce::Colour(0xff1e1e2e);
    tabs.addTab("Config", tabColour, &configTab, false);
    tabs.addTab("Results", tabColour, &resultsTab, false);
    tabs.addTab("Histogram", tabColour, &histogramTab, false);
    addAndMakeVisible(tabs);

    configTab.onBenchmarkComplete([weak = juce::Component::SafePointer<MainContent>(this)](BenchmarkResult result)
    {
        if (weak == nullptr)
            return;

        weak->resultsTab.addResult(std::move(result));
        weak->resultsTab.setRetestEnabled(true);
        weak->tabs.setCurrentTabIndex(1);
    });

    resultsTab.onSelectionChanged([weak = juce::Component::SafePointer<MainContent>(this)](const BenchmarkResult* result)
    {
        if (weak != nullptr)
            weak->histogramTab.setResult(result);
    });

    resultsTab.onRetest([weak = juce::Component::SafePointer<MainContent>(this)]
    {
        if (weak == nullptr)
            return;

        weak->configTab.triggerBenchmark();
        weak->resultsTab.setRetestEnabled(false);
    });

    // Restore previous session results
    resultsTab.restoreResults(appProperties);

    setSize(900, 600);
}

MainWindow::MainContent::~MainContent()
{
    resultsTab.saveResults(appProperties);
}

void MainWindow::MainContent::resized()
{
    tabs.setBounds(getLocalBounds());
}
