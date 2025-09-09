#include "GenereicPluginAdapters.h"
#include "../common/LookAndFeel.h"

namespace MoTool {

//==============================================================================
// GenericTracktionPluginUIAdapter

GenericPluginUIAdapter::GenericPluginUIAdapter(EditViewState& evs, tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(evs, pluginPtr)
{
    setSize(60, 40);  // Compact plugin button size
}

void GenericPluginUIAdapter::resized() {
    // Generic plugin UI is just a button-like component
}

void GenericPluginUIAdapter::paint(juce::Graphics& g) {
    g.fillAll(Colors::Theme::backgroundAlt);
    g.setColour(Colors::Theme::border);
    g.drawRect(getLocalBounds(), 1);

    g.setColour(Colors::Theme::textPrimary);
    g.setFont(10.0f);
    if (plugin) {
        g.drawText(plugin->getName(), getLocalBounds(), juce::Justification::centred);
    } else {
        g.drawText("No Plugin", getLocalBounds(), juce::Justification::centred);
    }
}

void GenericPluginUIAdapter::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isLeftButtonDown() && plugin) {
        // Open plugin editor when clicked
        plugin->showWindowExplicitly();
    }
}

}  // namespace MoTool
