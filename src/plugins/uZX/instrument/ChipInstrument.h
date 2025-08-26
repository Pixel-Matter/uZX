#pragma once

#include <JuceHeader.h>

#include "../midi_effects/MPEInstrumentFx.h"
#include "ChipInstrumentVoice.h"

namespace MoTool::uZX {

//==============================================================================
/**
    Represents a chiptune-style instrument that uses MPE.
    Used by ChipInstrumentPlugin
*/

class ChipInstrumentFx : public MPEInstrumentFx<ChipInstrumentVoice> {
public:
    ChipInstrumentFx() = default;

    // TODO parameters?

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentFx)
};

}  // namespace MoTool::uZX