#pragma once

#include <JuceHeader.h>
#include "NotesToPsgPlugin.h"
#include "../../../gui/devices/PluginDeviceUI.h"
#include "../../../gui/common/LabeledSlider.h"
#include "../../../gui/common/ParamBindings.h"

namespace MoTool::uZX {

namespace te = tracktion;

//==============================================================================
// NotesToPsgPluginUI
//==============================================================================
class NotesToPsgPluginUI : public PluginDeviceUI {
public:
    NotesToPsgPluginUI(tracktion::Plugin::Ptr pluginPtr);

    void paint(Graphics& g) override;
    void resized() override;

    NotesToPsgPlugin* notesToPsgPlugin() const;

    bool hasDeviceMenu() const override;
    void populateDeviceMenu(juce::PopupMenu& menu) override;

private:
    NotesToPsgPlugin::StaticParams& staticParams;

    Label tuningLabel;
    ComboBox tuningCombo;
    ComboBoxParamEndpointBinding tuningBinding;

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
