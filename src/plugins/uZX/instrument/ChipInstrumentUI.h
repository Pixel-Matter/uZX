#pragma once

#include <JuceHeader.h>
#include "ChipInstrumentPlugin.h"
#include "../../../controllers/ParamAttachments.h"
#include "../../../gui/devices/PluginDeviceUI.h"
#include "../../../gui/common/LabeledRotarySlider.h"

namespace MoTool::uZX {

namespace te = tracktion;


// TODO Oscillator UI component with modules for waveform, volume, coarse, finetune, pan...
// TODO ADSR component with graphics display

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

    MoTool::LabeledRotarySlider adsrAttackSlider, adsrDecaySlider, adsrSustainSlider, adsrReleaseSlider;
    // MoTool::LabeledRotarySlider adsrVelocitySlider;
    MoTool::LabeledRotarySlider pitchAttackSlider, pitchDecaySlider, pitchSustainSlider, pitchReleaseSlider, pitchDepthSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentUI)
};

}  // namespace MoTool::uZX
