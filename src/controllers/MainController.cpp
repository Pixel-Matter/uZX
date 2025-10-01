#include <JuceHeader.h>

#include "MainController.h"
#include "EditState.h"
#include "MainCommands.h"
#include "UIBehavior.h"

#include "../gui/main/MainWindow.h"
#include "../gui/main/MainDocument.h"

#include "../models/Behavior.h"
#include "../models/EditUtilities.h"
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "../plugins/uZX/instrument/ChipInstrumentPlugin.h"
#include "../plugins/uZX/notes_to_psg/NotesToPsgPlugin.h"

#include "../util/FileOps.h"
#include "../util/Helpers.h"

#include <common/Utilities.h>  // from Tracktion

using namespace MoTool::Commands;
using namespace MoTool::Helpers;

namespace MoTool {

void BaseController::setMainWindowTitle(const String& title) {
    mainWindow_.setTitle(title);
    mainWindow_.setName(title);
}

BaseController::BaseController()
    : engine_ {
        std::make_unique<PropertyStorage>(CharPointer_UTF8(ProjectInfo::projectName)),
        std::make_unique<ExtUIBehaviour>(),
        std::make_unique<ExtEngineBehaviour>()
    }
{
    // Workaround for JUCE CoreAudio buffer overflow bug at low sample rates
    ensureMinimumSampleRate();
}

void BaseController::initialize() {
    engine_.getPluginManager().createBuiltInType<uZX::AYChipPlugin>();
    engine_.getPluginManager().createBuiltInType<uZX::ChipInstrumentPlugin>();
    engine_.getPluginManager().createBuiltInType<uZX::NotesToPsgPlugin>();

    engine_.getDeviceManager().addChangeListener(this);

    setEdit(createOrLoadStartupEdit());
    selectionManager_.addChangeListener(this);

    commandManager_.registerAllCommandsForTarget(this);
    commandManager_.setFirstCommandTarget(this);
    // Install key mappings
    commandManager_.getKeyMappings()->resetToDefaultMappings();
    mainWindow_.addKeyListener(commandManager_.getKeyMappings());

    // menuBarModel::
    setApplicationCommandManagerToWatch(&commandManager_);

    #if JUCE_MAC
    setMacMainMenu(this);
    #else
    setMenuBar(this);
    #endif
}

BaseController::~BaseController() {
    engine_.getDeviceManager().removeChangeListener(this);

    // commandManager_.setFirstCommandTarget(nullptr);
    selectionManager_.deselectAll();
    selectionManager_.removeChangeListener(this);

    commandManager_.setFirstCommandTarget(nullptr);
    setApplicationCommandManagerToWatch(nullptr);

    #if JUCE_MAC
        setMacMainMenu(nullptr);
    #else
        setMenuBar(nullptr);
    #endif
    mainWindow_.removeKeyListener(commandManager_.getKeyMappings());
    mainWindow_.clearContentComponent();

    if (edit_ != nullptr) {
        EditFileOps::saveEdit(*edit_, true, true, false);
        edit_->getTempDirectory(false).deleteRecursively();
    }

    engine_.getTemporaryFileManager().getTempDirectory().deleteRecursively();
}


void BaseController::ensureMinimumSampleRate() {
    constexpr double MIN_SAMPLE_RATE = 44100.0;

    auto& deviceManager = engine_.getDeviceManager().deviceManager;
    auto* currentDevice = deviceManager.getCurrentAudioDevice();

    bool audioOk = false;

    if (currentDevice != nullptr) {
        auto currentSampleRate = currentDevice->getCurrentSampleRate();
        if (currentSampleRate < MIN_SAMPLE_RATE) {
            DBG("WARNING: Sample rate " << currentSampleRate << " Hz is below minimum " << MIN_SAMPLE_RATE << " Hz");
            deviceManager.closeAudioDevice();
        } else {
            audioOk = true;
        }
    }
    if (!audioOk) {
        MessageManager::callAsync([] {
            te::AppFunctions::showSettingsScreen();
        });
    }
}

te::Edit* BaseController::getEdit() {
    return edit_.get();
}

te::Engine& BaseController::getEngine() {
    return engine_;
}

EditViewState* BaseController::getEditViewState() {
    return editViewState_.get();
}

te::SelectionManager& BaseController::getSelectionManager() {
    return selectionManager_;
}

ApplicationCommandManager& BaseController::getCommandManager() {
    return commandManager_;
}

void BaseController::devicesChanged() {
    // if (!edit_) return;

    // edit_->getTransport().ensureContextAllocated();

    // Do not need this because we can setup this in UI per track
    // if (auto defaultMidiDevice = engine_.getDeviceManager().getDefaultMidiInDevice()) {
    //     // Find the input device instance for the default MIDI device
    //     te::InputDeviceInstance* defaultInstance = nullptr;
    //     for (auto instance : edit_->getAllInputDevices()) {
    //         if (&instance->getInputDevice() == defaultMidiDevice) {
    //             defaultInstance = instance;
    //             break;
    //         }
    //     }

    //     for (auto at : te::getTracksOfType<te::AudioTrack>(*edit_, true)) {
    //         if (at) {
    //             [[maybe_unused]] auto res = defaultInstance->setTarget(at->itemID, false, &edit_->getUndoManager(), 0);
    //         }
    //     }
    // }
    // edit_->restartPlayback();
}

void BaseController::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &selectionManager_) {
        // Selection changed, update command status
        commandManager_.commandStatusChanged();
    } else if (source == &engine_.getDeviceManager()) {
        devicesChanged();
    }
}

