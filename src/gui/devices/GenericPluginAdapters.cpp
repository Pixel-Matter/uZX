#include "GenericPluginAdapters.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

GenericPluginUIAdapter::GenericPluginUIAdapter(EditViewState& evs, tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(evs, pluginPtr)
{
    if (pluginPtr) {
        pluginName_ = pluginPtr->getName().substring(0, 8);  // Truncate for compact display
    }
    setSize(160, 200);  // Compact plugin button size
}

void GenericPluginUIAdapter::resized() {
    // Generic plugin UI is just a button-like component
}

void GenericPluginUIAdapter::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);
    // g.setColour(Colors::Theme::border);
    // g.drawRect(getLocalBounds(), 1);

    g.setColour(Colors::Theme::textPrimary);
    g.setFont(14.0f);
    g.drawText(pluginName_, getLocalBounds(), juce::Justification::centred);
}

void GenericPluginUIAdapter::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isLeftButtonDown() && plugin) {
        DBG("GenericPluginUIAdapter: Opening editor for plugin " << plugin->getName());
        // Open plugin editor when clicked
        if (auto editor = plugin->createEditor()) {
            editor->setVisible(true);
        }
    }
}

namespace GenericPluginUIFactory {

std::unique_ptr<PluginDeviceUI> createGenericUI(EditViewState& evs, tracktion::Plugin::Ptr plugin) {
    return std::make_unique<GenericPluginUIAdapter>(evs, plugin);
}

}  // namespace GenericPluginUIFactory

}  // namespace MoTool