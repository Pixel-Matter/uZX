
#include "nodes/GraphEditorPanel.h"
#include "MainDocument.h"

#include <JuceHeader.h>

using namespace juce;
// namespace te = tracktion::engine;


class MainWindow : public DocumentWindow {
public:
    MainWindow(String name, te::Engine& engine)
        : DocumentWindow(name,
            Desktop::getInstance().getDefaultLookAndFeel()
                .findColour(ResizableWindow::backgroundColourId),
            DocumentWindow::allButtons)
        , engine_(engine)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainDocumentComponent(engine_), true);
        // setContentOwned(new GraphDocumentComponent(), true);
        setResizable(true, true);
        setSize(1024, 740);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    te::Engine& engine_;
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

    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise(const String&) override {
        auto title = getApplicationName() + " v" + getApplicationVersion();
        mainWindow_.reset(new MainWindow(title, engine_));
    }

    void shutdown() override {
        mainWindow_ = nullptr;
    }

    void systemRequestedQuit() override {
        quit();
    }

private:
    te::Engine engine_ { ProjectInfo::projectName };
    std::unique_ptr<MainWindow> mainWindow_;
};

START_JUCE_APPLICATION(MoToolApp)
