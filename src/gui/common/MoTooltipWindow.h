#pragma once
#include <JuceHeader.h>

namespace MoTool {

class MoTooltipWindow : public juce::TooltipWindow {
public:
    explicit MoTooltipWindow(juce::Component* parentComponent = nullptr,
                            int millisecondsBeforeTipAppears = 700)
        : juce::TooltipWindow(parentComponent, millisecondsBeforeTipAppears)
    {
        // Default behavior - can be customized later
    }

    // // Override getTipFor for custom tooltip text logic if needed
    // juce::String getTipFor(juce::Component& component) override {
    //     auto result = juce::TooltipWindow::getTipFor(component);
    //     // DBG("Tooltip is " << result);
    //     return result;
    // }

    // Additional customization methods can be added here
    // For example:
    // - Custom timing behavior
    // - Custom positioning logic
    // - Animation support
    // - Multi-line tooltip support
    // - Rich text formatting
};

}  // namespace MoTool