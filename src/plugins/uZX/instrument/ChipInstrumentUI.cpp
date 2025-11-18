#include "ChipInstrumentUI.h"
#include "../../../gui/common/LookAndFeel.h"
#include "../../../gui/devices/PluginUIAdapterRegistry.h"

namespace te = tracktion;

namespace MoTool::uZX {

//==============================================================================
// ChipInstrumentUI
//==============================================================================
ChipInstrumentUI::ChipInstrumentUI(tracktion::Plugin::Ptr pluginPtr)
    : PluginDeviceUI(pluginPtr)
    , instrument(instrumentPlugin()->instrument)
    , oscillatorControls(pluginPtr, instrument.oscParams, 0)
{
    jassert(pluginPtr != nullptr);

    addAndMakeVisible(oscillatorControls);

    // Width calculation: 2 groups of 208px each + 8px spacing between + 16px outer margins
    // groupWidth = 8 + (32 + 8) * 5 - 8 + 8 = 208px
    // total = 208 + 8 + 208 + 16 = 440px
    setSize(440, 360);
}

ChipInstrumentPlugin* ChipInstrumentUI::instrumentPlugin() {
    return dynamic_cast<ChipInstrumentPlugin*>(plugin.get());
}

void ChipInstrumentUI::paint(Graphics&) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void ChipInstrumentUI::resized() {
    auto r = getLocalBounds().reduced(8, 4);

    // Position oscillator controls at the top
    // Height: 8 (header) + 8 (padding) + 32 (knob) + ~12 (label) + 8 (padding) ≈ 68px
    auto oscRow = r.removeFromTop(68);
    oscillatorControls.setBounds(oscRow);

    // Future: Add 3 more oscillator control rows here
}

REGISTER_PLUGIN_UI_ADAPTER(ChipInstrumentPlugin, ChipInstrumentUI)

}  // namespace MoTool::uZX
