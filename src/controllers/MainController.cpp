#include <JuceHeader.h>
#include <memory>

#include "MainController.h"
#include "EditState.h"
#include "MainCommands.h"
#include "UIBehavior.h"

#include "../gui/main/MainWindow.h"
#include "../gui/tuning/TuningWindow.h"
#include "../gui/main/MainDocument.h"
#include "../gui/main/AboutDialog.h"

#include "../models/Behavior.h"
#include "../models/EditUtilities.h"
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "../plugins/uZX/instrument/ChipInstrumentPlugin.h"
#include "../plugins/uZX/notes_to_psg/NotesToPsgPlugin.h"

#include "../util/FileOps.h"
#include "../util/Helpers.h"
#include "controllers/App.h"
#include "controllers/TuningController.h"

#include <common/Utilities.h>  // from Tracktion

using namespace MoTool::Commands;
using namespace MoTool::Helpers;

namespace MoTool {


AppController::AppController()
    : engine_ {
        std::make_unique<PropertyStorage>(CharPointer_UTF8(ProjectInfo::projectName)),
        std::make_unique<ExtUIBehaviour>(),
        std::make_unique<ExtEngineBehaviour>()
    }
{
    // engine_.getDeviceManager().initialise(engine_.getEngineBehaviour().shouldOpenAudioInputByDefault()
    //     ? te::DeviceManager::defaultNumChannelsToOpen : 0, te::DeviceManager::defaultNumChannelsToOpen);

    // Workaround for JUCE CoreAudio buffer overflow bug at low sample rates
    ensureMinimumSampleRate();
}

void AppController::initialize() {
    engine_.getPluginManager().createBuiltInType<uZX::AYChipPlugin>();
    engine_.getPluginManager().createBuiltInType<uZX::ChipInstrumentPlugin>();
    engine_.getPluginManager().createBuiltInType<uZX::NotesToPsgPlugin>();

    engine_.getDeviceManager().addChangeListener(this);

    selectionManager_.addChangeListener(this);

    commandManager_.registerAllCommandsForTarget(this);
    commandManager_.setFirstCommandTarget(this);
    // Install key mappings
    commandManager_.getKeyMappings()->resetToDefaultMappings();

    // menuBarModel::
    setApplicationCommandManagerToWatch(&commandManager_);

    #if JUCE_MAC
    setMacMainMenu(this);
    #else
    setMenuBar(this);
    #endif
}

AppController::~AppController() {
    engine_.getDeviceManager().removeChangeListener(this);
    engine_.getTemporaryFileManager().getTempDirectory().deleteRecursively();

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
}

te::SelectionManager& AppController::getSelectionManager() {
    return selectionManager_;
}

ApplicationCommandManager& AppController::getCommandManager() {
    return commandManager_;
}

void AppController::ensureMinimumSampleRate() {
    constexpr double MIN_SAMPLE_RATE = 44100.0;
    constexpr int MAX_BLOCK_SIZE = 256;

    auto& deviceManager = engine_.getDeviceManager().deviceManager;
    auto* currentDevice = deviceManager.getCurrentAudioDevice();

    bool audioOk = false;

    if (currentDevice != nullptr) {
        auto currentSampleRate = currentDevice->getCurrentSampleRate();
        if (currentSampleRate < MIN_SAMPLE_RATE) {
            const auto currentBufferSize = currentDevice->getCurrentBufferSizeSamples();
            DBG("WARNING: Sample rate " << currentSampleRate << " Hz is below minimum " << MIN_SAMPLE_RATE << " Hz"
                << ", buffer size: " << currentBufferSize);
            auto desiredBlockSize = juce::jmin(currentBufferSize, MAX_BLOCK_SIZE);

            auto availableSizes = currentDevice->getAvailableBufferSizes();
            if (!availableSizes.isEmpty()) {
                int candidateSize = availableSizes.getLast();
                for (auto size : availableSizes) {
                    if (size >= MAX_BLOCK_SIZE) {
                        candidateSize = size;
                        break;
                    }
                }
                desiredBlockSize = juce::jmin(currentBufferSize, candidateSize);
            }

            if (currentBufferSize > desiredBlockSize) {
                juce::AudioDeviceManager::AudioDeviceSetup setup;
                deviceManager.getAudioDeviceSetup(setup);
                setup.bufferSize = desiredBlockSize;
                DBG("  Current buffer size: " << currentBufferSize << ", setting to " << desiredBlockSize);
                if (auto error = deviceManager.setAudioDeviceSetup(setup, false); error.isNotEmpty())
                    DBG("Failed to set audio block size: " << error);
            }

            deviceManager.restartLastAudioDevice();
            const auto newBufferSize = currentDevice->getCurrentBufferSizeSamples();
            DBG("  New buffer size: " << newBufferSize);
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

void AppController::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &selectionManager_) {
        // Selection changed, update command status
        commandManager_.commandStatusChanged();
    }
}

void AppController::handlePluginManager() {
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

void AppController::showAboutDialog() {
    DialogWindow::LaunchOptions options;
    options.dialogTitle                  = "About " + JUCEApplication::getInstance()->getApplicationName();
    options.dialogBackgroundColour       = Colors::Theme::backgroundAlt;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = false;
    options.useBottomRightCornerResizer  = false;
    // TODO getCurrentFocusedDesktopWindow
    // options.componentToCentreAround      = &mainWindow_;
    options.content.setOwned(new AboutDialogComponent());
    options.runModal();
}


// ==============================================================================
// MenuBarModel
//==============================================================================
StringArray AppController::getMenuBarNames() {
    return MainAppCommands::getMenuBarNames();
}

PopupMenu AppController::getMenuForIndex(int /* menuIndex */, const String& menuName) {
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

void AppController::menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex*/ ) {
    // handled by CommandManager
}

ApplicationCommandTarget* AppController::getNextCommandTarget() {
    return nullptr;
}

void AppController::getAllCommands(Array<CommandID>& commands) {
    commands.addArray(MainAppCommands::getCommandIDs());
}

void AppController::getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) {
    // DBG("MainWindow::getCommandInfo: " << commandID);
    // Get main command info, name, desc, keypresses
    MainAppCommands::getCommandInfo(commandID, result);

    auto& arrangerController = MoToolApp::getArrangerController();
    auto* edit = arrangerController.getEdit();

    // Update command status based on current state
    switch (commandID) {
        case MainAppCommands::fileSave:
            result.setActive(edit != nullptr);
            break;

        case MainAppCommands::fileSaveAs:
            result.setActive(edit != nullptr);
            break;

        case MainAppCommands::fileReveal:
            result.setActive(edit != nullptr);
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
            result.setActive(edit != nullptr && edit->getUndoManager().canUndo());
            break;

        case MainAppCommands::editRedo:
            result.setActive(edit != nullptr && edit->getUndoManager().canRedo());
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
            result.setActive(edit != nullptr);
            break;

        case MainAppCommands::transportRecord:
            result.setActive(edit != nullptr && !edit->getTransport().isRecording());
            break;

        case MainAppCommands::transportRecordStop:
            result.setActive(edit != nullptr && edit->getTransport().isRecording());
            break;

        case MainAppCommands::transportToStart:
            result.setActive(edit != nullptr);
            break;

        case MainAppCommands::transportLoop:
            result.setTicked(edit != nullptr && edit->getTransport().looping);
            break;

        case MainAppCommands::viewZoomToSelection:
            result.setActive(edit != nullptr && selectionManager_.getSelectedObjects().size() > 0);
            break;

        // Add more dynamic command states...
    }
}

bool AppController::perform(const InvocationInfo& info) {
    // DBG("MainWindow::perform: " << info.commandID);
    auto& arrangerController = MoToolApp::getArrangerController();
    auto* edit = arrangerController.getEdit();

    switch (info.commandID) {
        case MainAppCommands::fileNew:
            arrangerController.handleNew();
            break;

        case MainAppCommands::fileOpen:
            arrangerController.handleOpen();
            break;

        case MainAppCommands::fileOpenRecent1:
        case MainAppCommands::fileOpenRecent2:
        case MainAppCommands::fileOpenRecent3:
        case MainAppCommands::fileOpenRecent4:
        case MainAppCommands::fileOpenRecent5:
        case MainAppCommands::fileOpenRecent6:
        case MainAppCommands::fileOpenRecent7:
        case MainAppCommands::fileOpenRecent8:
            arrangerController.handleOpenRecent(info.commandID - MainAppCommands::fileOpenRecent1);
            break;

        case MainAppCommands::fileClearRecentFiles:
            arrangerController.handleClearRecentFiles();
            break;

        case MainAppCommands::fileSave:
            te::AppFunctions::saveEdit();
            break;

        case MainAppCommands::fileSaveAs:
            arrangerController.handleSaveAs();
            break;

        case MainAppCommands::fileReveal:
            if (edit != nullptr) {
                EditFileOps::saveEdit(*edit, false, true, false);
                te::EditFileOperations(*edit).getEditFile().revealToUser();
            }
            break;

        case MainAppCommands::fileImportPsg:
            if (edit != nullptr) {
                importPsgAsClip(*edit, selectionManager_);
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
            arrangerController.handleRecord();
            break;

        case MainAppCommands::transportRecordStop:
            arrangerController.handleRecord();
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
            if (edit != nullptr) {
                Helpers::addAndSelectAudioTrack(*edit, selectionManager_);
            }
            break;

        // case AppCommands::addAutomationTrack:
        //     if (edit_ != nullptr) {
        //         Helpers::addAndSelectAutomationTrack(*edit_, selectionManager_);
        //     }
        //     break;

        // Track commands
        case MainAppCommands::trackRenderToAudio:
            if (edit != nullptr) {
                Helpers::renderSelectedTracksToAudioTrack(*edit, selectionManager_);
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

        case MainAppCommands::settingsTuningTables: {
            // Open the Tuning Tool in a new window
            openTuningWindow();
            break;
        }

        case MainAppCommands::helpAbout:
            showAboutDialog();
            break;

        default:
            return false;
    }

    return true;
}

void AppController::openTuningWindow() {
    if (tuningController_ != nullptr) {
        tuningController_->bringWindowToFront();
    } else {
        tuningController_ = std::make_unique<TuningController>(getEngine());
        tuningController_->initialize();
    }
}

te::Engine& AppController::getEngine() {
    return engine_;
}

//============================================================================
BaseController::BaseController(te::Engine& e)
    : engine_ {e}
{}

BaseController::~BaseController() {
    window_.removeKeyListener(MoToolApp::getCommandManager().getKeyMappings());
    window_.clearContentComponent();

    if (edit_ != nullptr) {
        EditFileOps::saveEdit(*edit_, true, true, false);
        edit_->getTempDirectory(false).deleteRecursively();
    }

    engine_.getTemporaryFileManager().getTempDirectory().deleteRecursively();
}

void BaseController::initialize() {
    engine_.getDeviceManager().addChangeListener(this);

    setEdit(createOrLoadStartupEdit());

    // TODO
    window_.addKeyListener(MoToolApp::getCommandManager().getKeyMappings());
}

void BaseController::bringWindowToFront() {
    window_.toFront(true);
    window_.setVisible(true);
}

void BaseController::changeListenerCallback(ChangeBroadcaster* source) {
    if (source == &engine_.getDeviceManager()) {
        devicesChanged();
    }
}

void BaseController::setMainWindowTitle(const String& title) {
    window_.setTitle(title);
    window_.setName(title);
}

te::Edit* BaseController::getEdit() {
    return edit_.get();
}

te::Engine& BaseController::getEngine() {
    return engine_;
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
ArrangerController::~ArrangerController() {
    // TODO actually we should clean up the render files when closing/destroying an edit, not when closing the app
    // We can subclass the edit and override destructor to do this
    if (edit_ != nullptr) {
        Helpers::removeUnusedRenderFiles(*edit_);
    }
}

void ArrangerController::devicesChanged() {
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

void ArrangerController::handleNew() {
    auto newEdit = createOrLoadEdit(EditFileOps::getTempEditFile());
    setEdit(std::move(newEdit), true);
}

void ArrangerController::handleOpen() {
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

void ArrangerController::handleOpenRecent(int fileIndex) {
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
            MoToolApp::getCommandManager().commandStatusChanged(); // Refresh menu
        }
    }
}

void ArrangerController::handleClearRecentFiles() {
    auto& storage = dynamic_cast<PropertyStorage&>(engine_.getPropertyStorage());
    storage.setCustomProperty("recentEdits", "");
    MoToolApp::getCommandManager().commandStatusChanged(); // Refresh menu to show empty state
}

void ArrangerController::handleSaveAs() {
    if (edit_ == nullptr) return;

    if (EditFileOps::saveEditAsWithDialog(*edit_)) {
        // TODO use listener or callback to update title when edit file changes
        auto name = te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension();
        setMainWindowTitle(name);
    }
}

void ArrangerController::handleRecord() {
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

std::unique_ptr<te::Edit> ArrangerController::createOrLoadStartupEdit() {
    return createOrLoadEdit(EditFileOps::getStartupEditFile());
}


EditViewState* ArrangerController::getEditViewState() {
    return editViewState_.get();
}

void ArrangerController::setEdit(std::unique_ptr<te::Edit> edit, bool savePrev) {
    jassert(edit != nullptr);

    if (savePrev && edit_ != nullptr) {
        EditFileOps::saveEdit(*edit_, true, true, false);
    }

    auto w = window_.getWidth(), h = window_.getHeight();

    // Clear UI components first to release any references to the old edit
    window_.clearContentComponent();

    // Clear selection manager references to old edit objects
    MoToolApp::getSelectionManager().deselectAll();

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

    editViewState_ = std::make_unique<EditViewState>(*edit_, MoToolApp::getSelectionManager());

    window_.setContentOwned(new MainDocumentComponent(*edit_, *editViewState_), true);
    setMainWindowTitle(te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension());
    window_.setSize(w, h);
    window_.repaint();
}

void ArrangerController::createTracksAndAssignInputs() {
    edit_->getTransport().ensureContextAllocated();
    edit_->ensureNumberOfAudioTracks(1);
}

void ArrangerController::zoomToSelection() {
    auto objects = MoToolApp::getSelectionManager().getSelectedObjects();
    objects = te::getClipSelectionWithCollectionClipContents(objects);
    auto range = te::getTimeRangeForSelectedItems(objects);
    auto viewState = getEditViewState();
    if (viewState != nullptr && !range.isEmpty()) {
        viewState->zoom.setRange(range);
    }
}

void ArrangerController::zoomHorizontal(float increment) {
    if (auto* viewState = getEditViewState()) {
        viewState->zoom.zoomHorizontally(increment);
    }
}

void ArrangerController::zoomToFitHorizontally() {
    auto viewState = getEditViewState();
    auto range = Helpers::getEffectiveClipsTimeRange(*getEdit());
    if (viewState != nullptr && !range.isEmpty()) {
        viewState->zoom.setRange(range);
    }
}

}  // namespace MoTool
