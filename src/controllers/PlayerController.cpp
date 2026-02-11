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

static String makePlayerWindowTitle(const String& filename = {}) {
    auto appName = String::fromUTF8("μZX Player");
    if (filename.isEmpty())
        return appName;
    return filename + String::fromUTF8(" — ") + appName;
}

void PlayerController::initialize() {
    setMainWindowTitle(makePlayerWindowTitle());
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
    setMainWindowTitle(makePlayerWindowTitle(te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension()));
    window_.setSize(w, h);
    window_.repaint();
}

void PlayerController::handleOpen() {
    auto location = EditFileOps::getRecentEditsDirectory();
    FileChooser fc("Open file", location, "*.uzx;*.psg");
    if (fc.browseForFileToOpen()) {
        auto selectedFile = fc.getResult();
        if (selectedFile.existsAsFile()) {
            handleOpenFile(selectedFile);
        }
    }
}

void PlayerController::handleOpenFile(const File& file) {
    if (!file.existsAsFile()) return;

    auto ext = file.getFileExtension().toLowerCase();
    if (ext == ".psg") {
        importPsgToNewEdit(file);
    } else if (ext == ".uzx") {
        EditFileOps::setRecentEditsDirectory(file);
        auto newEdit = createOrLoadEdit(file);
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
        if (!f.existsAsFile()) return;

        auto options = MessageBoxOptions()
            .withTitle("Import PSG")
            .withMessage("How would you like to import \"" + f.getFileNameWithoutExtension() + "\"?")
            .withButton("New Track")
            .withButton("New Edit")
            .withButton("Cancel");

        NativeMessageBox::showAsync(options, [this, f](int result) {
            DBG("Import result: " << result);
            if (result == 0)
                importPsgToNewTrack(f);
            else if (result == 1)
                importPsgToNewEdit(f);
        });
    });
}

void PlayerController::importPsgToNewTrack(const File& f) {
    auto& transport = edit_->getTransport();
    transport.stop(false, false);

    auto track = edit_->insertNewAudioTrack(te::TrackInsertPoint(nullptr, te::getAudioTracks(*edit_).getLast()), nullptr);
    jassert(track != nullptr);

    auto psgFile = uZX::PsgFile(f);
    psgFile.ensureRead();
    te::ClipPosition pos = {{te::TimePosition(), te::TimeDuration::fromSeconds(psgFile.getData().getLengthSeconds())}, {}};

    if (auto inserted = PsgClip::insertTo(*track, psgFile, pos)) {
        track->changed();
        ensureAYPluginOnTrack(*track);
        track->setName(f.getFileNameWithoutExtension());
        zoomToFitHorizontally();
    }
}

void PlayerController::importPsgToNewEdit(const File& f) {
    edit_->getTransport().stop(false, false);

    // Create a new edit and populate it before setting it on the UI
    auto newEditFile = EditFileOps::getTempEditFile();
    auto newEdit = createOrLoadEdit(newEditFile);
    newEdit->ensureNumberOfAudioTracks(1);
    auto* track = te::getAudioTracks(*newEdit).getFirst();
    jassert(track != nullptr);

    auto psgFile = uZX::PsgFile(f);
    psgFile.ensureRead();
    te::ClipPosition pos = {{te::TimePosition(), te::TimeDuration::fromSeconds(psgFile.getData().getLengthSeconds())}, {}};

    if (PsgClip::insertTo(*track, psgFile, pos)) {
        track->changed();

        // Ensure AY plugin using the new edit's plugin cache
        bool hasAY = false;
        for (auto& p : track->pluginList) {
            if (dynamic_cast<uZX::AYChipPlugin*>(p)) {
                hasAY = true;
                break;
            }
        }
        if (!hasAY) {
            auto plugin = newEdit->getPluginCache().createNewPlugin(uZX::AYChipPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin(plugin, 0, nullptr);
        }

        track->setName(f.getFileNameWithoutExtension());
    }

    // Update the edit file name to match the imported PSG file
    auto namedFile = newEditFile.getSiblingFile(f.getFileNameWithoutExtension())
        .withFileExtension(EditFileOps::getDefaultEditFileSuffix());
    newEdit->editFileRetriever = [namedFile] { return namedFile; };

    // Set the fully-prepared edit (rebuilds UI with clip already present)
    setEdit(std::move(newEdit), true);
    setMainWindowTitle(makePlayerWindowTitle(f.getFileNameWithoutExtension()));
    zoomToFitHorizontally();
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
