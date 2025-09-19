#pragma once

#include <JuceHeader.h>
#include "ChipInstrumentPlugin.h"
#include "../../../controllers/ParamAttachments.h"
#include "../../../gui/devices/PluginDeviceUI.h"

namespace MoTool::uZX {

namespace te = tracktion;

class LabeledRotarySlider : public Component {
public:
    LabeledRotarySlider(te::AutomatableParameter::Ptr param, const String& labelText = {}, const String& tooltip = {}, const String& valueSuffix = {});

    LabeledRotarySlider(te::AutomatableParameter::Ptr param, const ValueWithSource<float>& value);

    LabeledRotarySlider(const ValueWithSource<float>& value);

    void resized() override;
    void paint(Graphics& g) override;

    Slider& getSlider() { return slider; }
    Label& getLabel() { return label; }

    inline static constexpr int labelHeight = 10;
    inline static constexpr int labelOverlap = 2;

    int getLabelHeight() const { return labelHeight - labelOverlap; }

private:
    void showParameterMenu();
    void learnMidiCC();
    void clearMidiMapping();
    void mapMidiCC(int controllerID, int channel = 1);
    bool isParameterMapped() const;
    String getMappingDescription() const;

    // Helper functions for MIDI CC menu creation
    static String formatCCName(int ccNumber);
    static String formatControllerDescription(int controllerID, int channel);
    static PopupMenu createCCSubmenu(int startCC, int endCC);
    static PopupMenu createSpecialControllersSubmenu();
    static PopupMenu createMidiMappingSubmenu();

    class SliderMouseListener : public MouseListener {
    public:
        SliderMouseListener(LabeledRotarySlider& ownerRef) : owner(ownerRef) {}
        void mouseDown(const MouseEvent& e) override {
            if (e.mods.isRightButtonDown()) {
                owner.showParameterMenu();
            }
        }
    private:
        LabeledRotarySlider& owner;
    };

    Slider slider;
    Label label;
    SliderAttachment attachment;
    te::AutomatableParameter::Ptr parameter;
    SliderMouseListener mouseListener;

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

    LabeledRotarySlider adsrAttackSlider, adsrDecaySlider, adsrSustainSlider, adsrReleaseSlider;
    // LabeledRotarySlider adsrVelocitySlider;
    LabeledRotarySlider pitchAttackSlider, pitchDecaySlider, pitchSustainSlider, pitchReleaseSlider, pitchDepthSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentUI)
};

}  // namespace MoTool::uZX
