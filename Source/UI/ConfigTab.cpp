#include "ConfigTab.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace
{
    // Channel-count combo entries, shared by input and output selectors.
    struct ChannelOption { const char* label; int count; };
    constexpr ChannelOption kChannelOptions[] = {
        { "Mono",   1 },
        { "Stereo", 2 },
        { "5.1",    6 },
        { "7.1",    8 }
    };

    void populateChannelCombo(juce::ComboBox& box)
    {
        for (int i = 0; i < (int) std::size(kChannelOptions); ++i)
            box.addItem(kChannelOptions[i].label, i + 1);
    }

    int channelCountForId(int id)
    {
        const int idx = juce::jlimit(1, (int) std::size(kChannelOptions), id) - 1;
        return kChannelOptions[idx].count;
    }

    constexpr int kSampleRates[] = { 44100, 48000, 88200, 96000, 192000 };

    int sampleRateIdForRate(int rate)
    {
        // Map an exact sample rate to its combo id (1-based). Returns 0 if no match.
        for (int i = 0; i < (int) std::size(kSampleRates); ++i)
            if (kSampleRates[i] == rate)
                return i + 1;
        return 0;
    }

    int selectedSampleRateForBox(const juce::ComboBox& box)
    {
        const int id = box.getSelectedId();
        if (id < 1 || id > (int) std::size(kSampleRates))
            return kSampleRates[0];

        return kSampleRates[id - 1];
    }

    juce::String pluginStateKeyForFile(const juce::File& file)
    {
        return "pluginState." + juce::String::toHexString(file.getFullPathName().toLowerCase().hashCode64());
    }

    juce::String pluginEditorWindowBoundsKeyForFile(const juce::File& file)
    {
        return "pluginEditorWindowBounds." + juce::String::toHexString(file.getFullPathName().toLowerCase().hashCode64());
    }

    juce::String boundsToString(juce::Rectangle<int> bounds)
    {
        return juce::String(bounds.getX()) + ","
             + juce::String(bounds.getY()) + ","
             + juce::String(bounds.getWidth()) + ","
             + juce::String(bounds.getHeight());
    }

    juce::Rectangle<int> boundsFromString(const juce::String& text)
    {
        juce::StringArray parts;
        parts.addTokens(text, ",", "");

        if (parts.size() != 4)
            return {};

        return { parts[0].getIntValue(),
                 parts[1].getIntValue(),
                 parts[2].getIntValue(),
                 parts[3].getIntValue() };
    }

    juce::Rectangle<int> clampBoundsToDisplays(juce::Rectangle<int> bounds)
    {
        const auto displays = juce::Desktop::getInstance().getDisplays();
        const auto displayArea = displays.getDisplayForRect(bounds) != nullptr
                                   ? displays.getDisplayForRect(bounds)->userArea
                                   : displays.getPrimaryDisplay()->userArea;

        bounds.setSize(juce::jmin(bounds.getWidth(), displayArea.getWidth()),
                       juce::jmin(bounds.getHeight(), displayArea.getHeight()));

        if (bounds.getRight() > displayArea.getRight())
            bounds.setX(displayArea.getRight() - bounds.getWidth());
        if (bounds.getBottom() > displayArea.getBottom())
            bounds.setY(displayArea.getBottom() - bounds.getHeight());
        if (bounds.getX() < displayArea.getX())
            bounds.setX(displayArea.getX());
        if (bounds.getY() < displayArea.getY())
            bounds.setY(displayArea.getY());

        return bounds;
    }

    // DocumentWindow whose close button triggers a caller-supplied callback,
    // instead of the default JUCEApplicationBase::quit() behaviour.
    class EditorHostWindow : public juce::DocumentWindow
    {
    public:
        using juce::DocumentWindow::DocumentWindow;
        std::function<void()> onCloseButton;
        void closeButtonPressed() override { if (onCloseButton) onCloseButton(); }
    };
}