void BaseController::handlePluginManager() {
    DialogWindow::LaunchOptions o;
    o.dialogTitle                   = TRANS("Plugins");
    o.dialogBackgroundColour        = Colors::Theme::background;
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = true;
    o.useBottomRightCornerResizer   = true;

    auto v = new PluginListComponent (engine_.getPluginManager().pluginFormatManager,
                                      engine_.getPluginManager().knownPluginList,
                                      engine_.getTemporaryFileManager().getTempFile("PluginScanDeadMansPedal"),
                                      std::addressof(engine_.getPropertyStorage().getPropertiesFile()));
    v->setSize(800, 600);
    o.content.setOwned(v);
    o.launchAsync();
}

std::unique_ptr<te::Edit> BaseController::createOrLoadEdit(File editFile) {
    std::unique_ptr<te::Edit> edit;
    if (editFile.existsAsFile())
        edit = te::loadEditFromFile(engine_, editFile);
    else
        edit = te::createEmptyEdit(engine_, editFile);
    return edit;
}

// ================================= MainController =============================================
MainController::~MainController() {
    // TODO actually we should clean up the render files when closing/destroying an edit, not when closing the app
    // We can subclass the edit and override destructor to do this
    if (edit_ != nullptr) {
        Helpers::removeUnusedRenderFiles(*edit_);
    }
}

// ==============================================================================
// MenuBarModel
//==============================================================================
StringArray MainController::getMenuBarNames() {
    return MainAppCommands::getMenuBarNames();
}

PopupMenu MainController::getMenuForIndex(int /* menuIndex */, const String& menuName) {
    if (menuName == "File") {
        PopupMenu menu;
        menu.addCommandItem(&commandManager_, MainAppCommands::fileNew);
        menu.addCommandItem(&commandManager_, MainAppCommands::fileOpen);

        // Recent files submenu - populate dynamically
        auto recentFiles = EditFileOps::getRecentEdits();
        if (recentFiles.size() > 0) {
            PopupMenu recentMenu;

            for (int i = 0; i < recentFiles.size() && i < 8; ++i) {
                File recentFile(recentFiles[i]);
                recentMenu.addCommandItem(&commandManager_, MainAppCommands::fileOpenRecent1 + i);
            }

            recentMenu.addSeparator();
            recentMenu.addCommandItem(&commandManager_, MainAppCommands::fileClearRecentFiles);
            menu.addSubMenu("Open Recent", recentMenu);
        }

        menu.addSeparator();
        menu.addCommandItem(&commandManager_, MainAppCommands::fileSave);
        menu.addCommandItem(&commandManager_, MainAppCommands::fileSaveAs);
        menu.addCommandItem(&commandManager_, MainAppCommands::fileReveal);
        menu.addSeparator();
        menu.addCommandItem(&commandManager_, MainAppCommands::fileImportPsg);
        menu.addSeparator();
        menu.addCommandItem(&commandManager_, MainAppCommands::fileQuit);

        return menu;
    }

    return MainAppCommands::createMenu(&commandManager_, menuName);
}

