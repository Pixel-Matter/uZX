#pragma once

#include <JuceHeader.h>

#include "../midi_effects/MPEInstrumentFx.h"
#include "ChipInstrumentVoice.h"

namespace MoTool::uZX {

//==============================================================================

// TODO ChoiceParameterDef

template <typename Type>
struct ParameterDef {
    String paramID;
    Identifier propertyName;
    String description;
    Type defaultValue;
    NormalisableRange<Type> valueRange;
    String units = {};
    std::function<String(Type)> valueToStringFunction = {};
    std::function<Type(const String&)> stringToValueFunction = {};

    String toString() const {
        // output definition to string for output/debugging
        String s = paramID + ": " + description + " [" + String(valueRange.start)
                   + " - " + String(valueRange.end) + "]";
        if (units.isNotEmpty())
            s += " " + units;
        s += " (default " + String(defaultValue) + ")";
        return s;
    }

};

//==============================================================================
template <typename Type>
struct ValueWithDef {
    explicit ValueWithDef(const ParameterDef<Type>& def)
        : definition(def)
    {}

    // TODO variadic args passthru to ParameterDef<Type> ctor
    ValueWithDef(const ParameterDef<Type>& def, ValueTree& state, UndoManager* undoMgr = nullptr)
        : definition(def)
        , value(state, def.propertyName, undoMgr, def.defaultValue)
    {}

    ValueWithDef (ValueWithDef&&) = default;
    ValueWithDef& operator= (ValueWithDef&&) = default;

    // TODO pass ValueTree to ctor?
    inline void referTo(ValueTree& v, UndoManager* um) {
        value.referTo(v, definition.propertyName, um, definition.defaultValue);
    }

    // /// Sets a new value via the AutomatableParameter
    // void setParameter (Type newValue, NotificationType);

    // /// Releases the parameter and CachedValue
    // void reset();

    // /// Copies the matching property from the given ValueTree to this parameter
    // void setFromValueTree (const ValueTree&);

    ParameterDef<Type> definition;
    CachedValue<Type> value;
};


//==============================================================================
/**
    Represents a chiptune-style instrument that uses MPE.
    Used by ChipInstrumentPlugin
*/
class ChipInstrumentFx : public MPEInstrumentFx<ChipInstrumentVoice>,
                         public ValueTree::Listener
{
public:
    ChipInstrumentFx()
        : ChipInstrumentFx(ValueTree(te::IDs::PLUGIN))
    {}

    explicit ChipInstrumentFx(const ValueTree& vt)
        : oscParams(*this, 0)  // TODO multi-oscillator support
        , state(vt)
    {
        state.addListener(this);
        restoreStateFromValueTree(state);
    }

    ~ChipInstrumentFx() override {
        state.removeListener(this);
    }

    struct OscParameters {
        OscParameters() = default;

        OscParameters(ChipInstrumentFx& instrument, int oscNum) {
            ignoreUnused(instrument, oscNum);
        }

        // void attach();
        // void detach();

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

        ValueWithDef<float> ampAttack   {{"ampAttack",   te::IDs::ampAttack,   "Amp Attack",   0.0f,   {0.0f, 60.0f, 0.0f, 0.2f}}};
        ValueWithDef<float> ampDecay    {{"ampDecay",    te::IDs::ampDecay,    "Amp Decay",    0.0f,   {0.0f, 60.0f, 0.0f, 0.2f}}};
        ValueWithDef<float> ampSustain  {{"ampSustain",  te::IDs::ampSustain,  "Amp Sustain",  100.0f, {0.0f, 100.0f}, "%"}};
        ValueWithDef<float> ampRelease  {{"ampRelease",  te::IDs::ampRelease,  "Amp Release",  0.0f,   {0.0f, 60.0f, 0.0f, 0.2f}}};
        ValueWithDef<float> ampVelocity {{"ampVelocity", te::IDs::ampVelocity, "Amp Velocity", 100.0f, {0.0f, 100.0f}, "%"}};

        // TODO see ParameterWithStateValue
        // te::AutomatableParameter::Ptr tune, fineTune, level, pulseWidth, detune, spread, pan;

        void restoreStateFromValueTree(const ValueTree& v) {
            te::copyPropertiesToCachedValues(v,
                ampAttack  .value,
                ampDecay   .value,
                ampSustain .value,
                ampRelease .value,
                ampVelocity.value
            );
        }
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

    OscParameters oscParams {};

    // TODO multi-oscillator support
    // OwnedArray<OscParameters> oscParams; // Parameter sets for all four oscillators, indexed 0-3

private:
    ValueTree state;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentFx)
};

}  // namespace MoTool::uZX