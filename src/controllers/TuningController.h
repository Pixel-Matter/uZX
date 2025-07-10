#pragma once

#include <JuceHeader.h>

#include "MainController.h"
#include "TuningCommands.h"
#include "../viewmodels/tuning/TuningViewModel.h"
#include "../viewmodels/tuning/TuningPlayer.h"
#include "../gui/tuning/TuningPreview.h"

namespace MoTool {


class TuningController : public BaseController {
public:
    using BaseController::BaseController;

    ~TuningController() override = default;

    // ==============================================================================
    // MenuBarModel
    //==============================================================================
    StringArray getMenuBarNames() override;
    PopupMenu getMenuForIndex(int menuIndex, const String& menuName) override;
    void menuItemSelected(int /* menuItemID */, int /* topLevelMenuIndex*/ ) override;

    // ==============================================================================
    // ApplicationCommandTarget
    //==============================================================================
    ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands(Array<CommandID>& commands) override;
    void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override;
    bool perform(const InvocationInfo& info) override;

private:
    std::unique_ptr<te::Edit> createOrLoadStartupEdit() override;
    void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false) override;

    std::unique_ptr<TuningViewModel> viewModel_;
    std::unique_ptr<TuningPlayer> tuningPlayer_;
};

}  // namespace MoTool