void MainController::menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex*/ ) {
    // handled by CommandManager
}

ApplicationCommandTarget* MainController::getNextCommandTarget() {
    return nullptr;
}

void MainController::getAllCommands(Array<CommandID>& commands) {
    commands.addArray(MainAppCommands::getCommandIDs());
}

void MainController::getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) {
    // DBG("MainWindow::getCommandInfo: " << commandID);
    // Get main command info, name, desc, keypresses
    MainAppCommands::getCommandInfo(commandID, result);

    // Update command status based on current state
    switch (commandID) {
        case MainAppCommands::fileSave:
            result.setActive(edit_ != nullptr);
            break;

        case MainAppCommands::fileSaveAs:
            result.setActive(edit_ != nullptr);
            break;

        case MainAppCommands::fileReveal:
            result.setActive(edit_ != nullptr);
            break;

        case MainAppCommands::fileOpenRecent1:
        case MainAppCommands::fileOpenRecent2:
        case MainAppCommands::fileOpenRecent3:
        case MainAppCommands::fileOpenRecent4:
        case MainAppCommands::fileOpenRecent5:
        case MainAppCommands::fileOpenRecent6:
        case MainAppCommands::fileOpenRecent7:
        case MainAppCommands::fileOpenRecent8: {
            auto recentFiles = EditFileOps::getRecentEdits();
            int fileIndex = commandID - MainAppCommands::fileOpenRecent1;
            if (fileIndex < recentFiles.size()) {
                File recentFile(recentFiles[fileIndex]);
                result.setInfo(recentFile.getFileNameWithoutExtension(),
                             "Open " + recentFile.getFullPathName(), "File", 0);
                result.setActive(true);
            } else {
                result.setActive(false);
            }
            break;
        }

        case MainAppCommands::fileClearRecentFiles: {
            auto recentFiles = EditFileOps::getRecentEdits();
            result.setActive(recentFiles.size() > 0);
            break;
        }

        case MainAppCommands::editUndo:
            result.setActive(edit_ != nullptr && edit_->getUndoManager().canUndo());
            break;

        case MainAppCommands::editRedo:
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

        case MainAppCommands::transportPlay:
            result.setActive(edit_ != nullptr);
            break;

        case MainAppCommands::transportRecord:
            result.setActive(edit_ != nullptr && !edit_->getTransport().isRecording());
            break;

        case MainAppCommands::transportRecordStop:
            result.setActive(edit_ != nullptr && edit_->getTransport().isRecording());
            break;

        case MainAppCommands::transportToStart:
            result.setActive(edit_ != nullptr);
            break;

        case MainAppCommands::transportLoop:
            result.setTicked(edit_ != nullptr && edit_->getTransport().looping);
            break;

        case MainAppCommands::viewZoomToSelection:
            result.setActive(edit_ != nullptr && selectionManager_.getSelectedObjects().size() > 0);
            break;

        // Add more dynamic command states...
    }
}

