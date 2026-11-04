#include "PlayerController.h"
#include "App.h"
#include "EditState.h"

#include "../gui/main/PlayerDocument.h"
#include "../gui/common/Utilities.h"
#include "../models/PsgClip.h"
#include "../models/EditUtilities.h"
#include "../formats/psg/TurboSound.h"
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
    // The Player never records or routes hardware MIDI; skip the periodic device rescan.
    getEngine().getDeviceManager().setMidiDeviceScanIntervalSeconds(0);

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

void PlayerController::setEdit(std::unique_ptr<te::Edit> edit, bool savePrev) {
    if (edit == nullptr)  // unreadable file, already reported: keep the current edit
        return;

    if (savePrev && edit_ != nullptr) {
        EditFileOps::saveEdit(*edit_, true, true, false);
    }

    auto w = window_.getWidth(), h = window_.getHeight();

    window_.clearContentComponent();
    MoToolApp::getSelectionManager().deselectAll();
    editViewState_.reset();
    playWholeEdit_.reset();

    if (edit_ != nullptr) {
        edit_->getTempDirectory(false).deleteRecursively();
    }

    edit_ = std::move(edit);

    rescaleAllMidiClipsToFit(*edit_);

    edit_->playInStopEnabled = true;
    migrateTimecodeDisplaySettings(*edit_);  // as in Studio: a new edit shows bars | beats | frames
    useGlobalMasterVolume();

    edit_->getTransport().ensureContextAllocated();
    edit_->ensureNumberOfAudioTracks(1);
    playWholeEdit_ = std::make_unique<PlayWholeEdit>(*edit_);
    EditFileOps::saveEdit(*edit_, true, true, false);

    editViewState_ = std::make_unique<EditViewState>(*edit_, MoToolApp::getSelectionManager());
    editViewState_->showHeaders = false;
    editViewState_->showFooters = true;

    window_.setContentOwned(new PlayerDocumentComponent(*edit_, *editViewState_), true);
    setMainWindowTitle(makePlayerWindowTitle(te::EditFileOperations(*edit_).getEditFile().getFileNameWithoutExtension()));
    window_.setSize(w, h);
    window_.repaint();
}

void PlayerController::handleOpen() {
    auto location = EditFileOps::getRecentEditsDirectory();
    FileChooser fc("Open file", location, "*.uzx;*.psg");
    if (fc.browseForMultipleFilesToOpen())
        handleOpenFiles(fc.getResults());
}

void PlayerController::handleOpenFiles(const Array<File>& files) {
    Array<File> psgs;
    for (const auto& f : files)
        if (f.hasFileExtension("psg") && f.existsAsFile())
            psgs.add(f);

    if (! psgs.isEmpty()) {
        Helpers::withTurboSoundSet(psgs, [this](const Array<File>& set) { importPsgsToNewEdit(set); });
        return;
    }

    for (const auto& f : files) {
        if (f.hasFileExtension("uzx") && f.existsAsFile()) {
            EditFileOps::setRecentEditsDirectory(f);
            setEdit(createOrLoadEdit(f), true);
            return;
        }
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

    Helpers::browseForPSGFiles(engine_, [this](const Array<File>& picked) {
        Helpers::withTurboSoundSet(picked, [this](const Array<File>& files) {
            if (files.isEmpty()) return;

            const auto what = files.size() == 1 ? "\"" + files[0].getFileNameWithoutExtension() + "\""
                                                : String(files.size()) + " files";
            auto options = MessageBoxOptions()
                .withTitle("Import PSG")
                .withMessage("How would you like to import " + what + "?")
                .withButton(files.size() == 1 ? "New Track" : "New Tracks")
                .withButton("New Edit")
                .withButton("Cancel");

            NativeMessageBox::showAsync(options, [this, files](int result) {
                if (result == 0)
                    importPsgsToNewTracks(files);
                else if (result == 1)
                    importPsgsToNewEdit(files);
            });
        });
    });
}

void PlayerController::importPsgsToNewTracks(const Array<File>& files) {
    edit_->getTransport().stop(false, false);
    if (! Helpers::importPsgFiles(*edit_, files).isEmpty())
        zoomToFitHorizontally();
}

void PlayerController::importPsgsToNewEdit(const Array<File>& files) {
    // Populate a new edit before setting it on the UI; a fresh edit has one empty track for the first file
    auto newEditFile = EditFileOps::getTempEditFile();
    auto newEdit = createOrLoadEdit(newEditFile);
    const auto clips = Helpers::importPsgFiles(*newEdit, files, {}, te::getAudioTracks(*newEdit).getFirst());
    if (clips.isEmpty())
        return;

    // A PSG stores no clock. A fitted tuning names the machine the tune was written
    // for, so the opened tune plays at that clock; a tracker table leaves it as it is.
    for (auto* clip : clips)
        if (const auto& fit = clip->getTuning().fit)
            Helpers::ensureAYPluginOnTrack(*clip->getAudioTrack())
                .staticParams.chipClock.setStoredValue(fit->chipClock / uZX::MHz);

    edit_->getTransport().stop(false, false);

    // Name the edit file after the imported PSG files
    const auto name = uZX::getSetName(files);
    auto namedFile = newEditFile.getSiblingFile(name).withFileExtension(EditFileOps::getDefaultEditFileSuffix());
    newEdit->editFileRetriever = [namedFile] { return namedFile; };

    setEdit(std::move(newEdit), true);
    setMainWindowTitle(makePlayerWindowTitle(name));
    zoomToFitHorizontally();
}

}  // namespace MoTool