ConfigTab::ConfigTab(PluginLoader& loader, BenchmarkEngine& engine, juce::ApplicationProperties& properties)
    : pluginLoader(loader), benchmarkEngine(engine), appProperties(properties)
{
    // Initialise live audio device with saved state (if any) before any UI
    // restoration, so the device's SR/BS can seed the combos.
    juce::String savedAudio;
    if (auto* props = appProperties.getUserSettings())
        savedAudio = props->getValue("audioDeviceState", "");

    liveAudio.onDeviceConfigChanged = [weak = juce::Component::SafePointer<ConfigTab>(this)](double sr, int bs)
    {
        if (weak != nullptr)
            weak->seedConfigFromDevice(sr, bs);
    };
    liveAudio.initialise(savedAudio);
    liveAudio.attach();

    // Load button
    addAndMakeVisible(loadButton);
    loadButton.onClick = [this] { loadPluginClicked(); };

    pluginNameLabel.setText("No plugin loaded", juce::dontSendNotification);
    pluginNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(pluginNameLabel);

    // Name (free-form label for this run)
    addAndMakeVisible(nameLabel);
    nameEditor.setTextToShowWhenEmpty("(optional)", juce::Colour(0xff6c7086));
    nameEditor.onTextChange = [this]
    {
        nameClearButton.setVisible(nameEditor.getText().isNotEmpty());
        resized();
    };
    addAndMakeVisible(nameEditor);

    nameClearButton.setVisible(false);
    nameClearButton.onClick = [this]
    {
        nameEditor.setText({}, juce::sendNotification);
        nameEditor.grabKeyboardFocus();
    };
    addChildComponent(nameClearButton);

    // Block size
    addAndMakeVisible(blockSizeLabel);
    blockSizeSlider.setRange(2, 2000, 1);
    blockSizeSlider.setValue(512);
    blockSizeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 24);
    blockSizeSlider.onDragEnd     = [this] { applyPlayConfigAndResumeProcessing(); };
    blockSizeSlider.onValueChange = [this]
    {
        if (! blockSizeSlider.isMouseButtonDown())
            applyPlayConfigAndResumeProcessing();
    };
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
    for (int i = 0; i < (int) std::size(kSampleRates); ++i)
        sampleRateBox.addItem(juce::String(kSampleRates[i]), i + 1);
    sampleRateBox.setSelectedId(1);
    sampleRateBox.onChange = [this] { applyPlayConfigAndResumeProcessing(); };
    addAndMakeVisible(sampleRateBox);

    // Input channels
    addAndMakeVisible(inputChannelsLabel);
    populateChannelCombo(inputChannelsBox);
    inputChannelsBox.setSelectedId(2);
    inputChannelsBox.onChange = [this] { applyPlayConfigAndResumeProcessing(); };
    addAndMakeVisible(inputChannelsBox);

    // Output channels
    addAndMakeVisible(outputChannelsLabel);
    populateChannelCombo(outputChannelsBox);
    outputChannelsBox.setSelectedId(2);
    outputChannelsBox.onChange = [this] { applyPlayConfigAndResumeProcessing(); };
    addAndMakeVisible(outputChannelsBox);

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

    // Live audio controls
    liveAudioToggle.onClick = [this] { onLiveAudioToggled(); };
    addAndMakeVisible(liveAudioToggle);
    audioConfigButton.onClick = [this] { audioConfigClicked(); };
    addAndMakeVisible(audioConfigButton);

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
    if (benchmarkEngine.isRunning())
    {
        benchmarkEngine.signalThreadShouldExit();
        benchmarkEngine.stopThread(5000);
    }

    saveCurrentPluginEditorWindowState();
    saveCurrentPluginState();
    saveParameters();
    stopProcessingAndRelease();
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

    auto layoutRow = [&](juce::Label& label, juce::Component& control)
    {
        auto r = area.removeFromTop(rowHeight);
        label.setBounds(r.removeFromLeft(labelWidth));
        control.setBounds(r);
        area.removeFromTop(spacing);
    };

    layoutRow(nameLabel,           nameEditor);
    {
        const int fontH = (int) std::ceil(nameEditor.getFont().getHeight());
        const int topIndent = juce::jmax(0, (nameEditor.getHeight() - fontH) / 2);
        nameEditor.setIndents(4, topIndent);

        auto editorBounds = nameEditor.getBounds();
        const int sz = juce::jmax(18, editorBounds.getHeight() - 6);
        nameClearButton.setBounds(editorBounds.getRight() - sz - 3,
                                  editorBounds.getY() + (editorBounds.getHeight() - sz) / 2,
                                  sz, sz);
    }
    layoutRow(blockSizeLabel,      blockSizeSlider);
    layoutRow(numBlocksLabel,      numBlocksSlider);
    layoutRow(sampleRateLabel,     sampleRateBox);
    layoutRow(inputChannelsLabel,  inputChannelsBox);
    layoutRow(outputChannelsLabel, outputChannelsBox);
    layoutRow(numMidiNotesLabel,   numMidiNotesSlider);
    layoutRow(inputTypeLabel,      inputTypeBox);

    area.removeFromTop(spacing);

    // Live audio row
    auto liveRow = area.removeFromTop(rowHeight);
    liveAudioToggle.setBounds(liveRow.removeFromLeft(140));
    liveRow.removeFromLeft(10);
    audioConfigButton.setBounds(liveRow.removeFromLeft(120));
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
    saveCurrentPluginEditorWindowState();
    editorWindow.reset();
    saveCurrentPluginState();
    stopProcessingAndRelease();

    pluginLoader.loadPlugin(file, [this, file](const juce::String& error)
    {
        if (error.isEmpty())
        {
            pluginNameLabel.setText(pluginLoader.getPluginName(),
                                   juce::dontSendNotification);
            goButton.setEnabled(true);
            showEditorButton.setEnabled(true);
            updateMidiControls();
            currentPluginFile = file;
            restorePluginStateForFile(file);

            if (auto* props = appProperties.getUserSettings())
            {
                props->setValue("lastPluginPath", file.getFullPathName());
                props->setValue("lastBrowseDirectory", file.getParentDirectory().getFullPathName());
                props->saveIfNeeded();
            }

            applyPlayConfigAndResumeProcessing();
        }
        else
        {
            pluginNameLabel.setText(error, juce::dontSendNotification);
            goButton.setEnabled(pluginLoader.isPluginLoaded());
            showEditorButton.setEnabled(pluginLoader.isPluginLoaded());
            updateMidiControls();
            applyPlayConfigAndResumeProcessing();
        }
    });
}

