#pragma once

#include <JuceHeader.h>
#include "../../../controllers/Parameters.h"


namespace MoTool::uZX {

namespace IDs {
    #define DECLARE_ID(name)  const Identifier name(#name);
    DECLARE_ID(baseChannel)
    DECLARE_ID(omniMode)
    DECLARE_ID(mpeMode)
    DECLARE_ID(pbendRange)
    #undef DECLARE_ID
}

//==============================================================================
struct MidiFXOptions {

    ParameterValue<int> baseChannel {{ "baseMidiChannel", IDs::baseChannel, "Base MIDI Channel", "The base MIDI channel for this effect", 1, {1, 16}}};
    ParameterValue<bool> omniMode   {{ "omniMode",        IDs::omniMode,    "Omni Mode",         "Respond to all MIDI channels",          false}};
    ParameterValue<bool> mpeMode    {{ "mpeMode",         IDs::mpeMode,     "MPE Mode",          "Respond to MPE MIDI messages",          false}};
    ParameterValue<int> pbendRange  {{ "pbendRange",      IDs::pbendRange,  "Pitch Bend Range",  "The pitch bend range in semitones",     2, {1, 48}}};
};

}  // namespace MoTool::uZX
