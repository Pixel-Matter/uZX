
#include "Commands.h"
#include "MainDocument.h"

#include <JuceHeader.h>
#include <memory>

using namespace juce;
using namespace MoTool::Commands;


namespace {

static inline constexpr auto APP_EXTENSION = ".motool";

static String getAppFileGlob() {
    return "*" + String(APP_EXTENSION);
}

static inline File findRecentEdit(const File& dir, const String& fileGlob) {
    auto files = dir.findChildFiles(File::findFiles, false, fileGlob);
    if (files.size() > 0) {
        files.sort();
        return files.getLast();
    }
    return {};
}

static File getRecentEditFile() {
    auto d = File::getSpecialLocation(File::tempDirectory).getChildFile(ProjectInfo::projectName);
    d.createDirectory();

    auto f = findRecentEdit(d, getAppFileGlob());
    if (f.existsAsFile()) {
        return f;
    } else {
        return {};
    }
}

static File getTempEditFile() {
    auto d = File::getSpecialLocation(File::tempDirectory).getChildFile(ProjectInfo::projectName);
    d.createDirectory();
    return d.getNonexistentChildFile("Unnamed", APP_EXTENSION, false);
}

static File getStartupEditFile() {
    auto f = getRecentEditFile();
    if (!f.existsAsFile()) {
        f = getTempEditFile();
    }
    return f;
}

} // namespace


class MainWindow final : public DocumentWindow,
                         public MenuBarModel,
                         public ApplicationCommandTarget {
public:
    MainWindow(String name, te::Engine& engine)
        : DocumentWindow(name,
            Desktop::getInstance().getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
            DocumentWindow::allButtons)
        , engine_(engine)
    {
        setUsingNativeTitleBar(true);
        setSize(1024, 740); // set in setEdit
        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());

        setEdit(createOrLoadEdit(getStartupEditFile()));
        commandManager.initializeWithTarget(this);
        setMenuBar(this);
        setVisible(true);
    }

    ~MainWindow() override {
        if (edit_ != nullptr) {
            te::EditFileOperations(*edit_).save(true, true, false);
            edit_->getTempDirectory(false).deleteRecursively();
        }
        clearContentComponent();
        engine_.getTemporaryFileManager().getTempDirectory().deleteRecursively();
    }

    void closeButtonPressed() override {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

    StringArray getMenuBarNames() override {
        return AppCommands::getMenuBarNames();
    }

    PopupMenu getMenuForIndex(int /* menuIndex */, const String& menuName) override {
        return commandManager.createMenu(menuName);
    }

    void menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex*/ ) override {
        // hadled by CommandManager
        // commandManager.invokeDirectly(menuItemID, true);
    }

    ApplicationCommandTarget* getNextCommandTarget() override {
        return nullptr;
    }

    void getAllCommands(Array<CommandID>& commands) override {
        commands.addArray(AppCommands::getCommandIDs());
    }

    void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override {
        AppCommands::getCommandInfo(commandID, result);

        // Update command status based on current state
        switch (commandID) {
            case AppCommands::fileSave:
                result.setActive(edit_ != nullptr);
                break;

            case AppCommands::fileSaveAs:
                result.setActive(edit_ != nullptr);
                break;

            case AppCommands::editUndo:
                result.setActive(edit_ != nullptr && edit_->getUndoManager().canUndo());
                break;

            case AppCommands::editRedo:
                result.setActive(edit_ != nullptr && edit_->getUndoManager().canRedo());
                break;

            // case AppCommands::transportPlay:
            //     result.setActive(edit_ != nullptr && !edit_->getTransport().isPlaying());
            //     break;

            case AppCommands::transportRecord:
                result.setActive(edit_ != nullptr && !edit_->getTransport().isRecording());
                break;

            case AppCommands::transportRewind:
                result.setActive(edit_ != nullptr);
                break;

            case AppCommands::transportLoop:
                result.setTicked(edit_ != nullptr && edit_->getTransport().looping);
                break;

            // Add more dynamic command states...
        }
    }

    bool perform(const InvocationInfo& info) override {
        switch (info.commandID) {
            case AppCommands::fileNew:
                handleNew();
                break;

            case AppCommands::fileOpen:
                handleOpen();
                break;

            case AppCommands::fileSave:
                if (edit_ != nullptr) {
                    te::EditFileOperations(*edit_).save(false, true, false);
                }
                break;

            case AppCommands::fileSaveAs:
                if (edit_ != nullptr) {
                    te::EditFileOperations(*edit_).saveAs();
                }
                break;

            // ... other command handlers

            case AppCommands::fileQuit:
                JUCEApplication::getInstance()->systemRequestedQuit();
                break;

            default:
                return false;
        }

        return true;
    }

private:
    te::Engine& engine_;
    std::unique_ptr<te::Edit> edit_;
    CommandManager commandManager;

    void handleNew() {
        auto newEdit = createOrLoadEdit(getTempEditFile());
        setEdit(std::move(newEdit), true);
    }

    void handleOpen() {
        FileChooser fc ("Open file", File::getSpecialLocation(File::userDocumentsDirectory), getAppFileGlob());
        if (fc.browseForFileToOpen()) {
            auto newEdit = createOrLoadEdit(fc.getResult());
            setEdit(std::move(newEdit), true);
        }
    }

    std::unique_ptr<te::Edit> createOrLoadEdit(File editFile) {
        std::unique_ptr<te::Edit> edit;
        if (editFile.existsAsFile())
            edit = te::loadEditFromFile(engine_, editFile);
        else
            edit = te::createEmptyEdit(engine_, editFile);

        edit->playInStopEnabled = true;
        return edit;
    }

    void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false) {
        jassert(edit != nullptr);

        if (savePrev && edit_ != nullptr) {
            te::EditFileOperations(*edit_).save(true, true, false);
        }
        auto w = getWidth(), h = getHeight();
        clearContentComponent();

        edit_ = std::move(edit);
        setContentOwned(new MainDocumentComponent(engine_, *edit_), true);
        setSize(w, h);
        repaint();
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
