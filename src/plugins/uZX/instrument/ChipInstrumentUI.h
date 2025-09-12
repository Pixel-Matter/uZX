#pragma once

#include <JuceHeader.h>

#include "../../../gui/devices/PluginDeviceUI.h"

namespace MoTool::uZX {

class ChipInstrumentUI : public PluginDeviceUI {
public:
    ChipInstrumentUI(tracktion::Plugin::Ptr pluginPtr);

    void paint(Graphics& g) override;
    void resized() override;

    static constexpr int itemHeight = 20;
    static constexpr int itemSpacing = 4;

private:
    Slider adsrAttackSlider, adsrDecaySlider, adsrSustainSlider, adsrReleaseSlider, adsrVelocitySlider;
    Label adsrAttackLabel, adsrDecayLabel, adsrSustainLabel, adsrReleaseLabel, adsrVelocityLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentUI)
};

}  // namespace MoTool::uZX
