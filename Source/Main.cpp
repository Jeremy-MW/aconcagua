#include <juce_gui_basics/juce_gui_basics.h>
#include "MainWindow.h"
#include "UI/AconcaguaLookAndFeel.h"

class AconcaguaApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override           { return true; }

    void initialise(const juce::String&) override
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Aconcagua";
        options.folderName = "Aconcagua";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        appProperties.setStorageParameters(options);

        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), appProperties);
    }

    void shutdown() override
    {
        mainWindow.reset();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    AconcaguaLookAndFeel lookAndFeel;
    juce::ApplicationProperties appProperties;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(AconcaguaApplication)
