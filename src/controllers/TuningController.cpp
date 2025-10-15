#include "TuningController.h"
#include "App.h"
#include "TuningCommands.h"
#include "../gui/tuning/TuningPreview.h"

namespace MoTool {

using namespace MoTool::Commands;

void TuningController::initialize() {
    BaseController::initialize();
    // window_.setSize(800, 600);
    //
    window_.setCloseHandler([this] {
        MoToolApp::getAppController().closeTuningWindow();
    });
}

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