bool MainController::perform(const InvocationInfo& info) {
    // DBG("MainWindow::perform: " << info.commandID);
    switch (info.commandID) {
        case MainAppCommands::fileNew:
            handleNew();
            break;

        case MainAppCommands::fileOpen:
            handleOpen();
            break;

        case MainAppCommands::fileOpenRecent1:
        case MainAppCommands::fileOpenRecent2:
        case MainAppCommands::fileOpenRecent3:
        case MainAppCommands::fileOpenRecent4:
        case MainAppCommands::fileOpenRecent5:
        case MainAppCommands::fileOpenRecent6:
        case MainAppCommands::fileOpenRecent7:
        case MainAppCommands::fileOpenRecent8:
            handleOpenRecent(info.commandID - MainAppCommands::fileOpenRecent1);
            break;

        case MainAppCommands::fileClearRecentFiles:
            handleClearRecentFiles();
            break;

        case MainAppCommands::fileSave:
            te::AppFunctions::saveEdit();
            break;

        case MainAppCommands::fileSaveAs:
            handleSaveAs();
            break;

        case MainAppCommands::fileReveal:
            if (edit_ != nullptr) {
                EditFileOps::saveEdit(*edit_, false, true, false);
                te::EditFileOperations(*edit_).getEditFile().revealToUser();
            }
            break;

        case MainAppCommands::fileImportPsg:
            if (edit_ != nullptr) {
                importPsgAsClip(*edit_, selectionManager_);
            }
            break;

        case MainAppCommands::fileQuit:
            JUCEApplication::getInstance()->systemRequestedQuit();
            break;

        case MainAppCommands::editUndo:
            te::AppFunctions::undo();
            break;

        case MainAppCommands::editRedo:
            te::AppFunctions::redo();
            break;

        case MainAppCommands::editDelete:
            te::AppFunctions::deleteSelected();
            break;
        // Transport commands
        case MainAppCommands::transportPlay:
            te::AppFunctions::startStopPlay();
            break;

        case MainAppCommands::transportRecord:
            handleRecord();
            break;

        case MainAppCommands::transportRecordStop:
            handleRecord();
            break;

        case MainAppCommands::transportToStart:
            te::AppFunctions::goToStart();
            break;

        case MainAppCommands::transportToEnd:
            te::AppFunctions::goToEnd();
            break;

        case MainAppCommands::transportLoop:
            te::AppFunctions::toggleLoop();
            break;

        // Add commands
        case MainAppCommands::addAudioTrack:
            if (edit_ != nullptr) {
                Helpers::addAndSelectAudioTrack(*edit_, selectionManager_);
            }
            break;

        // case AppCommands::addAutomationTrack:
        //     if (edit_ != nullptr) {
        //         Helpers::addAndSelectAutomationTrack(*edit_, selectionManager_);
        //     }
        //     break;

        // Track commands
        case MainAppCommands::trackRenderToAudio:
            if (edit_ != nullptr) {
                Helpers::renderSelectedTracksToAudioTrack(*edit_, selectionManager_);
            }
            break;

        // View commands
        case MainAppCommands::viewZoomToProject:
            // TODO maybe invert this, put implementation here
            // and te::AppFunctions::zoomToFitHorizontally() invokes this command
            // rationale: what is zoom and how to zoom must be defined by the current controller
            te::AppFunctions::zoomToFitHorizontally();
            break;

        case MainAppCommands::viewZoomToSelection:
            te::AppFunctions::zoomToSelection();
            break;

        case MainAppCommands::viewZoomIn:
            te::AppFunctions::zoomIn();
            break;

        case MainAppCommands::viewZoomOut:
            te::AppFunctions::zoomOut();
            break;

        // Settings commands
        case MainAppCommands::settingsAudioMidi:
            te::AppFunctions::showSettingsScreen();
            break;

        case MainAppCommands::settingsPlugins:
            handlePluginManager();
            break;

        default:
            return false;
    }

    return true;
}

void MainController::handleNew() {
    auto newEdit = createOrLoadEdit(EditFileOps::getTempEditFile());
    setEdit(std::move(newEdit), true);
}

void MainController::handleOpen() {
    auto location = EditFileOps::getRecentEditsDirectory();
    FileChooser fc("Open file", location, EditFileOps::getAppFileGlob());
    if (fc.browseForFileToOpen()) {
        auto selectedFile = fc.getResult();
        if (selectedFile.existsAsFile()) {
            EditFileOps::setRecentEditsDirectory(selectedFile);
        }
        auto newEdit = createOrLoadEdit(selectedFile);
        setEdit(std::move(newEdit), true);
    }
}

void MainController::handleOpenRecent(int fileIndex) {
    auto recentFiles = EditFileOps::getRecentEdits();
    if (fileIndex >= 0 && fileIndex < recentFiles.size()) {
        File selectedFile(recentFiles[fileIndex]);
        if (selectedFile.existsAsFile()) {
            auto newEdit = createOrLoadEdit(selectedFile);
            setEdit(std::move(newEdit), true);
        } else {
            // File no longer exists, remove from recent files
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                "File Not Found",
                "The file '" + selectedFile.getFileName() + "' could not be found.",
                "OK");
            // The file will be automatically removed next time getRecentEdits() is called
            commandManager_.commandStatusChanged(); // Refresh menu
        }
    }
}

