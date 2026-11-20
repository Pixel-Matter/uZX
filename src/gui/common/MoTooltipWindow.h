#pragma once
#include <JuceHeader.h>

namespace MoTool {

/** TooltipWindow that lets its parent show through the rounded corners drawTooltip() leaves unpainted. */
class MoTooltipWindow : public juce::TooltipWindow {
public:
    explicit MoTooltipWindow(juce::Component* parentComponent = nullptr, int millisecondsBeforeTipAppears = 700)
        : juce::TooltipWindow(parentComponent, millisecondsBeforeTipAppears)
    {
        setOpaque(false);
    }
};

}  // namespace MoTool
