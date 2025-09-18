#include "ChipInstrument.h"

namespace MoTool::uZX {

ChipInstrumentFx::ChipInstrumentFx(const ValueTree& vt, UndoManager* um)
    : MPEInstrumentFx<ChipInstrumentVoice<ChipInstrumentFx>, ChipInstrumentFx>(*this)
    , state(vt)
    , undoManager(um)
    , oscParams(*this, 0)
{
    state.addListener(this);
    restoreStateFromValueTree(state);
}

ChipInstrumentFx::~ChipInstrumentFx() {
    state.removeListener(this);
}

void ChipInstrumentFx::updateParams() {
    // TODO update global instrument parameters to all voices
    // TODO thread safety
    // not used because voices read parameters themselves from instrument reference
}

void ChipInstrumentFx::restoreStateFromValueTree(const ValueTree& v) {
    // TODO for every oscillator
    oscParams.restoreStateFromValueTree(v);
}

ChipInstrumentFx::OscParameters::OscParameters(ChipInstrumentFx& inst, int oscNum)
    : instrument(inst)
    // TODO change te::IDs::* with own IDs
    , ampAttack   {{"ampAttack",   te::IDs::ampAttack,   "A", "Amp Attack Time",   0.0f,   {0.0f, 6.0f, 0.02f, 0.5f}, "s"}}
    , ampDecay    {{"ampDecay",    te::IDs::ampDecay,    "D", "Amp Decay Time",    0.0f,   {0.0f, 6.0f, 0.02f, 0.5f}, "s"}}
    , ampSustain  {{"ampSustain",  te::IDs::ampSustain,  "S", "Amp Sustain Level", 100.0f, {0.0f, 100.0f}, "%"}}
    , ampRelease  {{"ampRelease",  te::IDs::ampRelease,  "R", "Amp Release Time",  0.0f,   {0.0f, 6.0f, 0.02f, 0.5f}, "s"}}
    , ampVelocity {{"ampVelocity", te::IDs::ampVelocity, "V", "Amp Velocity Sensitivity", 100.0f, {0.0f, 100.0f}, "%"}}
{
    ignoreUnused(oscNum);
    referToState();
}

void ChipInstrumentFx::OscParameters::referToState() {
    ampAttack.referTo(instrument.state, instrument.undoManager);
    ampDecay.referTo(instrument.state, instrument.undoManager);
    ampSustain.referTo(instrument.state, instrument.undoManager);
    ampRelease.referTo(instrument.state, instrument.undoManager);
    ampVelocity.referTo(instrument.state, instrument.undoManager);
}

void ChipInstrumentFx::OscParameters::restoreStateFromValueTree(const ValueTree& v) {
    te::copyPropertiesToCachedValues(v,
        ampAttack  .value,
        ampDecay   .value,
        ampSustain .value,
        ampRelease .value,
        ampVelocity.value
    );
}

}  // namespace MoTool::uZX