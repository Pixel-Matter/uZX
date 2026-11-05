#pragma once

#include <JuceHeader.h>

#include "MainController.h"

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

private:
    std::unique_ptr<te::Edit> createOrLoadStartupEdit() override;
    void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false) override;
    void updateWindowTitle();

    void importPsgsToNewTracks(const Array<File>& files);
    void importPsgsToNewEdit(const Array<File>& files);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayerController)
};

}  // namespace MoTool
