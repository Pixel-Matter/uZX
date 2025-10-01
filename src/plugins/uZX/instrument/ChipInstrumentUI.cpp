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
    , ampGroup("ampGroup", "Amp")
    , pitchGroup("pitchGroup", "Pitch")
    , adsrAttackSlider  (*pluginPtr, instrument.oscParams.ampAttack)
    , adsrDecaySlider   (*pluginPtr, instrument.oscParams.ampDecay)
    , adsrSustainSlider (*pluginPtr, instrument.oscParams.ampSustain)
    , adsrReleaseSlider (*pluginPtr, instrument.oscParams.ampRelease)
    // , adsrVelocitySlider(instrument.oscParams.ampVelocity)
    , pitchAttackSlider  (*pluginPtr, instrument.oscParams.pitchAttack)
    , pitchDecaySlider   (*pluginPtr, instrument.oscParams.pitchDecay)
    , pitchSustainSlider (*pluginPtr, instrument.oscParams.pitchSustain)
    , pitchReleaseSlider (*pluginPtr, instrument.oscParams.pitchRelease)
    , pitchDepthSlider   (*pluginPtr, instrument.oscParams.pitchDepth)
{
    jassert(pluginPtr != nullptr);

    setSize(32 * 4 + 8 * 7, 360);

    // TODO set font size
    addAndMakeVisible(ampGroup);
    addAndMakeVisible(pitchGroup);

    addAndMakeVisible(adsrAttackSlider);
    addAndMakeVisible(adsrDecaySlider);
    addAndMakeVisible(adsrSustainSlider);
    addAndMakeVisible(adsrReleaseSlider);

    addAndMakeVisible(pitchAttackSlider);
    addAndMakeVisible(pitchDecaySlider);
    addAndMakeVisible(pitchSustainSlider);
    addAndMakeVisible(pitchReleaseSlider);
    addAndMakeVisible(pitchDepthSlider);
}

ChipInstrumentPlugin* ChipInstrumentUI::instrumentPlugin() {
    return dynamic_cast<ChipInstrumentPlugin*>(plugin.get());
}

void ChipInstrumentUI::paint(Graphics&) {
    // g.fillAll(Colors::Theme::backgroundAlt);
}

void ChipInstrumentUI::resized() {
    auto r = getLocalBounds().reduced(8, 4);
    static constexpr int knobSize = 32;
    static constexpr int spacing = 8;
    static constexpr int groupSpacing = 2;
    static constexpr int groupPadding = 8;
    static constexpr int groupHeaderHeight = spacing;

    // Amplitude group
    auto ampGroupHeight = groupHeaderHeight + groupPadding + knobSize + adsrAttackSlider.getLabelHeight() + groupPadding;
    auto ampGroupBounds = r.removeFromTop(ampGroupHeight);
    ampGroup.setBounds(ampGroupBounds);

    // Position amp controls within the group
    auto ampArea = ampGroupBounds.reduced(groupPadding, groupPadding);
    ampArea.removeFromTop(groupHeaderHeight);
    adsrAttackSlider.setBounds(ampArea.removeFromLeft(knobSize));
    ampArea.removeFromLeft(spacing);
    adsrDecaySlider.setBounds(ampArea.removeFromLeft(knobSize));
    ampArea.removeFromLeft(spacing);
    adsrSustainSlider.setBounds(ampArea.removeFromLeft(knobSize));
    ampArea.removeFromLeft(spacing);
    adsrReleaseSlider.setBounds(ampArea.removeFromLeft(knobSize));

    r.removeFromTop(groupSpacing);

    // Pitch group
    auto pitchGroupHeight = groupHeaderHeight + groupPadding + (knobSize + pitchAttackSlider.getLabelHeight()) * 2 + spacing + groupPadding - 4;
    auto pitchGroupBounds = r.removeFromTop(pitchGroupHeight);
    pitchGroup.setBounds(pitchGroupBounds);

    // Position pitch controls within the group
    auto pitchArea = pitchGroupBounds.reduced(groupPadding, groupPadding);
    pitchArea.removeFromTop(groupHeaderHeight);

    // Pitch ADSR row
    auto pitchAdsrRow = pitchArea.removeFromTop(knobSize + pitchAttackSlider.getLabelHeight());
    pitchAttackSlider.setBounds(pitchAdsrRow.removeFromLeft(knobSize));
    pitchAdsrRow.removeFromLeft(spacing);
    pitchDecaySlider.setBounds(pitchAdsrRow.removeFromLeft(knobSize));
    pitchAdsrRow.removeFromLeft(spacing);
    pitchSustainSlider.setBounds(pitchAdsrRow.removeFromLeft(knobSize));
    pitchAdsrRow.removeFromLeft(spacing);
    pitchReleaseSlider.setBounds(pitchAdsrRow.removeFromLeft(knobSize));

    pitchArea.removeFromTop(spacing - 4);

    // Pitch Depth row
    auto depthRow = pitchArea.removeFromTop(knobSize + pitchDepthSlider.getLabelHeight());
    pitchDepthSlider.setBounds(depthRow.removeFromLeft(knobSize));
}

REGISTER_PLUGIN_UI_ADAPTER(ChipInstrumentPlugin, ChipInstrumentUI)

}  // namespace MoTool::uZX
