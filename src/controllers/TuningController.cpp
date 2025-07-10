#include "TuningController.h"
#include "App.h"

namespace MoTool {

using namespace MoTool::Commands;

// ==============================================================================
// MenuBarModel
//==============================================================================
StringArray TuningController::getMenuBarNames() {
    return TuningsAppCommands::getMenuBarNames();
}

PopupMenu TuningController::getMenuForIndex(int /* menuIndex */, const String& menuName) {
    return TuningsAppCommands::createMenu(&commandManager_, menuName);
}

void TuningController::menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex*/ ) {
    // hadled by CommandManager
}

// ==============================================================================
// ApplicationCommandTarget
//==============================================================================

ApplicationCommandTarget* TuningController::getNextCommandTarget() {
    return nullptr;
}

void TuningController::getAllCommands(Array<CommandID>& commands) {
    commands.addArray(TuningsAppCommands::getCommandIDs());
}

void TuningController::getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) {
    // DBG("MainWindow::getCommandInfo: " << commandID);
    // Get main command info, name, desc, keypresses
    TuningsAppCommands::getCommandInfo(commandID, result);

    // Update command status based on current state
    switch (commandID) {
        case TuningsAppCommands::editUndo:
            result.setActive(edit_ != nullptr && edit_->getUndoManager().canUndo());
            break;

        case TuningsAppCommands::editRedo:
            result.setActive(edit_ != nullptr && edit_->getUndoManager().canRedo());
            break;

        default:
            break;
    }
}

bool TuningController::perform(const InvocationInfo& info) {
    // DBG("MainWindow::perform: " << info.commandID);
    switch (info.commandID) {
        case TuningsAppCommands::fileQuit:
            JUCEApplication::getInstance()->systemRequestedQuit();
            break;

        case TuningsAppCommands::editUndo:
            te::AppFunctions::undo();
            break;

        case TuningsAppCommands::editRedo:
            te::AppFunctions::redo();
            break;

        // Settings commands
        case TuningsAppCommands::settingsAudioMidi:
            te::AppFunctions::showSettingsScreen();
            break;

        case TuningsAppCommands::settingsPlugins:
            handlePluginManager();
            break;

        default:
            return false;
    }

    return true;
}

std::unique_ptr<te::Edit> TuningController::createOrLoadStartupEdit() {
    return createOrLoadEdit({});  // No startup edit for TuningController
}

void TuningController::setEdit(std::unique_ptr<te::Edit> edit, bool /*savePrev*/) {
    jassert(edit != nullptr);

    // if (savePrev && edit_ != nullptr) {
    //     te::EditFileOperations(*edit_).save(true, true, false);
    // }

    auto w = mainWindow_.getWidth(), h = mainWindow_.getHeight();
    mainWindow_.clearContentComponent();

    editViewState_.reset();
    edit_ = std::move(edit);
    // te::EditFileOperations(*edit_).save(true, true, false);

    viewModel_ = std::make_unique<TuningViewModel>(*edit_);
    tuningPlayer_ = std::make_unique<TuningPlayer>(*viewModel_);

    mainWindow_.setContentOwned(new TuningPreviewComponent(*viewModel_, *tuningPlayer_), true);
    mainWindow_.setName(MoToolApp::getApp().getApplicationFancyName());
    mainWindow_.setSize(w, h);
    mainWindow_.repaint();
}

void TuningController::createTracksAndAssignInputs() {
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