void ConfigTab::loadPluginClicked()
{
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
    if (! pluginLoader.isPluginLoaded() || benchmarkEngine.isRunning())
        return;

    goButton.setEnabled(false);
    loadButton.setEnabled(false);
    statusLabel.setText("Running benchmark...", juce::dontSendNotification);

    // Suspend live audio / pump and re-prepare the plugin for benchmark settings.
    liveAudio.setPlugin(nullptr, 0, 0);
    idlePump.stop();
    if (pluginIsPrepared)
    {
        pluginLoader.releasePlugin();
        pluginIsPrepared = false;
    }

    auto config = gatherConfig();
    pluginLoader.preparePlugin(config.sampleRate, config.blockSize,
                               config.numInputChannels, config.numOutputChannels);
    pluginIsPrepared = true;

    benchmarkEngine.startBenchmark(
        pluginLoader.getPluginInstance(),
        pluginLoader.getPluginName(),
        config,
        [weak = juce::Component::SafePointer<ConfigTab>(this)](BenchmarkResult result)
        {
            if (weak == nullptr)
                return;

            weak->goButton.setEnabled(true);
            weak->loadButton.setEnabled(true);
            weak->statusLabel.setText("Complete: " + juce::String(result.totalMs, 1) + " ms total, "
                                      + juce::String(result.avgUs, 1) + " us avg",
                                      juce::dontSendNotification);

            // Re-release: live or pump path will re-prepare with its own SR/BS.
            if (weak->pluginIsPrepared)
            {
                weak->pluginLoader.releasePlugin();
                weak->pluginIsPrepared = false;
            }
            weak->applyPlayConfigAndResumeProcessing();

            if (weak->benchmarkCompleteCallback)
                weak->benchmarkCompleteCallback(std::move(result));
        });
}

BenchmarkConfig ConfigTab::gatherConfig() const
{
    BenchmarkConfig cfg;
    cfg.name = nameEditor.getText().trim();
    cfg.blockSize = static_cast<int>(blockSizeSlider.getValue());
    cfg.numBlocks = static_cast<int>(numBlocksSlider.getValue());

    cfg.sampleRate = selectedSampleRateForBox(sampleRateBox);

    cfg.numInputChannels  = channelCountForId(inputChannelsBox.getSelectedId());
    cfg.numOutputChannels = channelCountForId(outputChannelsBox.getSelectedId());

    cfg.numMidiNotes = static_cast<int>(numMidiNotesSlider.getValue());

    switch (inputTypeBox.getSelectedId())
    {
        case 1:  cfg.inputType = InputType::Noise; break;
        case 2:  cfg.inputType = InputType::Midi;  break;
        case 3:  cfg.inputType = InputType::Both;  break;
        default: cfg.inputType = InputType::Noise; break;
    }

    return cfg;
}

