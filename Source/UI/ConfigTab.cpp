#include "ConfigTab.h"

ConfigTab::ConfigTab(PluginLoader& loader, BenchmarkEngine& engine, juce::ApplicationProperties& properties)
    : pluginLoader(loader), benchmarkEngine(engine), appProperties(properties)
{
    // Load button
    addAndMakeVisible(loadButton);
    loadButton.onClick = [this] { loadPluginClicked(); };

    pluginNameLabel.setText("No plugin loaded", juce::dontSendNotification);
    pluginNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(pluginNameLabel);

    // Block size
    addAndMakeVisible(blockSizeLabel);
    blockSizeSlider.setRange(2, 2000, 1);
    blockSizeSlider.setValue(512);
    blockSizeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 24);
    addAndMakeVisible(blockSizeSlider);

    // Number of blocks
    addAndMakeVisible(numBlocksLabel);
    numBlocksSlider.setRange(1, 1000000, 1);
    numBlocksSlider.setValue(10000);
    numBlocksSlider.setSkewFactorFromMidPoint(10000);
    numBlocksSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24);
    addAndMakeVisible(numBlocksSlider);

    // Sample rate
    addAndMakeVisible(sampleRateLabel);
    sampleRateBox.addItem("44100", 1);
    sampleRateBox.addItem("48000", 2);
    sampleRateBox.addItem("88200", 3);
    sampleRateBox.addItem("96000", 4);
    sampleRateBox.addItem("192000", 5);
    sampleRateBox.setSelectedId(1);
    addAndMakeVisible(sampleRateBox);

    // Channel config
    addAndMakeVisible(channelConfigLabel);
    channelConfigBox.addItem("Mono", 1);
    channelConfigBox.addItem("Stereo", 2);
    channelConfigBox.addItem("5.1", 3);
    channelConfigBox.addItem("7.1", 4);
    channelConfigBox.setSelectedId(2);
    addAndMakeVisible(channelConfigBox);

    // MIDI notes
    addAndMakeVisible(numMidiNotesLabel);
    numMidiNotesSlider.setRange(0, 128, 1);
    numMidiNotesSlider.setValue(0);
    numMidiNotesSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 24);
    addAndMakeVisible(numMidiNotesSlider);

    // Input type
    addAndMakeVisible(inputTypeLabel);
    inputTypeBox.addItem("White Noise", 1);
    inputTypeBox.addItem("MIDI", 2);
    inputTypeBox.addItem("Both", 3);
    inputTypeBox.setSelectedId(1);
    addAndMakeVisible(inputTypeBox);

    // Plugin editor toggle
    showEditorButton.setEnabled(false);
    showEditorButton.onClick = [this] { toggleEditorClicked(); };
    addAndMakeVisible(showEditorButton);

    // About button
    aboutButton.onClick = [this] { aboutClicked(); };
    addAndMakeVisible(aboutButton);

    // Go button
    goButton.setEnabled(false);
    goButton.onClick = [this] { goClicked(); };
    addAndMakeVisible(goButton);

    // Status
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    setWantsKeyboardFocus(true);

    // Restore all persisted parameters and last plugin
    restoreParameters();
}

ConfigTab::~ConfigTab()
{
    saveParameters();
}

void ConfigTab::resized()
{
    auto area = getLocalBounds().reduced(20);
    const int rowHeight = 30;
    const int spacing = 8;
    const int labelWidth = 120;

    // Load plugin row
    auto row = area.removeFromTop(rowHeight);
    loadButton.setBounds(row.removeFromLeft(120));
    row.removeFromLeft(10);
    showEditorButton.setBounds(row.removeFromRight(130));
    row.removeFromRight(10);
    pluginNameLabel.setBounds(row);
    area.removeFromTop(spacing * 2);

    // Helper lambda for parameter rows
    auto layoutRow = [&](juce::Label& label, juce::Component& control)
    {
        auto r = area.removeFromTop(rowHeight);
        label.setBounds(r.removeFromLeft(labelWidth));
        control.setBounds(r);
        area.removeFromTop(spacing);
    };

    layoutRow(blockSizeLabel, blockSizeSlider);
    layoutRow(numBlocksLabel, numBlocksSlider);
    layoutRow(sampleRateLabel, sampleRateBox);
    layoutRow(channelConfigLabel, channelConfigBox);
    layoutRow(numMidiNotesLabel, numMidiNotesSlider);
    layoutRow(inputTypeLabel, inputTypeBox);

    area.removeFromTop(spacing);

    // Go button + About
    auto goRow = area.removeFromTop(40);
    goButton.setBounds(goRow.removeFromLeft(100));
    goRow.removeFromLeft(10);
    aboutButton.setBounds(goRow.removeFromRight(70));
    goRow.removeFromRight(10);
    statusLabel.setBounds(goRow);
}

