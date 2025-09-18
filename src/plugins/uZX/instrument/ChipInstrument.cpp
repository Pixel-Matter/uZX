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
    , pitchAttack {{"pitchAttack", Identifier("pitchAttack"), "pA", "Pitch Attack Time",  0.0f,   {0.0f, 6.0f, 0.02f, 0.5f}, "s"}}
    , pitchDecay  {{"pitchDecay",  Identifier("pitchDecay"),  "pD", "Pitch Decay Time",   0.0f,   {0.0f, 6.0f, 0.02f, 0.5f}, "s"}}
    , pitchSustain{{"pitchSustain",Identifier("pitchSustain"),"pS", "Pitch Sustain Level",0.0f,   {0.0f, 100.0f}, "%"}}
    , pitchRelease{{"pitchRelease",Identifier("pitchRelease"),"pR", "Pitch Release Time", 0.0f,   {0.0f, 6.0f, 0.02f, 0.5f}, "s"}}
    , pitchDepth  {{"pitchDepth",  Identifier("pitchDepth"), "pDp", "Pitch Depth",       0.0f,   {-24.0f, 24.0f}, "st"}}
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
    pitchAttack.referTo(instrument.state, instrument.undoManager);
    pitchDecay.referTo(instrument.state, instrument.undoManager);
    pitchSustain.referTo(instrument.state, instrument.undoManager);
    pitchRelease.referTo(instrument.state, instrument.undoManager);
    pitchDepth.referTo(instrument.state, instrument.undoManager);
}

void ChipInstrumentFx::OscParameters::restoreStateFromValueTree(const ValueTree& v) {
    te::copyPropertiesToCachedValues(v,
        ampAttack  .value,
        ampDecay   .value,
        ampSustain .value,
        ampRelease .value,
        ampVelocity.value,
        pitchAttack .value,
        pitchDecay  .value,
        pitchSustain.value,
        pitchRelease.value,
        pitchDepth  .value
    );
}

}  // namespace MoTool::uZX