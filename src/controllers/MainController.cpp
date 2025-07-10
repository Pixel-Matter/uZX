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
#include "../plugins/uZX/MidiToPsgPlugin.h"

#include "../util/FileOps.h"

#include <common/Utilities.h>  // from Tracktion

using namespace MoTool::Commands;
using namespace MoTool::Helpers;

namespace MoTool {

void BaseController::setMainWindowTitle(const String& title) {
    mainWindow_.setTitle(title);
}

BaseController::BaseController()
    : engine_ {
        CharPointer_UTF8(ProjectInfo::projectName),
        std::make_unique<ExtUIBehaviour>(),
        std::make_unique<ExtEngineBehaviour>()
    }
{}

void BaseController::initialize() {
    // DBG("Engine properties storage is " << engine_.getPropertyStorage().getPropertiesFile().getFile().getFullPathName());
    engine_.getPluginManager().createBuiltInType<uZX::AYChipPlugin>();
    engine_.getPluginManager().createBuiltInType<uZX::MidiToPsgPlugin>();

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
    if (edit_ != nullptr) {
        te::EditFileOperations(*edit_).save(true, true, false);
        edit_->getTempDirectory(false).deleteRecursively();
    }
    // commandManager_.setFirstCommandTarget(nullptr);
    selectionManager_.deselectAll();
    selectionManager_.removeChangeListener(this);

    commandManager_.setFirstCommandTarget(nullptr);
    setApplicationCommandManagerToWatch(nullptr);

    mainWindow_.removeKeyListener(commandManager_.getKeyMappings());
    #if JUCE_MAC
        setMacMainMenu(nullptr);
    #else
        setMenuBar(nullptr);
    #endif
    mainWindow_.clearContentComponent();

    engine_.getTemporaryFileManager().getTempDirectory().deleteRecursively();
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

void BaseController::changeListenerCallback (ChangeBroadcaster*) {
    // Called when the selection changes
    // DBG("Selection changed");
    commandManager_.commandStatusChanged();
}

void BaseController::handlePluginManager() {
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
    return MainAppCommands::createMenu(&commandManager_, menuName);
}

void MainController::menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex*/ ) {
    // hadled by CommandManager
}

ApplicationCommandTarget* MainController::getNextCommandTarget() {
    // DBG("MainWindow::getNextCommandTarget");
    // find in children
    // for (auto* c : getChildren()) {
    //     if (auto* main = dynamic_cast<MainDocumentComponent*>(c)) {
    //         DBG("MainWindow::getNextCommandTarget: MainDocumentComponent found");
    //         for (auto* d : main->getChildren()) {
    //             if (auto* target = dynamic_cast<ApplicationCommandTarget*>(d)) {
    //                 DBG("MainWindow::getNextCommandTarget: found");
    //                 return target;
    //             }
    //         }
    //     } else {
    //         DBG("MainWindow::getNextCommandTarget: MainDocumentComponent not found");
    //     }
    // }
    // DBG("MainWindow::getNextCommandTarget: not found");
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

        case MainAppCommands::fileSave:
            te::AppFunctions::saveEdit();
            break;

        case MainAppCommands::fileSaveAs:
            handleSaveAs();
            break;

        case MainAppCommands::fileReveal:
            if (edit_ != nullptr) {
                te::EditFileOperations(*edit_).save(false, true, false);
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
            // assuming autoscroll is enabled
            // editViewState_->zoom.scrollToCurrentPosition();
            break;

        case MainAppCommands::transportToEnd:
            te::AppFunctions::goToEnd();
            // assuming autoscroll is enabled
            // editViewState_->zoom.scrollToCurrentPosition();
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
    FileChooser fc("Open file", File::getSpecialLocation(File::userDocumentsDirectory), EditFileOps::getAppFileGlob());
    if (fc.browseForFileToOpen()) {
        auto newEdit = createOrLoadEdit(fc.getResult());
        setEdit(std::move(newEdit), true);
    }
}

void MainController::handleSaveAs() {
    if (edit_ == nullptr) return;

    auto efo = te::EditFileOperations(*edit_);
    auto newEditName = te::getNonExistentSiblingWithIncrementedNumberSuffix(efo.getEditFile(), false);
    juce::FileChooser fc("Save As...", newEditName, EditFileOps::getAppFileGlob());

    if (fc.browseForFileToSave(false)) {
        efo.saveAs(fc.getResult().withFileExtension(EditFileOps::EDIT_FILE_SUFFIX));
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
        te::EditFileOperations(*edit_).save(true, true, false);
    }

    auto w = mainWindow_.getWidth(), h = mainWindow_.getHeight();
    mainWindow_.clearContentComponent();

    editViewState_.reset();
    edit_ = std::move(edit);
    // FIXME implement BPM editing with clips remapping
    // 8 * 13f = 104f — one bar
    // one beat - 104f / 4 = 26f = 1s / 50f * 26f = 0.52s
    // beats per minute = 60 * 50 / 26 = 115.3846153846 bpm
    // need to remap clips to new tempo
    edit_->tempoSequence.getTempoAt(edit_->getTransport().getPosition()).setBpm(115.3846153846);
    setEditTimecodeFormat(*edit_, TimecodeTypeExt::barsBeatsFps50);
    createTracksAndAssignInputs();
    te::EditFileOperations(*edit_).save(true, true, false);

    edit_->playInStopEnabled = true;

    editViewState_ = std::make_unique<EditViewState>(*edit_, getSelectionManager());

    mainWindow_.setContentOwned(new MainDocumentComponent(*edit_, *editViewState_), true);
    mainWindow_.setName(te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension());
    mainWindow_.setSize(w, h);
    mainWindow_.repaint();
}

void MainController::createTracksAndAssignInputs() {
    for (auto& midiIn : engine_.getDeviceManager().getMidiInDevices()) {
        midiIn->setMonitorMode(te::InputDevice::MonitorMode::automatic);
        midiIn->setEnabled(true);
    }

    edit_->getTransport().ensureContextAllocated();
    if (te::getAudioTracks(*edit_).size() == 0) {
        int trackNum = 0;

        for (auto instance : edit_->getAllInputDevices()) {
            if (instance->getInputDevice().getDeviceType() == te::InputDevice::physicalMidiDevice) {
                if (auto t = EngineHelpers::getOrInsertAudioTrackAt(*edit_, trackNum)) {
                    [[ maybe_unused ]] auto res = instance->setTarget(t->itemID, true, &edit_->getUndoManager(), 0);
                    instance->setRecordingEnabled(t->itemID, true);
                    trackNum++;
                }
            }
        }
    }
    edit_->restartPlayback();
}



}  // namespace MoTool