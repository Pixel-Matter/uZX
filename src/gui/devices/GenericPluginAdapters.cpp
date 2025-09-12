#include "GenericPluginAdapters.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

GenericPluginUIAdapter::GenericPluginUIAdapter(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
{
    if (plugin) {
        pluginName_ = plugin->getName();
    }
    setSize(160, 200);  // Compact plugin button size
}

void GenericPluginUIAdapter::resized() {
    // Generic plugin UI is just a button-like component
}

void GenericPluginUIAdapter::paint(juce::Graphics& g) {
    // g.fillAll(Colors::Theme::backgroundAlt);
    if (!plugin) {
        return;
    }

    String pluginInfo = pluginName_ + "\n";
    pluginInfo += "by " + plugin->getVendor() + "\n";
    pluginInfo += plugin->getSelectableDescription() + "\n";
    pluginInfo += plugin->takesMidiInput() ? "takes MIDI\n" : "";
    pluginInfo += "(click to open)";

    g.setColour(Colors::Theme::textPrimary);
    g.setFont(14.0f);
    g.drawFittedText(pluginInfo, getLocalBounds(), juce::Justification::centred, 5);
}

void GenericPluginUIAdapter::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isLeftButtonDown() && plugin) {
        plugin->showWindowExplicitly();
    }
}

namespace GenericPluginUIFactory {

std::unique_ptr<PluginDeviceUI> createGenericUI(tracktion::Plugin::Ptr plugin) {
    return std::make_unique<GenericPluginUIAdapter>(plugin);
}

}  // namespace GenericPluginUIFactory

}  // namespace MoTool