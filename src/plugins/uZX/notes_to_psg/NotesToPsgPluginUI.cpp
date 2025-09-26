#include "NotesToPsgPluginUI.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"

namespace MoTool::uZX {

//==============================================================================
// NotesToPsgPluginUI
//==============================================================================
NotesToPsgPluginUI::NotesToPsgPluginUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , baseParams(notesToPsgPlugin()->staticParams)
    , baseMidiKnob(baseParams.baseMidiChannel)
{
    jassert(pluginPtr != nullptr);

    setSize(128, 80);

    addAndMakeVisible(baseMidiKnob);
}

NotesToPsgPlugin* NotesToPsgPluginUI::notesToPsgPlugin() {
    return dynamic_cast<NotesToPsgPlugin*>(plugin.get());
}

void NotesToPsgPluginUI::paint(Graphics&) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void NotesToPsgPluginUI::resized() {
    static constexpr int knobSize = 32;
    // static constexpr int spacing = 8;

    auto r = getLocalBounds().reduced(8, 8);
    baseMidiKnob.setBounds(r.removeFromTop(knobSize + baseMidiKnob.getLabelHeight()));
}

REGISTER_PLUGIN_UI_ADAPTER(NotesToPsgPlugin, NotesToPsgPluginUI)

}  // namespace MoTool::uZX