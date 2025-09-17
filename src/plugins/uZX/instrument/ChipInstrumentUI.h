#pragma once

#include <JuceHeader.h>
#include "ChipInstrumentPlugin.h"
#include "../../../controllers/ParamAttachments.h"
#include "../../../gui/devices/PluginDeviceUI.h"

namespace MoTool::uZX {

class LabeledRotarySlider : public Component {
public:
    LabeledRotarySlider(te::AutomatableParameter::Ptr parameter, const String& labelText = {}, const String& tooltip = {}, const String& valueSuffix = {});

    LabeledRotarySlider(te::AutomatableParameter::Ptr parameter, const ValueWithSource<float>& value);

    LabeledRotarySlider(const ValueWithSource<float>& value);

    void resized() override;

    Slider& getSlider() { return slider; }
    Label& getLabel() { return label; }

    inline static constexpr int labelHeight = 10;
    inline static constexpr int labelOverlap = 2;

    int getLabelHeight() const { return labelHeight - labelOverlap; }

private:
    Slider slider;
    Label label;
    SliderAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabeledRotarySlider)
};


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

    LabeledRotarySlider adsrAttackSlider, adsrDecaySlider, adsrSustainSlider, adsrReleaseSlider, adsrVelocitySlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentUI)
};

}  // namespace MoTool::uZX
