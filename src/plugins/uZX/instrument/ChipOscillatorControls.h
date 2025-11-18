#pragma once

#include <JuceHeader.h>
#include "ChipInstrument.h"
#include "../../../gui/common/LabeledSlider.h"

namespace MoTool::uZX {

//==============================================================================
/**
    A component containing amp and pitch envelope controls for a single oscillator.
    This component can be reused for multiple oscillators.
*/
class ChipOscillatorControls : public Component {
public:
    ChipOscillatorControls(tracktion::Plugin::Ptr plugin,
                           ChipInstrumentFx::OscParameters& params,
                           int oscNumber);

    void paint(Graphics& g) override;
    void resized() override;

private:
    GroupComponent ampGroup;
    GroupComponent pitchGroup;

    LabeledSlider ampLevelSlider;
    LabeledSlider adsrAttackSlider, adsrDecaySlider, adsrSustainSlider, adsrReleaseSlider;
    LabeledSlider pitchDepthSlider;
    LabeledSlider pitchAttackSlider, pitchDecaySlider, pitchSustainSlider, pitchReleaseSlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipOscillatorControls)
};

}  // namespace MoTool::uZX
