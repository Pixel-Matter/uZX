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
        ChannelGroup(AYChipPlugin& plugin,
                     ParameterValue<bool>& channelParam,
                     ParameterValue<bool>& toneParam,
                     ParameterValue<bool>& noiseParam,
                     ParameterValue<bool>& envelopeParam)
            : channelOn   { plugin, channelParam }
            , toneOn      { plugin, toneParam }
            , noiseOn     { plugin, noiseParam }
            , envelopeOn  { plugin, envelopeParam }
        {}

        ToggleButton channelOn;
        ToggleButton toneOn;
        ToggleButton noiseOn;
        ToggleButton envelopeOn;

        void addToComponent(Component& parent) {
            parent.addAndMakeVisible(channelOn);
            parent.addAndMakeVisible(toneOn);
            parent.addAndMakeVisible(noiseOn);
            parent.addAndMakeVisible(envelopeOn);
        }
    };

    // Channel groups
    ChannelGroup channelA { plugin_, plugin_.dynamicParams.channelA,
                            plugin_.dynamicParams.toneA,
                            plugin_.dynamicParams.noiseA,
                            plugin_.dynamicParams.envelopeA };
    ChannelGroup channelB { plugin_, plugin_.dynamicParams.channelB,
                            plugin_.dynamicParams.toneB,
                            plugin_.dynamicParams.noiseB,
                            plugin_.dynamicParams.envelopeB };
    ChannelGroup channelC { plugin_, plugin_.dynamicParams.channelC,
                            plugin_.dynamicParams.toneC,
                            plugin_.dynamicParams.noiseC,
                            plugin_.dynamicParams.envelopeC };

    std::array<ChannelGroup*, 3> channelGroups { &channelA, &channelB, &channelC };

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
