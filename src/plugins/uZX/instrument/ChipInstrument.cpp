#include "ChipInstrument.h"

namespace MoTool::uZX {

ChipInstrumentFx::OscParameters::OscParameters(ChipInstrumentFx& inst, int oscNum)
    // : instrument(inst)
{
    ignoreUnused(oscNum);
    referTo(inst.state, inst.undoManager);
}

//==============================================================================
ChipInstrumentFx::ChipInstrumentFx(const ValueTree& vt, UndoManager* um)
    : MPEInstrumentFx<ChipInstrumentVoice<ChipInstrumentFx>, ChipInstrumentFx>(*this)
    , state(vt)
    , undoManager(um)
    , oscParams(*this, 0)
{
    oscParams.referTo(state, undoManager);
    state.addListener(this);
    restoreStateFromValueTree(state);
}

ChipInstrumentFx::~ChipInstrumentFx() {
    state.removeListener(this);
}

// void ChipInstrumentFx::updateParams() {
//     // update global instrument parameters to all voices
//     // not used because voices read parameters themselves from instrument reference
// }

void ChipInstrumentFx::restoreStateFromValueTree(const ValueTree& v) {
    // TODO for every oscillator
    oscParams.restoreStateFromValueTree(v);
}

}  // namespace MoTool::uZX