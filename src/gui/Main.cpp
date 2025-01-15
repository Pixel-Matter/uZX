
#include "nodes/GraphEditorPanel.h"

#include <JuceHeader.h>

using namespace juce;


class MainWindow : public DocumentWindow {
public:
    MainWindow(String name)
        : DocumentWindow(name,
            Desktop::getInstance().getDefaultLookAndFeel()
                .findColour(ResizableWindow::backgroundColourId),
            DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new GraphDocumentComponent(), true);
        setResizable(true, true);
        setSize(1024, 740);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};


class MoToolApp : public JUCEApplication {
public:
    const String getApplicationName() override      {
        return ProjectInfo::projectName;
    }
    const String getApplicationVersion() override   {
        return ProjectInfo::versionString;
    }

    bool moreThanOneInstanceAllowed() override            { return true; }

    void initialise(const String& commandLine) override {
        auto title = getApplicationName() + " v" + getApplicationVersion();
        mainWindow.reset(new MainWindow(title));
    }

    void shutdown() override {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override {
        quit();
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(MoToolApp)