void ConfigTab::applyPlayConfigAndResumeProcessing()
{
    if (! pluginLoader.isPluginLoaded())
        return;

    // Stop everything that might be calling processBlock.
    liveAudio.setPlugin(nullptr, 0, 0);
    idlePump.stop();

    if (pluginIsPrepared)
    {
        pluginLoader.releasePlugin();
        pluginIsPrepared = false;
    }

    const int pIn  = channelCountForId(inputChannelsBox.getSelectedId());
    const int pOut = channelCountForId(outputChannelsBox.getSelectedId());

    const bool wantLive = liveAudioToggle.getToggleState() && liveAudio.isDeviceOpen();

    if (wantLive)
    {
        const double sr = liveAudio.getDeviceSampleRate();
        const int    bs = liveAudio.getDeviceBlockSize();

        pluginLoader.preparePlugin(sr, bs, pIn, pOut);
        pluginIsPrepared = true;

        liveAudio.setPlugin(pluginLoader.getPluginInstance(), pIn, pOut);
    }
    else
    {
        auto cfg = gatherConfig();
        pluginLoader.preparePlugin(cfg.sampleRate, cfg.blockSize, pIn, pOut);
        pluginIsPrepared = true;

        idlePump.start(pluginLoader.getPluginInstance(),
                       cfg.sampleRate, cfg.blockSize, pIn, pOut);
    }
}

void ConfigTab::stopProcessingAndRelease()
{
    liveAudio.setPlugin(nullptr, 0, 0);
    idlePump.stop();

    if (pluginIsPrepared)
    {
        pluginLoader.releasePlugin();
        pluginIsPrepared = false;
    }
}

void ConfigTab::onLiveAudioToggled()
{
    if (auto* props = appProperties.getUserSettings())
    {
        props->setValue("liveAudioActive", liveAudioToggle.getToggleState());
        props->saveIfNeeded();
    }

    if (liveAudioToggle.getToggleState() && ! liveAudio.isDeviceOpen())
    {
        statusLabel.setText("No audio device open — using idle pump.",
                            juce::dontSendNotification);
    }

    applyPlayConfigAndResumeProcessing();
}

void ConfigTab::audioConfigClicked()
{
    auto* selector = new juce::AudioDeviceSelectorComponent(
        liveAudio.getDeviceManager(),
        /*minIn*/  0, /*maxIn*/  256,
        /*minOut*/ 0, /*maxOut*/ 256,
        /*showMidiInput*/   true,
        /*showMidiOutput*/  false,
        /*stereoPairs*/     true,
        /*hideAdvanced*/    false);
    selector->setSize(550, 500);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(selector);
    opts.dialogTitle = "Audio Configuration";
    opts.dialogBackgroundColour = juce::Colour(0xff1e1e2e);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.launchAsync();
}

void ConfigTab::seedConfigFromDevice(double sampleRate, int blockSize)
{
    // Match Sample Rate combo if device's rate is one of our standard values.
    const int srId = sampleRateIdForRate((int) sampleRate);
    if (srId > 0)
        sampleRateBox.setSelectedId(srId, juce::dontSendNotification);

    if (blockSize >= blockSizeSlider.getMinimum() && blockSize <= blockSizeSlider.getMaximum())
        blockSizeSlider.setValue(blockSize, juce::dontSendNotification);

    // Re-prepare for the new device settings if live mode is on.
    applyPlayConfigAndResumeProcessing();
}

void ConfigTab::toggleEditorClicked()
{
    if (editorWindow != nullptr)
    {
        saveCurrentPluginEditorWindowState();
        editorWindow.reset();
        return;
    }

    auto* editor = pluginLoader.createEditor();
    if (editor == nullptr)
    {
        statusLabel.setText("Plugin has no GUI", juce::dontSendNotification);
        return;
    }

    auto host = std::make_unique<EditorHostWindow>(
        pluginLoader.getPluginName() + " - Editor",
        juce::Colour(0xff1e1e2e),
        juce::DocumentWindow::closeButton);

    host->onCloseButton = [this]
    {
        juce::MessageManager::callAsync([weak = juce::Component::SafePointer<ConfigTab>(this)]
        {
            if (weak != nullptr)
            {
                weak->saveCurrentPluginEditorWindowState();
                weak->editorWindow.reset();
            }
        });
    };

    host->setUsingNativeTitleBar(true);
    host->setContentOwned(editor, true);
    host->setResizable(false, false);

    if (currentPluginFile != juce::File{})
        restorePluginEditorWindowStateForFile(currentPluginFile, *host);
    else
        host->centreWithSize(editor->getWidth(), editor->getHeight());

    host->setVisible(true);

    editorWindow = std::move(host);
}

