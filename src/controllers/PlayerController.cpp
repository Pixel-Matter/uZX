#include "PlayerController.h"
#include "App.h"
#include "EditState.h"

#include "../gui/main/PlayerDocument.h"
#include "../gui/common/Utilities.h"
#include "../models/PsgClip.h"
#include "../models/EditUtilities.h"
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "../util/FileOps.h"
#include "../util/Helpers.h"

using namespace MoTool::Helpers;

namespace MoTool {

void PlayerController::initialize() {
    auto title = String::fromUTF8("Pixel Matter μZX Player v") + MoToolApp::getApp().getApplicationVersion();
    setMainWindowTitle(title);
    window_.setComponentID("player");

    #if !JUCE_MAC
    window_.setMenuBar(&MoToolApp::getAppController());
    #endif

    BaseController::initialize();
}

PlayerController::~PlayerController() {
    #if !JUCE_MAC
    window_.setMenuBar(nullptr);
    #endif

    window_.clearContentComponent();
}

std::unique_ptr<te::Edit> PlayerController::createOrLoadStartupEdit() {
    return createOrLoadEdit(EditFileOps::getStartupEditFile());
}

EditViewState* PlayerController::getEditViewState() {
    return editViewState_.get();
}

void PlayerController::setEdit(std::unique_ptr<te::Edit> edit, bool savePrev) {
    jassert(edit != nullptr);

    if (savePrev && edit_ != nullptr) {
        EditFileOps::saveEdit(*edit_, true, true, false);
    }

    auto w = window_.getWidth(), h = window_.getHeight();

    window_.clearContentComponent();
    MoToolApp::getSelectionManager().deselectAll();
    editViewState_.reset();

    if (edit_ != nullptr) {
        edit_->getTempDirectory(false).deleteRecursively();
    }

    edit_ = std::move(edit);

    rescaleAllMidiClipsToFit(*edit_);

    edit_->playInStopEnabled = true;
    setEditTimecodeFormat(*edit_, TimecodeTypeExt::barsBeatsFps50);

    edit_->getTransport().ensureContextAllocated();
    edit_->ensureNumberOfAudioTracks(1);
    EditFileOps::saveEdit(*edit_, true, true, false);

    editViewState_ = std::make_unique<EditViewState>(*edit_, MoToolApp::getSelectionManager());
    editViewState_->showHeaders = false;

    window_.setContentOwned(new PlayerDocumentComponent(*edit_, *editViewState_), true);
    setMainWindowTitle(te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension());
    window_.setSize(w, h);
    window_.repaint();
}

void PlayerController::handleOpen() {
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

void PlayerController::handleOpenRecent(int fileIndex) {
    auto recentFiles = EditFileOps::getRecentEdits();
    if (fileIndex >= 0 && fileIndex < recentFiles.size()) {
        File selectedFile(recentFiles[fileIndex]);
        if (selectedFile.existsAsFile()) {
            auto newEdit = createOrLoadEdit(selectedFile);
            setEdit(std::move(newEdit), true);
        } else {
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                "File Not Found",
                "The file '" + selectedFile.getFileName() + "' could not be found.",
                "OK");
            MoToolApp::getCommandManager().commandStatusChanged();
        }
    }
}

void PlayerController::handleClearRecentFiles() {
    auto& storage = dynamic_cast<PropertyStorage&>(engine_.getPropertyStorage());
    storage.setCustomProperty("recentEdits", "");
    MoToolApp::getCommandManager().commandStatusChanged();
}

void PlayerController::handleImportPsgReplace() {
    if (edit_ == nullptr) return;

    Helpers::browseForPSGFile(engine_, [this](const File& f) {
        auto& transport = edit_->getTransport();
        transport.stop(false, false);

        // Remove all clips from all tracks
        for (auto track : te::getClipTracks(*edit_)) {
            auto clips = track->getClips();
            for (int i = clips.size(); --i >= 0;) {
                clips[i]->removeFromParent();
            }
        }

        // Remove all audio tracks except first
        auto audioTracks = te::getAudioTracks(*edit_);
        for (int i = audioTracks.size() - 1; i > 0; --i) {
            edit_->deleteTrack(audioTracks[i]);
        }

        // Ensure we have at least one track
        edit_->ensureNumberOfAudioTracks(1);
        auto* track = te::getAudioTracks(*edit_).getFirst();
        jassert(track != nullptr);

        // Import PSG file
        auto psgFile = uZX::PsgFile(f);
        psgFile.ensureRead();
        te::ClipPosition pos = {{te::TimePosition(), te::TimeDuration::fromSeconds(psgFile.getData().getLengthSeconds())}, {}};

        if (auto inserted = PsgClip::insertTo(*track, psgFile, pos)) {
            track->changed();
            ensureAYPluginOnTrack(*track);
            track->setName(f.getFileNameWithoutExtension());

            // Zoom to fit
            zoomToFitHorizontally();
        }
    });
}

void PlayerController::ensureAYPluginOnTrack(te::AudioTrack& track) {
    for (auto& p : track.pluginList) {
        if (dynamic_cast<uZX::AYChipPlugin*>(p))
            return;
    }
    auto plugin = edit_->getPluginCache().createNewPlugin(uZX::AYChipPlugin::xmlTypeName, {});
    track.pluginList.insertPlugin(plugin, 0, nullptr);
}

void PlayerController::zoomHorizontal(float increment) {
    if (auto* viewState = getEditViewState()) {
        viewState->zoom.zoomHorizontally(increment);
    }
}

void PlayerController::zoomToFitHorizontally() {
    auto viewState = getEditViewState();
    auto range = Helpers::getEffectiveClipsTimeRange(*getEdit());
    if (viewState != nullptr && !range.isEmpty()) {
        viewState->zoom.setRange(range);
    }
}

}  // namespace MoTool
