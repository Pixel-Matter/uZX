
#include "MainDocument.h"

#include <JuceHeader.h>
#include <memory>

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
        setEdit(createOrLoadEdit(getRecentEditFile()));

        setResizable(true, true);
        setSize(1024, 740);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    ~MainWindow() override {
        if (edit_ != nullptr) {
            te::EditFileOperations (*edit_).save(true, true, false);
            edit_->getTempDirectory(false).deleteRecursively();
        }
        engine_.getTemporaryFileManager().getTempDirectory().deleteRecursively();
    }

    void closeButtonPressed() override {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    te::Engine& engine_;
    std::unique_ptr<te::Edit> edit_;

    String getApplicationExtension() {
        return ".motool";
    }

    File getRecentEditFile() {
        auto d = File::getSpecialLocation(File::tempDirectory).getChildFile(ProjectInfo::projectName);
        d.createDirectory();

        auto f = Helpers::findRecentEdit(d);
        if (f.existsAsFile()) {
            return f;
        } else {
            return d.getNonexistentChildFile("Test", getApplicationExtension(), false);
        }
    }

    std::unique_ptr<te::Edit> createOrLoadEdit(File editFile = {}) {
        if (editFile == File()) {
            FileChooser fc ("New Edit", File::getSpecialLocation (File::userDocumentsDirectory), "*" + getApplicationExtension());
            if (fc.browseForFileToSave(true))
                editFile = fc.getResult();
            else
                return {};
        }

        std::unique_ptr<te::Edit> edit;
        if (editFile.existsAsFile())
            edit = te::loadEditFromFile(engine_, editFile);
        else
            edit = te::createEmptyEdit(engine_, editFile);

        edit->editFileRetriever = [editFile] { return editFile; };
        edit->playInStopEnabled = true;
        return edit;
    }

    void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false) {
        jassert(edit != nullptr);

        if (savePrev && edit_ != nullptr) {
            te::EditFileOperations(*edit_).save(true, true, false);
        }
        clearContentComponent();

        edit_ = std::move(edit);
        setContentOwned(new MainDocumentComponent(engine_, *edit_), true);
    }

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
        mainWindow_ = std::make_unique<MainWindow>(title, engine_);
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
