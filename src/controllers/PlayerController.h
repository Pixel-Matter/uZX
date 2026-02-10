#pragma once

#include <JuceHeader.h>

#include "MainController.h"

namespace MoTool {

class PlayerController : public BaseController {
public:
    using BaseController::BaseController;

    ~PlayerController() override;

    void initialize() override;
    EditViewState* getEditViewState();

    void zoomHorizontal(float increment) override;
    void zoomToFitHorizontally() override;

    void handleOpen();
    void handleOpenRecent(int fileIndex);
    void handleClearRecentFiles();
    void handleImportPsgReplace();

private:
    std::unique_ptr<te::Edit> createOrLoadStartupEdit() override;
    void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false) override;

    void ensureAYPluginOnTrack(te::AudioTrack& track);

    std::unique_ptr<EditViewState> editViewState_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayerController)
};

}  // namespace MoTool
