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
class ChipInstrumentFx : public MPEInstrumentFx<ChipInstrumentVoice<ChipInstrumentFx>, ChipInstrumentFx>,
                         public ValueTree::Listener
{
public:
    ChipInstrumentFx(const ValueTree& vt, UndoManager* um = nullptr);
    ~ChipInstrumentFx() override;

    struct OscParameters {
        OscParameters(ChipInstrumentFx& inst, int oscNum);

        template<typename Visitor>
        void visit(Visitor&& visitor) {
            visitor(ampAttack);
            visitor(ampDecay);
            visitor(ampSustain);
            visitor(ampRelease);
            visitor(ampVelocity);
            visitor(pitchAttack);
            visitor(pitchDecay);
            visitor(pitchSustain);
            visitor(pitchRelease);
            visitor(pitchDepth);
        }

        void referToState();
        void restoreStateFromValueTree(const ValueTree& v);
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
        ValueWithSource<float> ampAttack;
        ValueWithSource<float> ampDecay;
        ValueWithSource<float> ampSustain;
        ValueWithSource<float> ampRelease;
        ValueWithSource<float> ampVelocity;
        ValueWithSource<float> pitchAttack;
        ValueWithSource<float> pitchDecay;
        ValueWithSource<float> pitchSustain;
        ValueWithSource<float> pitchRelease;
        ValueWithSource<float> pitchDepth;
    };

    void updateParams();
    void restoreStateFromValueTree(const ValueTree& v);

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