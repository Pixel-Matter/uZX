#pragma once

#include <JuceHeader.h>
#include "ChipInstrumentPlugin.h"
#include "ChipOscillatorControls.h"
#include "../../../gui/devices/PluginDeviceUI.h"

namespace MoTool::uZX {

namespace te = tracktion;

//==============================================================================
// ChipInstrumentUI
//==============================================================================
class ChipInstrumentUI : public PluginDeviceUI {
public:
    ChipInstrumentUI(tracktion::Plugin::Ptr pluginPtr);

    void paint(Graphics& g) override;
    void resized() override;

    ChipInstrumentPlugin* instrumentPlugin();

private:
    ChipInstrumentFx& instrument;

    ChipOscillatorControls oscillatorControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentUI)
};

}  // namespace MoTool::uZX
