#include "NotesToPsgPluginUI.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"
#include "juce_graphics/juce_graphics.h"

namespace MoTool::uZX {

//==============================================================================
// NotesToPsgPluginUI
//==============================================================================
NotesToPsgPluginUI::NotesToPsgPluginUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , staticParams(notesToPsgPlugin()->staticParams)
    , tuningBinding(tuningCombo, staticParams.tuningTable)
{
    jassert(pluginPtr != nullptr);

    tuningLabel.setText("Tuning preset:", dontSendNotification);
    tuningLabel.setJustificationType(Justification::centredLeft);
    tuningLabel.setFont(FontOptions().withPointHeight(11.0f));

    addAndMakeVisible(tuningLabel);
    addAndMakeVisible(tuningCombo);

    setSize(192, 72);
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

    addMidiRangeMenu(menu, staticParams.baseMidiChannel, staticParams.baseMidiChannel.definition.description, 4);
}

void NotesToPsgPluginUI::paint(Graphics&) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void NotesToPsgPluginUI::resized() {
    static constexpr int itemHeight = 20;
    static constexpr int spacing = 4;

    auto r = getLocalBounds().reduced(8, 4);

    tuningLabel.setBounds(r.removeFromTop(itemHeight));
    r.removeFromTop(spacing);
    tuningCombo.setBounds(r.removeFromTop(itemHeight));
}

REGISTER_PLUGIN_UI_ADAPTER(NotesToPsgPlugin, NotesToPsgPluginUI)

}  // namespace MoTool::uZX
