#include "TracktionPluginAdapters.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

//==============================================================================
// GenericTracktionPluginUIAdapter

GenericTracktionPluginUIAdapter::GenericTracktionPluginUIAdapter(EditViewState& evs, tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(evs, pluginPtr)
{
    if (pluginPtr) {
        pluginName_ = pluginPtr->getName().substring(0, 8);  // Truncate for compact display
    }
    setSize(60, 40);  // Compact plugin button size
}

void GenericTracktionPluginUIAdapter::resized() {
    // Generic plugin UI is just a button-like component
}

void GenericTracktionPluginUIAdapter::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);
    g.setColour(Colors::Theme::border);
    g.drawRect(getLocalBounds(), 1);

    g.setColour(Colors::Theme::textPrimary);
    g.setFont(10.0f);
    g.drawText(pluginName_, getLocalBounds(), juce::Justification::centred);
}

void GenericTracktionPluginUIAdapter::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isLeftButtonDown() && plugin) {
        // Open plugin editor when clicked
        if (auto editor = plugin->createEditor()) {
            editor->setVisible(true);
        }
    }
}

//==============================================================================
namespace TracktionPluginUIFactory {
    // Generic tracktion plugin factories for fallback
    std::unique_ptr<PluginDeviceUI> createGenericUI(EditViewState& evs, tracktion::Plugin::Ptr plugin) {
        return std::make_unique<GenericTracktionPluginUIAdapter>(evs, plugin);
    }
}

// Auto-register tracktion plugin adapters
// REGISTER_PLUGIN_UI_ADAPTER(tracktion::LevelMeterPlugin, LevelMeterPluginUIAdapter, true, false)

}  // namespace MoTool