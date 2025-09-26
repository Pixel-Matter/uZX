#pragma once

#include <JuceHeader.h>
#include <memory>

#include "ChipInstrumentVoice.h"
#include "../midi_effects/MPEInstrumentFx.h"
#include "../midi_effects/Parameters.h"

namespace MoTool::uZX {

namespace IDs {
    #define DECLARE_ID(name)  inline const Identifier name(#name);
    DECLARE_ID(ampAttack)
    DECLARE_ID(ampDecay)
    DECLARE_ID(ampSustain)
    DECLARE_ID(ampRelease)
    DECLARE_ID(ampVelocity)
    DECLARE_ID(pitchAttack)
    DECLARE_ID(pitchDecay)
    DECLARE_ID(pitchSustain)
    DECLARE_ID(pitchRelease)
    DECLARE_ID(pitchDepth)
    #undef DECLARE_ID
}

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

    class OscParameters : public ParamsBase<OscParameters> {
    public:
        OscParameters(ChipInstrumentFx& inst, int oscNum);

        //==============================================================================
        /**
         * TODO: Parameters to implement
         * - shape (square, saw, triangle, noise)
         */
        // shape (square, saw, triangle, noise)
        // [x] amp adsr
        // amp lfo, steps, etc
        // amp level
        // pan, adsr, lfo, steps
        // coarse and fine tune
        // snap to envelope period
        // [x] pitch envelope
        // pitch lfo
        // AY env shape, retrigger
        // unison voices, detune

        ValueWithSource<float> ampAttack    {{"ampAttack",   IDs::ampAttack,   "A", "Amp Attack Time",   0.0f,   {0.0f, 6.0f, 0.02f, 0.5f}, "s"}};
        ValueWithSource<float> ampDecay     {{"ampDecay",    IDs::ampDecay,    "D", "Amp Decay Time",    0.0f,   {0.0f, 6.0f, 0.02f, 0.5f}, "s"}};
        ValueWithSource<float> ampSustain   {{"ampSustain",  IDs::ampSustain,  "S", "Amp Sustain Level", 100.0f, {0.0f, 100.0f}, "%"}};
        ValueWithSource<float> ampRelease   {{"ampRelease",  IDs::ampRelease,  "R", "Amp Release Time",  0.0f,   {0.0f, 6.0f, 0.02f, 0.5f}, "s"}};
     // ValueWithSource<float> ampVelocity  {{"ampVelocity", IDs::ampVelocity, "V", "Amp Velocity Sensitivity", 100.0f, {0.0f, 100.0f}, "%"}};
        ValueWithSource<float> pitchAttack  {{"pitchAttack", IDs::pitchAttack, "A", "Pitch Attack Time",  0.0f,  {0.0f, 6.0f, 0.02f, 0.5f}, "s"}};
        ValueWithSource<float> pitchDecay   {{"pitchDecay",  IDs::pitchDecay,  "D", "Pitch Decay Time",   0.0f,  {0.0f, 6.0f, 0.02f, 0.5f}, "s"}};
        ValueWithSource<float> pitchSustain {{"pitchSustain",IDs::pitchSustain,"S", "Pitch Sustain Level",0.0f,  {0.0f, 100.0f}, "%"}};
        ValueWithSource<float> pitchRelease {{"pitchRelease",IDs::pitchRelease,"R", "Pitch Release Time", 0.0f,  {0.0f, 6.0f, 0.02f, 0.5f}, "s"}};
        ValueWithSource<float> pitchDepth   {{"pitchDepth",  IDs::pitchDepth,  "Depth", "Pitch Depth",    0.0f,  {-48.0f, 48.0f}, "st"}};

        template<typename Visitor>
        void visit(Visitor&& visitor) {
            visitor(ampAttack);
            visitor(ampDecay);
            visitor(ampSustain);
            visitor(ampRelease);
            // visitor(ampVelocity);
            visitor(pitchAttack);
            visitor(pitchDecay);
            visitor(pitchSustain);
            visitor(pitchRelease);
            visitor(pitchDepth);
        }
    };

    // void updateParams();
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