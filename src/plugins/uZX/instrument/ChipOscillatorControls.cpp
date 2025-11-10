#include "ChipOscillatorControls.h"
#include "ChipInstrument.h"

namespace MoTool::uZX {

ChipOscillatorControls::ChipOscillatorControls(tracktion::Plugin::Ptr plugin,
                                               ChipInstrumentFx::OscParameters& params,
                                               int oscNumber)
    : ampGroup("ampGroup", "Amp")
    , pitchGroup("pitchGroup", "Pitch")
    , ampLevelSlider(*plugin, params.ampLevel)
    , adsrAttackSlider(*plugin, params.ampAttack)
    , adsrDecaySlider(*plugin, params.ampDecay)
    , adsrSustainSlider(*plugin, params.ampSustain)
    , adsrReleaseSlider(*plugin, params.ampRelease)
    , pitchDepthSlider(*plugin, params.pitchDepth)
    , pitchAttackSlider(*plugin, params.pitchAttack)
    , pitchDecaySlider(*plugin, params.pitchDecay)
    , pitchSustainSlider(*plugin, params.pitchSustain)
    , pitchReleaseSlider(*plugin, params.pitchRelease)
{
    addAndMakeVisible(ampGroup);
    addAndMakeVisible(pitchGroup);

    addAndMakeVisible(ampLevelSlider);
    addAndMakeVisible(adsrAttackSlider);
    addAndMakeVisible(adsrDecaySlider);
    addAndMakeVisible(adsrSustainSlider);
    addAndMakeVisible(adsrReleaseSlider);

    addAndMakeVisible(pitchDepthSlider);
    addAndMakeVisible(pitchAttackSlider);
    addAndMakeVisible(pitchDecaySlider);
    addAndMakeVisible(pitchSustainSlider);
    addAndMakeVisible(pitchReleaseSlider);
}

void ChipOscillatorControls::paint(Graphics&) {
    // Nothing to paint, groups handle their own rendering
}

void ChipOscillatorControls::resized() {
    auto r = getLocalBounds();
    static constexpr int knobSize = 32;
    static constexpr int spacing = 8;
    static constexpr int groupSpacing = 8;
    static constexpr int groupPadding = 8;
    static constexpr int groupHeaderHeight = spacing;

    // Calculate group dimensions
    auto groupHeight = groupHeaderHeight + groupPadding + knobSize + ampLevelSlider.getLabelHeight() + groupPadding;
    auto groupWidth = groupPadding + (knobSize + spacing) * 5 - spacing + groupPadding;

    // Amplitude group (left side)
    auto ampGroupBounds = r.removeFromLeft(groupWidth);
    ampGroupBounds.setHeight(groupHeight);
    ampGroup.setBounds(ampGroupBounds);

    // Position amp controls within the group - Level, Attack, Decay, Sustain, Release
    auto ampArea = ampGroupBounds.reduced(groupPadding, groupPadding);
    ampArea.removeFromTop(groupHeaderHeight);
    ampLevelSlider.setBounds(ampArea.removeFromLeft(knobSize));
    ampArea.removeFromLeft(spacing);
    adsrAttackSlider.setBounds(ampArea.removeFromLeft(knobSize));
    ampArea.removeFromLeft(spacing);
    adsrDecaySlider.setBounds(ampArea.removeFromLeft(knobSize));
    ampArea.removeFromLeft(spacing);
    adsrSustainSlider.setBounds(ampArea.removeFromLeft(knobSize));
    ampArea.removeFromLeft(spacing);
    adsrReleaseSlider.setBounds(ampArea.removeFromLeft(knobSize));

    r.removeFromLeft(groupSpacing);

    // Pitch group (right side) - single row with Depth first
    auto pitchGroupBounds = r.removeFromLeft(groupWidth);
    pitchGroupBounds.setHeight(groupHeight);
    pitchGroup.setBounds(pitchGroupBounds);

    // Position pitch controls within the group - Depth, Attack, Decay, Sustain, Release
    auto pitchArea = pitchGroupBounds.reduced(groupPadding, groupPadding);
    pitchArea.removeFromTop(groupHeaderHeight);

    pitchDepthSlider.setBounds(pitchArea.removeFromLeft(knobSize));
    pitchArea.removeFromLeft(spacing);
    pitchAttackSlider.setBounds(pitchArea.removeFromLeft(knobSize));
    pitchArea.removeFromLeft(spacing);
    pitchDecaySlider.setBounds(pitchArea.removeFromLeft(knobSize));
    pitchArea.removeFromLeft(spacing);
    pitchSustainSlider.setBounds(pitchArea.removeFromLeft(knobSize));
    pitchArea.removeFromLeft(spacing);
    pitchReleaseSlider.setBounds(pitchArea.removeFromLeft(knobSize));
}

}  // namespace MoTool::uZX
