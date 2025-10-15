#pragma once

#include <JuceHeader.h>
#include "../../controllers/TuningController.h"

namespace MoTool {

// class TuningWindow : public juce::DocumentWindow {
// public:
//     TuningWindow() : juce::DocumentWindow("Tuning Tool",
//                                           juce::Colours::lightgrey,
//                                           juce::DocumentWindow::allButtons)
//     {
//         setComponentID("tuning");
//         setUsingNativeTitleBar(true);
//         setResizable(true, true);
//         setSize(900, 600);
//         controller_ = std::make_unique<TuningController>();
//         controller_->initialize();
//         setContentNonOwned(controller_->getMainWindow(), true);
//         setVisible(true);
//     }

//     void closeButtonPressed() override {
//         setVisible(false);
//         delete this;
//     }

// private:
//     std::unique_ptr<TuningController> controller_;
// };

} // namespace MoTool
