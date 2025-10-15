#pragma once

#include <JuceHeader.h>

#include "MainController.h"
#include "../viewmodels/tuning/TuningViewModel.h"
#include "../viewmodels/tuning/TuningPlayer.h"

namespace MoTool {


class TuningController : public BaseController {
public:
    using BaseController::BaseController;

    ~TuningController() override = default;

    void initialize() override;

private:
    std::unique_ptr<te::Edit> createOrLoadStartupEdit() override;
    void setEdit(std::unique_ptr<te::Edit> edit, bool savePrev = false) override;
    void devicesChanged() override;

    std::unique_ptr<TuningViewModel> viewModel_;
    std::unique_ptr<TuningPlayer> tuningPlayer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningController)
};

}  // namespace MoTool