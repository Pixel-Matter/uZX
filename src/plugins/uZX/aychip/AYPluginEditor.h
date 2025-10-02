#pragma once

#include <JuceHeader.h>

#include "AYPlugin.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/common/LabeledSlider.h"
#include "../../../gui/common/ChoiceButton.h"
#include "../../../gui/devices/PluginDeviceUI.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"


namespace MoTool::uZX {

//==============================================================================
// Editor for AYChipPlugin
//==============================================================================
class AYPluginUI : public PluginDeviceUI {
public:
    AYPluginUI(tracktion::Plugin::Ptr pluginPtr)
       : PluginDeviceUI(pluginPtr)
       , plugin_(*dynamic_cast<AYChipPlugin*>(pluginPtr.get()))
    {
        constrainer_.setMinimumWidth(160);
        setSize(160, 320);

        addAndMakeVisible(midiChannelKnob);
        addAndMakeVisible(chipTypeButton);
        addAndMakeVisible(clockKnob);

        addAndMakeVisible(volumeKnob);
        addAndMakeVisible(layoutButton);
        addAndMakeVisible(stereoKnob);
    }

    void paint(Graphics& g) override {
        g.fillAll(Colors::Theme::backgroundAlt);
    }

    void resized() override {
        auto r = getLocalBounds().reduced(8, 8);

        // static
        auto staticRow = r.removeFromTop(itemHeight * 2);
        auto width = staticRow.getWidth() / 3;
        midiChannelKnob.setBounds(staticRow.removeFromLeft(width));
        chipTypeButton.setBounds(staticRow.removeFromLeft(width));
        clockKnob.setBounds(staticRow);

        // automatable
        r.removeFromTop(itemSpacing);
        auto knobsRow = r.removeFromTop(itemHeight * 2);

        width = knobsRow.getWidth() / 3;
        layoutButton.setBounds(knobsRow.removeFromLeft(width).withSizeKeepingCentre(width, 20));
        stereoKnob.setBounds(knobsRow.removeFromLeft(width));
        volumeKnob.setBounds(knobsRow);
    }

    ComponentBoundsConstrainer* getBoundsConstrainer() {
        return &constrainer_;
    }

    static constexpr int itemHeight = 20;
    static constexpr int itemSpacing = 4;

private:
    AYChipPlugin& plugin_;
    ComponentBoundsConstrainer constrainer_;

    // static, cannot be automated
    LabeledSlider midiChannelKnob { plugin_, plugin_.staticParams.baseMidiChannel };
    ChoiceButton chipTypeButton   { plugin_, plugin_.staticParams.chipType };
    LabeledSlider clockKnob       { plugin_, plugin_.staticParams.chipClock };
    // TextButton DCButton        { plugin_, plugin_.staticParams.removeDC };

    // dynamic, can be automated
    LabeledSlider volumeKnob   { plugin_, plugin_.dynamicParams.volume };
    ChoiceButton layoutButton  { plugin_, plugin_.dynamicParams.layout };
    LabeledSlider stereoKnob   { plugin_, plugin_.dynamicParams.stereoWidth };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYPluginUI)
};

REGISTER_PLUGIN_UI_ADAPTER(AYChipPlugin, AYPluginUI)


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
