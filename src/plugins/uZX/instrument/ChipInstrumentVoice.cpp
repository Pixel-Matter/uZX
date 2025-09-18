#include "ChipInstrumentVoice.h"

namespace MoTool::uZX {

String toString(te::LinEnvelope adsr) {
    String result;
    switch (adsr.getState()) {
        case te::LinEnvelope::State::idle:    result = result + " idle";    break;
        case te::LinEnvelope::State::attack:  result = result + " attack";  break;
        case te::LinEnvelope::State::decay:   result = result + " decay";   break;
        case te::LinEnvelope::State::sustain: result = result + " sustain"; break;
        case te::LinEnvelope::State::release: result = result + " release"; break;
        default:                          result = result + " unknown"; break;
    }
    return result + " " + String(adsr.getEnvelopeValue());
}

}  // namespace MoTool::uZX