void ConfigTab::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void ConfigTab::loadPluginFromFile(const juce::File& file)
{
    editorWindow.reset();

    pluginLoader.loadPlugin(file, [this, file](const juce::String& error)
    {
        if (error.isEmpty())
        {
            pluginNameLabel.setText(pluginLoader.getPluginName(),
                                   juce::dontSendNotification);
            goButton.setEnabled(true);
            showEditorButton.setEnabled(true);
            updateMidiControls();

            // Persist plugin path and browse directory
            if (auto* props = appProperties.getUserSettings())
            {
                props->setValue("lastPluginPath", file.getFullPathName());
                props->setValue("lastBrowseDirectory", file.getParentDirectory().getFullPathName());
                props->saveIfNeeded();
            }
        }
        else
        {
            pluginNameLabel.setText(error, juce::dontSendNotification);
            goButton.setEnabled(false);
            showEditorButton.setEnabled(false);
        }
    });
}

void ConfigTab::loadPluginClicked()
{
    // Restore last browse directory, or fall back to common VST3 location
    juce::File startDir = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
                              .getChildFile("VST3");

    if (auto* props = appProperties.getUserSettings())
    {
        auto lastDir = props->getValue("lastBrowseDirectory", "");
        if (lastDir.isNotEmpty())
        {
            juce::File dir(lastDir);
            if (dir.isDirectory())
                startDir = dir;
        }
    }

    fileChooser = std::make_unique<juce::FileChooser>(
        "Select a VST3 plugin", startDir, "*.vst3");

    auto chooserFlags = juce::FileBrowserComponent::openMode
                      | juce::FileBrowserComponent::canSelectFiles
                      | juce::FileBrowserComponent::canSelectDirectories;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result == juce::File{})
            return;

        loadPluginFromFile(result);
    });
}

void ConfigTab::goClicked()
{
    if (!pluginLoader.isPluginLoaded() || benchmarkEngine.isRunning())
        return;

    goButton.setEnabled(false);
    loadButton.setEnabled(false);
    statusLabel.setText("Running benchmark...", juce::dontSendNotification);

    auto config = gatherConfig();

    benchmarkEngine.startBenchmark(
        pluginLoader.getPluginInstance(),
        pluginLoader.getPluginName(),
        config,
        [this](BenchmarkResult result)
        {
            goButton.setEnabled(true);
            loadButton.setEnabled(true);
            statusLabel.setText("Complete: " + juce::String(result.totalMs, 1) + " ms total, "
                                + juce::String(result.avgUs, 1) + " us avg",
                                juce::dontSendNotification);

            if (benchmarkCompleteCallback)
                benchmarkCompleteCallback(std::move(result));
        });
}

BenchmarkConfig ConfigTab::gatherConfig() const
{
    BenchmarkConfig cfg;
    cfg.blockSize = static_cast<int>(blockSizeSlider.getValue());
    cfg.numBlocks = static_cast<int>(numBlocksSlider.getValue());

    const int sampleRates[] = { 44100, 48000, 88200, 96000, 192000 };
    cfg.sampleRate = sampleRates[sampleRateBox.getSelectedId() - 1];

    const int channelCounts[] = { 1, 2, 6, 8 };
    cfg.numChannels = channelCounts[channelConfigBox.getSelectedId() - 1];

    cfg.numMidiNotes = static_cast<int>(numMidiNotesSlider.getValue());

    switch (inputTypeBox.getSelectedId())
    {
        case 1:  cfg.inputType = InputType::Noise; break;
        case 2:  cfg.inputType = InputType::Midi;  break;
        case 3:  cfg.inputType = InputType::Both;   break;
        default: cfg.inputType = InputType::Noise;  break;
    }

    return cfg;
}

