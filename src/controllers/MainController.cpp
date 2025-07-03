#include <JuceHeader.h>

#include "MainController.h"
#include "EditState.h"
#include "Commands.h"
#include "UIBehavior.h"

#include "../gui/main/MainWindow.h"
#include "../gui/main/MainDocument.h"
#include "../gui/tuning/TuningPreview.h"

#include "../models/Behavior.h"
#include "../models/EditUtilities.h"
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "../plugins/uZX/MidiToPsgPlugin.h"

#include "../util/FileOps.h"

#include <common/Utilities.h>  // from Tracktion

using namespace MoTool::Commands;
using namespace MoTool::Helpers;

namespace MoTool {

MainController::MainController()
    : engine_ {
        CharPointer_UTF8(ProjectInfo::projectName),
        std::make_unique<ExtUIBehaviour>(),
        std::make_unique<ExtEngineBehaviour>()
    }
{
    // DBG("Engine properties storage is " << engine_.getPropertyStorage().getPropertiesFile().getFile().getFullPathName());
    engine_.getPluginManager().createBuiltInType<uZX::AYChipPlugin>();
    engine_.getPluginManager().createBuiltInType<uZX::MidiToPsgPlugin>();

    setEdit(createOrLoadEdit(EditFileOps::getStartupEditFile()));
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

MainController::~MainController() {
    if (edit_ != nullptr) {
        te::EditFileOperations(*edit_).save(true, true, false);
        edit_->getTempDirectory(false).deleteRecursively();
        Helpers::removeUnusedRenderFiles(*edit_);
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

void MainController::setMainWindowTitle(const String& title) {
    mainWindow_.setTitle(title);
}

te::Edit* MainController::getEdit() {
    return edit_.get();
}

te::Engine& MainController::getEngine() {
    return engine_;
}

EditViewState* MainController::getEditViewState() {
    return editViewState_.get();
}

te::SelectionManager& MainController::getSelectionManager() {
    return selectionManager_;
}

ApplicationCommandManager& MainController::getCommandManager() {
    return commandManager_;
}

// ==============================================================================
// MenuBarModel
//==============================================================================
StringArray MainController::getMenuBarNames() {
    return AppCommands::getMenuBarNames();
}

PopupMenu MainController::getMenuForIndex(int /* menuIndex */, const String& menuName) {
    return AppCommands::createMenu(&commandManager_, menuName);
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
    commands.addArray(AppCommands::getCommandIDs());
}

void MainController::getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) {
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

        case AppCommands::viewZoomToSelection:
            result.setActive(edit_ != nullptr && selectionManager_.getSelectedObjects().size() > 0);
            break;

        // Add more dynamic command states...
    }
}

bool MainController::perform(const InvocationInfo& info) {
    // DBG("MainWindow::perform: " << info.commandID);
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

        case AppCommands::fileImportPsg:
            if (edit_ != nullptr) {
                importPsgAsClip(*edit_, selectionManager_);
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
        // Transport commands
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
            // assuming autoscroll is enabled
            // editViewState_->zoom.scrollToCurrentPosition();
            break;

        case AppCommands::transportToEnd:
            te::AppFunctions::goToEnd();
            // assuming autoscroll is enabled
            // editViewState_->zoom.scrollToCurrentPosition();
            break;

        case AppCommands::transportLoop:
            te::AppFunctions::toggleLoop();
            break;

        // Add commands
        case AppCommands::addAudioTrack:
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
        case AppCommands::trackRenderToAudio:
            if (edit_ != nullptr) {
                Helpers::renderSelectedTracksToAudioTrack(*edit_, selectionManager_);
            }
            break;

        // View commands
        case AppCommands::viewZoomToProject:
            te::AppFunctions::zoomToFitHorizontally();
            break;

        case AppCommands::viewZoomToSelection:
            te::AppFunctions::zoomToSelection();
            break;

        case AppCommands::viewZoomIn:
            te::AppFunctions::zoomIn();
            break;

        case AppCommands::viewZoomOut:
            te::AppFunctions::zoomOut();
            break;

        // Settings commands
        case AppCommands::settingsAudioMidi:
            te::AppFunctions::showSettingsScreen();
            break;

        case AppCommands::settingsPlugins:
            hanldePluginManager();
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

void MainController::hanldePluginManager() {
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

std::unique_ptr<te::Edit> MainController::createOrLoadEdit(File editFile) {
    std::unique_ptr<te::Edit> edit;
    if (editFile.existsAsFile())
        edit = te::loadEditFromFile(engine_, editFile);
    else
        edit = te::createEmptyEdit(engine_, editFile);

    edit->playInStopEnabled = true;
    return edit;
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

    editViewState_ = std::make_unique<EditViewState>(*edit_, getSelectionManager());

    if (true /* start at Tuning mode */) {
        mainWindow_.setContentOwned(new TuningPreviewComponent(&edit_->getUndoManager()), true);
        mainWindow_.setName("Tuning System View");
    } else {
        // mainWindow_.setContentOwned(new MainDocumentComponent(*edit_, *editViewState_), true);
        // mainWindow_.setName(te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension());
    }

    mainWindow_.setSize(w, h);
    mainWindow_.repaint();
}

// TODO refactor to EditController
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


void MainController::changeListenerCallback (ChangeBroadcaster*) {
    // Called when the selection changes
    // DBG("Selection changed");
    commandManager_.commandStatusChanged();
}


}  // namespace MoTool