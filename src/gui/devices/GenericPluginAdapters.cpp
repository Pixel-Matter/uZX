#include "GenericPluginAdapters.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

GenericPluginUIAdapter::GenericPluginUIAdapter(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
{
    if (plugin) {
        pluginName_ = plugin->getName();
        if (auto externalPlugin = dynamic_cast<tracktion::ExternalPlugin*>(plugin.get())) {
            isExternal_ = true;
            if (auto pi = externalPlugin->getAudioPluginInstance()) {
                hasEditor_ = pi->hasEditor();
            }
        }
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

    String pluginInfo;
    pluginInfo += pluginName_ + "\n";
    pluginInfo += isExternal_ ? "(external plugin)\n" : "";
    pluginInfo += "by " + plugin->getVendor() + ".\n\n";

    pluginInfo += plugin->getSelectableDescription() + "\n\n";

    pluginInfo += plugin->takesMidiInput() ? "Takes MIDI.\n" : "";
    if (hasEditor_) {
        pluginInfo += "\n(Click to open)\n";
    }

    plugin->visitAllAutomatableParams([&](tracktion::AutomatableParameter& param) {
        pluginInfo += param.getParameterName() + ": " + String(param.getCurrentValue(), 2) + "\n";
    });

    g.setColour(Colors::Theme::textPrimary);
    g.setFont(14.0f);
    g.drawFittedText(pluginInfo, getLocalBounds(), juce::Justification::centred, 10);
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