#pragma once

#include <JuceHeader.h>

#include "AYPlugin.h"
#include "../../../gui/common/ChoiceButton.h"
#include "../../../gui/common/ToggleButton.h"
#include "../../../gui/common/ComboBindingWithPresets.h"
#include "../../../gui/common/ComboBoxWithOverrideId.h"
#include "../../../gui/common/LabeledSlider.h"
#include "../../../gui/devices/PluginDeviceUI.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"


namespace MoTool::uZX {

//==============================================================================
// Editor for AYChipPlugin
//==============================================================================
class AYPluginUI : public PluginDeviceUI {
public:
    AYPluginUI(tracktion::Plugin::Ptr pluginPtr);

    void paint(Graphics& g) override;

    void resized() override;

    ComponentBoundsConstrainer* getBoundsConstrainer();

    bool hasDeviceMenu() const override;
    void populateDeviceMenu(juce::PopupMenu& menu) override;

    static constexpr int itemHeight = 20;
    static constexpr int itemSpacing = 4;

private:
    AYChipPlugin& plugin_;
    ComponentBoundsConstrainer constrainer_;

    // static, cannot be automated
    ChoiceButton chipTypeButton   { plugin_, plugin_.staticParams.chipType };
    ComboBoxWithOverrideId chipClockCombo;
    ComboBindingWithPresets chipClockBinding;
    // TextButton DCButton        { plugin_, plugin_.staticParams.removeDC };

    // dynamic, can be automated
    LabeledSlider volumeKnob   { plugin_, plugin_.dynamicParams.volume };
    ChoiceButton layoutButton  { plugin_, plugin_.dynamicParams.layout };
    LabeledSlider stereoKnob   { plugin_, plugin_.dynamicParams.stereoWidth };

    struct ChannelGroup {
        ChannelGroup(AYChipPlugin& plugin)
            : channelOn   { plugin, plugin.dynamicParams.channelA }
            , toneOn      { plugin, plugin.dynamicParams.toneA }
            , noiseOn     { plugin, plugin.dynamicParams.noiseA }
            , envelopeOn  { plugin, plugin.dynamicParams.envelopeA }
        {}
        ToggleButton channelOn;
        ToggleButton toneOn;
        ToggleButton noiseOn;
        ToggleButton envelopeOn;
    };

    // Channel and effect toggles
    ToggleButton channelAButton   { plugin_, plugin_.dynamicParams.channelA };
    ToggleButton channelBButton   { plugin_, plugin_.dynamicParams.channelB };
    ToggleButton channelCButton   { plugin_, plugin_.dynamicParams.channelC };

    ToggleButton toneAButton      { plugin_, plugin_.dynamicParams.toneA };
    ToggleButton toneBButton      { plugin_, plugin_.dynamicParams.toneB };
    ToggleButton toneCButton      { plugin_, plugin_.dynamicParams.toneC };

    ToggleButton noiseAButton     { plugin_, plugin_.dynamicParams.noiseA };
    ToggleButton noiseBButton     { plugin_, plugin_.dynamicParams.noiseB };
    ToggleButton noiseCButton     { plugin_, plugin_.dynamicParams.noiseC };

    ToggleButton envelopeAButton  { plugin_, plugin_.dynamicParams.envelopeA };
    ToggleButton envelopeBButton  { plugin_, plugin_.dynamicParams.envelopeB };
    ToggleButton envelopeCButton  { plugin_, plugin_.dynamicParams.envelopeC };

    // Helper arrays for iteration
    std::array<ToggleButton*, 3> channelButtons;
    std::array<ToggleButton*, 3> toneButtons;
    std::array<ToggleButton*, 3> noiseButtons;
    std::array<ToggleButton*, 3> envelopeButtons;

    void setupToggleButtons();
    void layoutChannelToggles(juce::Rectangle<int>& r);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYPluginUI)
};


//==============================================================================
// Editor component for AYChipPlugin is a simple wrapper around AYPluginUI
//==============================================================================
class AYPluginEditor : public tracktion::Plugin::EditorComponent,
                       private AYPluginUI
{
public:
    AYPluginEditor(tracktion::Plugin::Ptr p)
      : AYPluginUI(p)
    {}

    bool allowWindowResizing() override {
        return true;
    }

    ComponentBoundsConstrainer* getBoundsConstrainer() override {
        return AYPluginUI::getBoundsConstrainer();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYPluginEditor)
};

}
