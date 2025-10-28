#include "NotesToPsgPluginUI.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"

namespace MoTool::uZX {

//==============================================================================
// NotesToPsgPluginUI
//==============================================================================
NotesToPsgPluginUI::NotesToPsgPluginUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , baseParams(notesToPsgPlugin()->staticParams)
{
    jassert(pluginPtr != nullptr);

    setSize(128, 80);
}

NotesToPsgPlugin* NotesToPsgPluginUI::notesToPsgPlugin() const {
    return dynamic_cast<NotesToPsgPlugin*>(plugin.get());
}

bool NotesToPsgPluginUI::hasDeviceMenu() const {
    return notesToPsgPlugin() != nullptr;
}

void NotesToPsgPluginUI::populateDeviceMenu(juce::PopupMenu& menu) {
    if (notesToPsgPlugin() == nullptr) {
        return;
    }

    addMidiRangeMenu(menu, baseParams.baseMidiChannel, baseParams.baseMidiChannel.definition.description, 4);
}

void NotesToPsgPluginUI::paint(Graphics&) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void NotesToPsgPluginUI::resized() {
    // static constexpr int knobSize = 32;
    // static constexpr int spacing = 8;

    // auto r = getLocalBounds().reduced(8, 8);
}

REGISTER_PLUGIN_UI_ADAPTER(NotesToPsgPlugin, NotesToPsgPluginUI)

}  // namespace MoTool::uZX
