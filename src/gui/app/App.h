#pragma once

#include <JuceHeader.h>

#include "MainDocument.h"
#include "Commands.h"

#include "../common/LookAndFeel.h"
// #include "../../model/Selectable.h"
#include "../../util/FileOps.h"
#include "tracktion_engine/tracktion_engine.h"

#include <memory>

using namespace juce;
using namespace MoTool::Commands;
using namespace MoTool::EditFileOps;


namespace MoTool {

class MainWindow final : public DocumentWindow,
                         public MenuBarModel,
                         public ApplicationCommandTarget,
                         private ChangeListener {
public:
    MainWindow(String name, te::Engine& engine, CommandManager& cmdMgr)
        : DocumentWindow(name,
            Colors::Theme::backgroundAlt,
            DocumentWindow::allButtons)
        , engine_ {engine}
        , commandManager_ {cmdMgr}
    {
        setUsingNativeTitleBar(true);
        setSize(1024, 740);
        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());

        setEdit(createOrLoadEdit(getStartupEditFile()));
        commandManager_.initializeWithTarget(this);
        #if JUCE_MAC
            setMacMainMenu(this);
        #else
            setMenuBar(this);
        #endif
        // Install key mappings
        commandManager_.getKeyMappings()->resetToDefaultMappings();
        addKeyListener(commandManager_.getKeyMappings());

        selectionManager_.addChangeListener(this);

        setVisible(true);
    }

    ~MainWindow() override {
        if (edit_ != nullptr) {
            te::EditFileOperations(*edit_).save(true, true, false);
            edit_->getTempDirectory(false).deleteRecursively();
        }
        selectionManager_.removeChangeListener(this);
        removeKeyListener(commandManager_.getKeyMappings());
        #if JUCE_MAC
            setMacMainMenu(nullptr);
        #else
            setMenuBar(nullptr);
        #endif
        clearContentComponent();
        engine_.getTemporaryFileManager().getTempDirectory().deleteRecursively();
    }

    te::Edit* getEdit() {
        return edit_.get();
    }

    te::SelectionManager& getSelectionManager() {
        return selectionManager_;
    }