void ConfigTab::saveParameters()
{
    if (auto* props = appProperties.getUserSettings())
    {
        props->setValue("configName", nameEditor.getText());
        props->setValue("blockSize", static_cast<int>(blockSizeSlider.getValue()));
        props->setValue("numBlocks", static_cast<int>(numBlocksSlider.getValue()));
        props->setValue("sampleRateId",      sampleRateBox.getSelectedId());
        props->setValue("inputChannelsId",   inputChannelsBox.getSelectedId());
        props->setValue("outputChannelsId",  outputChannelsBox.getSelectedId());
        props->setValue("numMidiNotes",      static_cast<int>(numMidiNotesSlider.getValue()));
        props->setValue("inputTypeId",       inputTypeBox.getSelectedId());
        props->setValue("liveAudioActive",   liveAudioToggle.getToggleState());
        props->setValue("audioDeviceState",  liveAudio.getStateAsXmlString());
        props->saveIfNeeded();
    }
}

void ConfigTab::saveCurrentPluginState()
{
    if (! pluginLoader.isPluginLoaded() || currentPluginFile == juce::File{})
        return;

    // Detach realtime callers before asking the plugin to serialise itself.
    liveAudio.setPlugin(nullptr, 0, 0);
    idlePump.stop();

    juce::MemoryBlock state;
    pluginLoader.getPluginInstance()->getStateInformation(state);

    if (auto* props = appProperties.getUserSettings())
    {
        props->setValue(pluginStateKeyForFile(currentPluginFile), state.toBase64Encoding());
        props->saveIfNeeded();
    }
}

void ConfigTab::restorePluginStateForFile(const juce::File& file)
{
    if (! pluginLoader.isPluginLoaded())
        return;

    juce::String encodedState;
    if (auto* props = appProperties.getUserSettings())
        encodedState = props->getValue(pluginStateKeyForFile(file), {});

    if (encodedState.isEmpty())
        return;

    juce::MemoryBlock state;
    if (state.fromBase64Encoding(encodedState) && state.getSize() > 0)
    {
        pluginLoader.getPluginInstance()->setStateInformation(state.getData(),
                                                              static_cast<int>(state.getSize()));
    }
}

void ConfigTab::saveCurrentPluginEditorWindowState()
{
    if (editorWindow == nullptr || currentPluginFile == juce::File{})
        return;

    if (auto* props = appProperties.getUserSettings())
    {
        props->setValue(pluginEditorWindowBoundsKeyForFile(currentPluginFile),
                        boundsToString(editorWindow->getBounds()));
        props->saveIfNeeded();
    }
}

void ConfigTab::restorePluginEditorWindowStateForFile(const juce::File& file,
                                                      juce::DocumentWindow& window)
{
    auto* content = window.getContentComponent();
    const int naturalWidth = content != nullptr ? juce::jmax(1, content->getWidth()) : 600;
    const int naturalHeight = content != nullptr ? juce::jmax(1, content->getHeight()) : 400;

    juce::String savedBoundsText;
    if (auto* props = appProperties.getUserSettings())
        savedBoundsText = props->getValue(pluginEditorWindowBoundsKeyForFile(file), {});

    const auto savedBounds = boundsFromString(savedBoundsText);
    if (savedBounds.isEmpty())
    {
        window.centreWithSize(naturalWidth, naturalHeight);
        return;
    }

    window.setBounds(clampBoundsToDisplays(savedBounds.withSizeKeepingCentre(naturalWidth, naturalHeight)));
}

void ConfigTab::restoreParameters()
{
    if (auto* props = appProperties.getUserSettings())
    {
        nameEditor.setText(props->getValue("configName", ""), juce::sendNotification);
        blockSizeSlider.setValue(props->getIntValue("blockSize", 512), juce::dontSendNotification);
        numBlocksSlider.setValue(props->getIntValue("numBlocks", 10000), juce::dontSendNotification);
        sampleRateBox.setSelectedId(props->getIntValue("sampleRateId", 1), juce::dontSendNotification);

        const int legacyChId = props->getIntValue("channelConfigId", 2);
        inputChannelsBox.setSelectedId(props->getIntValue("inputChannelsId",  legacyChId), juce::dontSendNotification);
        outputChannelsBox.setSelectedId(props->getIntValue("outputChannelsId", legacyChId), juce::dontSendNotification);

        numMidiNotesSlider.setValue(props->getIntValue("numMidiNotes", 0), juce::dontSendNotification);
        inputTypeBox.setSelectedId(props->getIntValue("inputTypeId", 1), juce::dontSendNotification);

        liveAudioToggle.setToggleState(props->getBoolValue("liveAudioActive", false),
                                       juce::dontSendNotification);

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

    if (! midi)
    {
        if (inputTypeBox.getSelectedId() == 2)
            inputTypeBox.setSelectedId(1);

        numMidiNotesSlider.setValue(0, juce::dontSendNotification);
    }
}
