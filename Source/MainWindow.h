#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginHost/PluginLoader.h"
#include "PluginHost/BenchmarkEngine.h"
#include "UI/ConfigTab.h"
#include "UI/ResultsTab.h"
#include "UI/HistogramTab.h"

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow(const juce::String& name, juce::ApplicationProperties& properties);
    void closeButtonPressed() override;

private:
    class MainContent : public juce::Component
    {
    public:
        MainContent(juce::ApplicationProperties& properties);
        ~MainContent() override;
        void resized() override;

    private:
        juce::ApplicationProperties& appProperties;
        PluginLoader pluginLoader;
        BenchmarkEngine benchmarkEngine;
        juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
        ConfigTab configTab;
        ResultsTab resultsTab;
        HistogramTab histogramTab;
    };

    MainContent mainContent;
    juce::ApplicationProperties& appProperties;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};
