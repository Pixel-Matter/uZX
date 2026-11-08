#pragma once

#include <JuceHeader.h>

#include "MainController.h"
#include "RememberedStereo.h"

namespace MoTool {

class PlayerController : public BaseController {
public:
    using BaseController::BaseController;

    ~PlayerController() override;

    void initialize() override;

    void handleOpen();
    /** Opens the PSG files together in a new edit (offering a single PSG's Turbo Sound set),
        or else the first .uzx project. */
    void handleOpenFiles(const Array<File>& files);
    void handleOpenRecent(int fileIndex);
    void handleClearRecentFiles();
    /** Saves in place; an edit that only lives in the temp folder goes through Save As. */
    void handleSave();
    void handleSaveAs();
    void handleImportPsgReplace();

    /** Viz mode: full screen with only the notes and each chip's scopes. An app state, not saved:
        it stays on for the next file opened. */
    void setVizMode(bool on);
    bool isVizMode() const noexcept { return vizMode_; }

private:
    std::unique_ptr<te::Edit> createOrLoadStartupEdit() override;
    void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false) override;
    void updateWindowTitle();

    void importPsgsToNewTracks(const Array<File>& files);
    void importPsgsToNewEdit(const Array<File>& files);

    std::unique_ptr<RememberedStereo> rememberedStereo_;
    bool vizMode_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayerController)
};

}  // namespace MoTool