void MainController::handleClearRecentFiles() {
    auto& storage = dynamic_cast<PropertyStorage&>(engine_.getPropertyStorage());
    storage.setCustomProperty("recentEdits", "");
    commandManager_.commandStatusChanged(); // Refresh menu to show empty state
}

void MainController::handleSaveAs() {
    if (edit_ == nullptr) return;

    if (EditFileOps::saveEditAsWithDialog(*edit_)) {
        // TODO use listener or callback to update title when edit file changes
        auto name = te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension();
        setMainWindowTitle(name);
    }
}

void MainController::handleRecord() {
    if (edit_ == nullptr) return;

    bool wasRecording = edit_->getTransport().isRecording();
    te::AppFunctions::record();
    if (wasRecording) {
        te::EditFileOperations(*edit_).save(true, true, false);
    }
}

// void handleInsertAudioClip() {
//     Helpers::browseForAudioFile(edit.engine, [this](const File& f) {
//         if (f.existsAsFile()) {
//             auto track = getSelectedOrInsertAudioTrack(edit, selectionManager);
//             te::AudioFile audioFile(edit.engine, f);
//             if (audioFile.isValid()) {
//                 if (auto inserted = track->insertWaveClip(
//                     f.getFileNameWithoutExtension(),
//                     f,
//                     {{{}, te::TimeDuration::fromSeconds(audioFile.getLength())}, {}},
//                     false)
//                 ) {
//                     // DBG("Inserted clip: " << inserted->getName());
//                 }
//             }
//         }
//     });
// }

// void handleInsertMidiClip() {
//     auto seq = Helpers::readMidi(MIDI_CLIP_DATA, 1);
//     auto len = seq.getEndTime();
//     double insertTime = edit.getTransport().getPosition().inSeconds();
//     seq.addTimeToMessages(insertTime);
//     auto time = te::TimeRange(te::TimePosition::fromSeconds(insertTime), te::TimeDuration::fromSeconds(len));
//     auto track = getSelectedOrInsertAudioTrack(edit, selectionManager);

//     if (auto clip = track->insertMIDIClip("MidiClip", time, &selectionManager)) {
//         clip->mergeInMidiSequence(seq, te::MidiList::NoteAutomationType::none);
//         clip->setMidiChannel(te::MidiChannel(1));
//     }
// }

std::unique_ptr<te::Edit> MainController::createOrLoadStartupEdit() {
    return createOrLoadEdit(EditFileOps::getStartupEditFile());
}

void MainController::setEdit(std::unique_ptr<te::Edit> edit, bool savePrev) {
    jassert(edit != nullptr);

    if (savePrev && edit_ != nullptr) {
        EditFileOps::saveEdit(*edit_, true, true, false);
    }

    auto w = mainWindow_.getWidth(), h = mainWindow_.getHeight();

    // Clear UI components first to release any references to the old edit
    mainWindow_.clearContentComponent();

    // Clear selection manager references to old edit objects
    selectionManager_.deselectAll();

    // Reset view state before replacing edit
    editViewState_.reset();

    // Clean up old edit temp directory if it exists
    if (edit_ != nullptr) {
        edit_->getTempDirectory(false).deleteRecursively();
    }

    // Replace the edit
    edit_ = std::move(edit);

    rescaleAllMidiClipsToFit(*edit_);

    edit_->playInStopEnabled = true;
    setEditTimecodeFormat(*edit_, TimecodeTypeExt::barsBeatsFps50);

    createTracksAndAssignInputs();
    EditFileOps::saveEdit(*edit_, true, true, false);

    editViewState_ = std::make_unique<EditViewState>(*edit_, getSelectionManager());

    mainWindow_.setContentOwned(new MainDocumentComponent(*edit_, *editViewState_), true);
    setMainWindowTitle(te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension());
    mainWindow_.setSize(w, h);
    mainWindow_.repaint();
}

void MainController::createTracksAndAssignInputs() {
    edit_->getTransport().ensureContextAllocated();
    edit_->ensureNumberOfAudioTracks(1);
}

}  // namespace MoTool