    void closeButtonPressed() override {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

    StringArray getMenuBarNames() override {
        return AppCommands::getMenuBarNames();
    }

    PopupMenu getMenuForIndex(int /* menuIndex */, const String& menuName) override {
        return commandManager_.createMenu(menuName);
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
        // DBG("MainWindow::getCommandInfo: " << commandID);
        // Get main command info, name, desc, keypresses
        AppCommands::getCommandInfo(commandID, result);

        // Update command status based on current state
        switch (commandID) {
            case AppCommands::fileSave:
                result.setActive(edit_ != nullptr);
                break;

            case AppCommands::fileSaveAs:
                result.setActive(edit_ != nullptr);
                break;

            case AppCommands::fileReveal:
                result.setActive(edit_ != nullptr);
                break;

            case AppCommands::editUndo:
                result.setActive(edit_ != nullptr && edit_->getUndoManager().canUndo());
                break;

            case AppCommands::editRedo:
                result.setActive(edit_ != nullptr && edit_->getUndoManager().canRedo());
                break;

            // TODO  Theese commands are not being updated on selection change, only transportPlay and transportRecordStop are updated
            // case AppCommands::editDelete: [[fallthrough]];
            // case AppCommands::editCut:    [[fallthrough]];
            // case AppCommands::editCopy:
            //     if (edit_ != nullptr) {
            //         if (auto sm = edit_->engine.getUIBehaviour().getCurrentlyFocusedSelectionManager()) {
            //             DBG("Selected objects: " << sm->getSelectedObjects().size());
            //             result.setActive(!sm->getSelectedObjects().isEmpty());
            //             break;
            //         }
            //     }
            //     result.setActive(false);
            //     break;

            case AppCommands::transportPlay:
                result.setActive(edit_ != nullptr);
                break;

            case AppCommands::transportRecord:
                result.setActive(edit_ != nullptr && !edit_->getTransport().isRecording());
                break;

            case AppCommands::transportRecordStop:
                result.setActive(edit_ != nullptr && edit_->getTransport().isRecording());
                break;

            case AppCommands::transportToStart:
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
                te::AppFunctions::saveEdit();
                break;

            case AppCommands::fileSaveAs:
                handleSaveAs();
                break;

            case AppCommands::fileReveal:
                if (edit_ != nullptr) {
                    te::EditFileOperations(*edit_).save(false, true, false);
                    te::EditFileOperations(*edit_).getEditFile().revealToUser();
                }
                break;

            case AppCommands::fileQuit:
                JUCEApplication::getInstance()->systemRequestedQuit();
                break;

            case AppCommands::editUndo:
                te::AppFunctions::undo();
                break;

            case AppCommands::editRedo:
                te::AppFunctions::redo();
                break;

            case AppCommands::editDelete:
                te::AppFunctions::deleteSelected();
                break;

            case AppCommands::transportPlay:
                te::AppFunctions::startStopPlay();
                break;

            case AppCommands::transportRecord:
                handleRecord();
                break;

            case AppCommands::transportRecordStop:
                handleRecord();
                break;

            case AppCommands::transportToStart:
                te::AppFunctions::goToStart();
                break;

            case AppCommands::transportToEnd:
                te::AppFunctions::goToEnd();
                break;

            case AppCommands::transportLoop:
                te::AppFunctions::toggleLoop();
                break;

            case AppCommands::settingsAudioMidi:
                EngineHelpers::showAudioDeviceSettings(engine_);
                break;

            case AppCommands::settingsPlugins:
                hanldePluginManager();
                break;

            default:
                return false;
        }

        return true;
    }

private:
    te::Engine& engine_;
    std::unique_ptr<te::Edit> edit_;
    CommandManager& commandManager_;
    te::SelectionManager selectionManager_ {engine_};

    void handleNew() {
        auto newEdit = createOrLoadEdit(getTempEditFile());
        setEdit(std::move(newEdit), true);
    }

    void handleOpen() {
        FileChooser fc("Open file", File::getSpecialLocation(File::userDocumentsDirectory), getAppFileGlob());
        if (fc.browseForFileToOpen()) {
            auto newEdit = createOrLoadEdit(fc.getResult());
            setEdit(std::move(newEdit), true);
        }
    }

    void handleSaveAs() {
        if (edit_ == nullptr) return;

        auto efo = te::EditFileOperations(*edit_);
        auto newEditName = te::getNonExistentSiblingWithIncrementedNumberSuffix(efo.getEditFile(), false);
        juce::FileChooser fc("Save As...", newEditName, getAppFileGlob());

        if (fc.browseForFileToSave(false)) {
            efo.saveAs(fc.getResult().withFileExtension(EditFileOps::EDIT_FILE_SUFFIX));
        }
    }

    void handleRecord() {
        if (edit_ == nullptr) return;

        bool wasRecording = edit_->getTransport().isRecording();
        te::AppFunctions::record();
        if (wasRecording) {
            te::EditFileOperations(*edit_).save(true, true, false);
        }
    }

    void hanldePluginManager() {
        DialogWindow::LaunchOptions o;
        o.dialogTitle                   = TRANS("Plugins");
        o.dialogBackgroundColour        = juce::Colours::black;
        o.escapeKeyTriggersCloseButton  = true;
        o.useNativeTitleBar             = true;
        o.resizable                     = true;
        o.useBottomRightCornerResizer   = true;

        auto v = new PluginListComponent (engine_.getPluginManager().pluginFormatManager,
                                          engine_.getPluginManager().knownPluginList,
                                          engine_.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal"),
                                          std::addressof(engine_.getPropertyStorage().getPropertiesFile()));
        v->setSize(800, 600);
        o.content.setOwned(v);
        o.launchAsync();
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
        setName(te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension());
        setContentOwned(new MainDocumentComponent(*edit_, getSelectionManager()), true);
        setSize(w, h);
        repaint();
    }

    void changeListenerCallback (ChangeBroadcaster*) override {
        // Called when the selection changes
        // DBG("Selection changed");
        commandManager_.commandStatusChanged();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};


class MoToolApp : public JUCEApplication {
public:
    MoToolApp();

    const String getApplicationName() override      {
        return ProjectInfo::projectName;
    }
    const String getApplicationVersion() override   {
        return ProjectInfo::versionString;
    }
    bool moreThanOneInstanceAllowed() override  { return true; }

    void initialise(const String&) override;

    void shutdown() override;

    void systemRequestedQuit() override;

    MainWindow* getMainWindow() const {
        return mainWindow_.get();
    }

    static CommandManager& getCommandManager();

    static MoToolApp& getApp();

private:
    te::Engine engine_;
    std::unique_ptr<MainWindow> mainWindow_;
    std::unique_ptr<MoLookAndFeel> lookAndFeel;
    CommandManager commandManager;
};

namespace Commands {

// // ApplicationCommandManager& getGlobalCommandManager();
// ApplicationCommandManager& getGlobalCommandManager() {
//     return MoToolApp::getCommandManager();
// }

} // namespace Commands

} // namespace MoTool
