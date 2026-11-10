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
#include "../scope/ScopePluginUI.h"


namespace MoTool::uZX {

//==============================================================================
/** The chip's A, B and C oscilloscopes, following its scope settings. */
class AYScopes : public Component
{
public:
    struct Look
    {
        bool classic = false;     // white on black with grey rules between, else each in its channel's colour
        bool sideBySide = false;  // A | B | C, else A over B over C
    };

    AYScopes(AYChipPlugin& plugin, Look look);

    void paint(Graphics& g) override;
    void resized() override;

    static constexpr int spacing = 4;

private:
    std::array<std::unique_ptr<WaveformDisplay>, 3> displays_;
    const Look look_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AYScopes)
};


//==============================================================================
// Editor for AYChipPlugin
//==============================================================================
class AYPluginUI : public PluginDeviceUI {
public:
    AYPluginUI(tracktion::Plugin::Ptr pluginPtr);
    ~AYPluginUI() override;

    void paint(Graphics& g) override;

    void resized() override;

    ComponentBoundsConstrainer* getBoundsConstrainer();

    bool hasDeviceMenu() const override;
    int getMinimumHeight() const override;
    void populateDeviceMenu(PopupMenu& menu) override;

    static constexpr int itemHeight = 20;
    static constexpr int itemSpacing = AYScopes::spacing;
    static constexpr int minToggleHeight = 16;
    static constexpr int maxButtonWidth = 44;  // as at the narrowest: a wider panel widens the scopes  // T, N and E stay legible: button text is 10 shorter

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

    struct ChannelGroup : private Value::Listener {
        ChannelGroup(AYChipPlugin& plugin,
                     ParameterValue<bool>& channelParam,
                     ParameterValue<bool>& toneParam,
                     ParameterValue<bool>& noiseParam,
                     ParameterValue<bool>& envelopeParam,
                     ParameterValue<bool>& soloParam);

        ~ChannelGroup();

        ToggleButton channelOn;
        ToggleButton toneOn;
        ToggleButton noiseOn;
        ToggleButton envelopeOn;

        void addToComponent(Component& parent);
        void setupWidgets();
        void layoutButtons(Rectangle<int> bounds);

    private:
        void valueChanged(Value&) override;
        void updateTNEState();
        void updateSoloColour();
        void setupButtonColours(ToggleButton& button);

        ParameterValue<bool>& channelParam_;
        ParameterValue<bool>& soloParam_;
    };

    // Channel groups
    ChannelGroup channelA { plugin_, plugin_.channelMuter.channelA,
                            plugin_.channelMuter.toneA,
                            plugin_.channelMuter.noiseA,
                            plugin_.channelMuter.envelopeA,
                            plugin_.channelMuter.soloA };
    ChannelGroup channelB { plugin_, plugin_.channelMuter.channelB,
                            plugin_.channelMuter.toneB,
                            plugin_.channelMuter.noiseB,
                            plugin_.channelMuter.envelopeB,
                            plugin_.channelMuter.soloB };
    ChannelGroup channelC { plugin_, plugin_.channelMuter.channelC,
                            plugin_.channelMuter.toneC,
                            plugin_.channelMuter.noiseC,
                            plugin_.channelMuter.envelopeC,
                            plugin_.channelMuter.soloC };

    std::array<ChannelGroup*, 3> channelGroups { &channelA, &channelB, &channelC };

    AYScopes scopes_ {plugin_, {}};

    void setupToggleButtons();
    void layoutChannelToggles(Rectangle<int>& r);

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