void ConfigTab::toggleEditorClicked()
{
    if (editorWindow != nullptr)
    {
        editorWindow.reset();
        return;
    }

    auto* editor = pluginLoader.createEditor();
    if (editor == nullptr)
    {
        statusLabel.setText("Plugin has no GUI", juce::dontSendNotification);
        return;
    }

    editorWindow = std::make_unique<juce::DocumentWindow>(
        pluginLoader.getPluginName() + " - Editor",
        juce::Colour(0xff1e1e2e),
        juce::DocumentWindow::closeButton);

    editorWindow->setUsingNativeTitleBar(true);
    editorWindow->setContentOwned(editor, true);
    editorWindow->setResizable(false, false);
    editorWindow->centreWithSize(editor->getWidth(), editor->getHeight());
    editorWindow->setVisible(true);
}

void ConfigTab::saveParameters()
{
    if (auto* props = appProperties.getUserSettings())
    {
        props->setValue("blockSize", static_cast<int>(blockSizeSlider.getValue()));
        props->setValue("numBlocks", static_cast<int>(numBlocksSlider.getValue()));
        props->setValue("sampleRateId", sampleRateBox.getSelectedId());
        props->setValue("channelConfigId", channelConfigBox.getSelectedId());
        props->setValue("numMidiNotes", static_cast<int>(numMidiNotesSlider.getValue()));
        props->setValue("inputTypeId", inputTypeBox.getSelectedId());
        props->saveIfNeeded();
    }
}

void ConfigTab::restoreParameters()
{
    if (auto* props = appProperties.getUserSettings())
    {
        blockSizeSlider.setValue(props->getIntValue("blockSize", 512), juce::dontSendNotification);
        numBlocksSlider.setValue(props->getIntValue("numBlocks", 10000), juce::dontSendNotification);
        sampleRateBox.setSelectedId(props->getIntValue("sampleRateId", 1), juce::dontSendNotification);
        channelConfigBox.setSelectedId(props->getIntValue("channelConfigId", 2), juce::dontSendNotification);
        numMidiNotesSlider.setValue(props->getIntValue("numMidiNotes", 0), juce::dontSendNotification);
        inputTypeBox.setSelectedId(props->getIntValue("inputTypeId", 1), juce::dontSendNotification);

        auto lastPlugin = props->getValue("lastPluginPath", "");
        if (lastPlugin.isNotEmpty())
        {
            juce::File pluginFile(lastPlugin);
            if (pluginFile.exists())
                loadPluginFromFile(pluginFile);
        }
    }
}

bool ConfigTab::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey && goButton.isEnabled())
    {
        goClicked();
        return true;
    }

    if (key == juce::KeyPress::escapeKey && benchmarkEngine.isRunning())
    {
        benchmarkEngine.signalThreadShouldExit();
        statusLabel.setText("Cancelling...", juce::dontSendNotification);
        return true;
    }

    return false;
}

void ConfigTab::aboutClicked()
{
    auto* app = juce::JUCEApplication::getInstance();
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        "About Aconcagua",
        "Aconcagua v" + app->getApplicationVersion() + "\n\n"
        "VST3 Plugin Performance Benchmarking Tool\n\n"
        "Keyboard shortcuts:\n"
        "  Enter - Run benchmark\n"
        "  Escape - Cancel running benchmark",
        "OK");
}

void ConfigTab::updateMidiControls()
{
    bool midi = pluginLoader.acceptsMidi();
    numMidiNotesSlider.setEnabled(midi);
    numMidiNotesLabel.setEnabled(midi);

    if (!midi)
    {
        // If input type was MIDI-only, switch to Noise
        if (inputTypeBox.getSelectedId() == 2)
            inputTypeBox.setSelectedId(1);

        // Remove MIDI-only option, keep Both available but it will just send noise
        numMidiNotesSlider.setValue(0, juce::dontSendNotification);
    }
}
