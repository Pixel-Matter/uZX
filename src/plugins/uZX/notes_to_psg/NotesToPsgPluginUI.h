#pragma once

#include <JuceHeader.h>
#include "NotesToPsgPlugin.h"
#include "../../../gui/devices/PluginDeviceUI.h"
#include "../../../gui/common/ComboBindingWithPresets.h"
#include "../../../gui/common/ComboBoxWithOverrideId.h"
#include "../../../gui/common/ParamBindings.h"

namespace MoTool::uZX {

namespace te = tracktion;

//==============================================================================
// NotesToPsgPluginUI
//==============================================================================
class NotesToPsgPluginUI : public PluginDeviceUI
                         , private ChangeListener
{
public:
    NotesToPsgPluginUI(tracktion::Plugin::Ptr pluginPtr);

    ~NotesToPsgPluginUI() override;

    void paint(Graphics& g) override;
    void resized() override;

    NotesToPsgPlugin* notesToPsgPlugin() const;

    bool hasDeviceMenu() const override;
    void populateDeviceMenu(juce::PopupMenu& menu) override;

private:
    NotesToPsgPlugin::StaticParams& staticParams;

    static constexpr int itemHeight = 20;
    static constexpr int spacing = 8;

    struct TuningGroup {
        TuningGroup(NotesToPsgPluginUI& ui);
        void resize(Rectangle<int>& r);

        Label label;
        ComboBox combo;
        ComboBoxParamEndpointBinding binding;
    } tuning;

    struct InfoGroup : public Value::Listener {
        InfoGroup(NotesToPsgPluginUI& ui);
        ~InfoGroup() override;
        void resize(Rectangle<int>& r);
        void update();
        void valueChanged(Value& value) override;

        Label tuningType;
        Label refTuning;
        Label tonicAndScale;
        Label chipClockLabel;
        Label chipClock;
        Label a4FrequencyLabel;
        Label a4Frequency;

        NotesToPsgPlugin& plugin;
        NotesToPsgPluginUI& parentUI;

        // ComboBoxWithOverrideId combo;
        // ComboBindingWithPresets binding;
    } tuningInfo;

    void changeListenerCallback(ChangeBroadcaster* source) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotesToPsgPluginUI)
};

//==============================================================================
// NotesToPsgPluginEditor
//==============================================================================
class NotesToPsgPluginEditor : public te::Plugin::EditorComponent
{
public:
    NotesToPsgPluginEditor(te::Plugin::Ptr p)
        : ui(p)
    {
        addAndMakeVisible(ui);
        setOpaque(true);
        setSize(ui.getWidth(), ui.getHeight());
    }

    void resized() override {
        ui.setBounds(getLocalBounds());
    }

    bool allowWindowResizing() override {
        return false;  // Fixed size UI
    }

    ComponentBoundsConstrainer* getBoundsConstrainer() override {
        return nullptr;  // No constraints needed for fixed size
    }

private:
    NotesToPsgPluginUI ui;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotesToPsgPluginEditor)
};

}  // namespace MoTool::uZX
