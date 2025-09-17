#pragma once

#include <JuceHeader.h>
#include <memory>

#include "ChipInstrumentVoice.h"
#include "../midi_effects/MPEInstrumentFx.h"
#include "../midi_effects/Parameters.h"

namespace MoTool::uZX {


//==============================================================================
/**
    Represents a chiptune-style instrument that uses MPE.
    Used by ChipInstrumentPlugin
*/
class ChipInstrumentFx : public MPEInstrumentFx<ChipInstrumentVoice>,
                         public ValueTree::Listener
{
public:
    ChipInstrumentFx(const ValueTree& vt, UndoManager* um = nullptr)
        : state(vt)
        , undoManager(um)
        , oscParams(*this, 0)  // TODO multi-oscillator support
    {
        state.addListener(this);
        restoreStateFromValueTree(state);
    }

    ~ChipInstrumentFx() override {
        state.removeListener(this);
    }

    struct OscParameters {
        OscParameters(ChipInstrumentFx& inst, int oscNum)
            : instrument(inst)
            // TODO change te::IDs::* with own IDs
            , ampAttack   {{"ampAttack",   te::IDs::ampAttack,   "A", "Amp Attack Time",   0.0f,   {0.0f, 60.0f, 0.0f, 0.2f}}}
            , ampDecay    {{"ampDecay",    te::IDs::ampDecay,    "D", "Amp Decay Time",    0.0f,   {0.0f, 60.0f, 0.0f, 0.2f}}}
            , ampSustain  {{"ampSustain",  te::IDs::ampSustain,  "S", "Amp Sustain Level", 100.0f, {0.0f, 100.0f}, "%"}}
            , ampRelease  {{"ampRelease",  te::IDs::ampRelease,  "R", "Amp Release Time",  0.0f,   {0.0f, 60.0f, 0.0f, 0.2f}}}
            , ampVelocity {{"ampVelocity", te::IDs::ampVelocity, "V", "Amp Velocity Sensitivity", 100.0f, {0.0f, 100.0f}, "%"}}
        {
            ignoreUnused(oscNum);

            referToState();
        }

        // void attach();  // to what?
        // void detach();

        // TODO see ParameterWithStateValue, but without explicit dependency on te::AutomatableParameter

        void referToState() {
            ampAttack.referTo(instrument.state, instrument.undoManager);
            ampDecay.referTo(instrument.state, instrument.undoManager);
            ampSustain.referTo(instrument.state, instrument.undoManager);
            ampRelease.referTo(instrument.state, instrument.undoManager);
            ampVelocity.referTo(instrument.state, instrument.undoManager);
        }

        void restoreStateFromValueTree(const ValueTree& v) {
            te::copyPropertiesToCachedValues(v,
                ampAttack  .value,
                ampDecay   .value,
                ampSustain .value,
                ampRelease .value,
                ampVelocity.value
            );
        }
        //=======================================================================================
        // TODO
        // shape (square, saw, triangle, noise)
        // amp level,
        // [x] amp adsr,
        // amp lfo, steps, etc
        // pan, adsr, lfo, steps
        // coarse and fine tune
        // pitch envelope, lfo
        // AY env shape, retrigger
        // unison voices, detune

    private:
        ChipInstrumentFx& instrument;

    public:
        ValueWithDef<float> ampAttack;
        ValueWithDef<float> ampDecay;
        ValueWithDef<float> ampSustain;
        ValueWithDef<float> ampRelease;
        ValueWithDef<float> ampVelocity;

    };

    // Voices do not store ValueTree state, so we need to update them manually
    void updateParams() {
        // TODO update global instrument parameters to all voices
        // TODO thread safety
    }

    void restoreStateFromValueTree(const ValueTree& v) {
        // TODO for every oscillator
        oscParams.restoreStateFromValueTree(v);
    }

public:
    ValueTree state;
    UndoManager* undoManager = nullptr;
    OscParameters oscParams;

    // TODO multi-oscillator support
    // OwnedArray<OscParameters> oscParams; // Parameter sets for all four oscillators, indexed 0-3

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentFx)
};

}  // namespace MoTool::uZX