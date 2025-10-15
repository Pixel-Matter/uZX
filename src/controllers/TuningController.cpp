#include "TuningController.h"
#include "App.h"
#include "TuningCommands.h"
#include "../gui/tuning/TuningPreview.h"

namespace MoTool {

using namespace MoTool::Commands;

// // ==============================================================================
// // MenuBarModel
// //==============================================================================
// StringArray TuningController::getMenuBarNames() {
//     return TuningsAppCommands::getMenuBarNames();
// }

// PopupMenu TuningController::getMenuForIndex(int /* menuIndex */, const String& menuName) {
//     return TuningsAppCommands::createMenu(&commandManager_, menuName);
// }

// void TuningController::menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex*/ ) {
//     // hadled by CommandManager
// }

// // ==============================================================================
// // ApplicationCommandTarget
// //==============================================================================

// ApplicationCommandTarget* TuningController::getNextCommandTarget() {
//     return nullptr;
// }

// void TuningController::getAllCommands(Array<CommandID>& commands) {
//     commands.addArray(TuningsAppCommands::getCommandIDs());
// }

// void TuningController::getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) {
//     // DBG("MainWindow::getCommandInfo: " << commandID);
//     // Get main command info, name, desc, keypresses
//     TuningsAppCommands::getCommandInfo(commandID, result);

//     // Update command status based on current state
//     switch (commandID) {
//         case TuningsAppCommands::editUndo:
//             result.setActive(edit_ != nullptr && edit_->getUndoManager().canUndo());
//             break;

//         case TuningsAppCommands::editRedo:
//             result.setActive(edit_ != nullptr && edit_->getUndoManager().canRedo());
//             break;

//         default:
//             break;
//     }
// }

// bool TuningController::perform(const InvocationInfo& info) {
//     // DBG("MainWindow::perform: " << info.commandID);
//     switch (info.commandID) {
//         case TuningsAppCommands::fileQuit:
//             JUCEApplication::getInstance()->systemRequestedQuit();
//             break;

//         case TuningsAppCommands::editUndo:
//             te::AppFunctions::undo();
//             break;

//         case TuningsAppCommands::editRedo:
//             te::AppFunctions::redo();
//             break;

//         // Settings commands
//         case TuningsAppCommands::settingsAudioMidi:
//             te::AppFunctions::showSettingsScreen();
//             break;

//         case TuningsAppCommands::settingsPlugins:
//             handlePluginManager();
//             break;

//         default:
//             return false;
//     }

//     return true;
// }

std::unique_ptr<te::Edit> TuningController::createOrLoadStartupEdit() {
    return createOrLoadEdit({});  // No startup edit for TuningController
}

void TuningController::setEdit(std::unique_ptr<te::Edit> edit, bool /*savePrev*/) {
    jassert(edit != nullptr);

    // if (savePrev && edit_ != nullptr) {
    //     te::EditFileOperations(*edit_).save(true, true, false);
    // }

    auto w = window_.getWidth(), h = window_.getHeight();
    window_.clearContentComponent();
    edit_ = std::move(edit);
    edit_->playInStopEnabled = true;  // Disable play/stop for tuning edits
    edit_->getTransport().ensureContextAllocated();
    // te::EditFileOperations(*edit_).save(true, true, false);

    viewModel_ = std::make_unique<TuningViewModel>(*edit_);
    tuningPlayer_ = std::make_unique<TuningPlayer>(*viewModel_);

    window_.setContentOwned(new TuningPreviewComponent(*viewModel_, *tuningPlayer_), true);
    window_.setName("Tuning");
    window_.setSize(w, h);
    window_.repaint();
}

void TuningController::devicesChanged() {
    if (!edit_) return;
    // DBG("TuningController::devicesChanged");

    edit_->getTransport().ensureContextAllocated();

    if (auto defaultMidiDevice = engine_.getDeviceManager().getDefaultMidiInDevice()) {
        // Find the input device instance for the default MIDI device
        te::InputDeviceInstance* defaultInstance = nullptr;
        for (auto instance : edit_->getAllInputDevices()) {
            if (&instance->getInputDevice() == defaultMidiDevice) {
                defaultInstance = instance;
                break;
            }
        }

        for (auto at : te::getTracksOfType<te::AudioTrack>(*edit_, true)) {
            if (at) {
                [[maybe_unused]] auto res = defaultInstance->setTarget(at->itemID, false, &edit_->getUndoManager(), 0);
            }
        }
    }
    edit_->restartPlayback();
}

}  // namespace MoTool