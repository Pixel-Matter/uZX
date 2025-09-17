#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>

#include "ChipInstrument.h"
#include "../midi_effects/MidiEffect.h"
#include "../midi_effects/Parameters.h"


namespace MoTool::uZX {

//==============================================================================
class ChipInstrumentPlugin :
                             public MidiFxPluginBase<ChipInstrumentFx>,
                             private tracktion::LevelMeasurer::Client
{
public:
    using Ptr = ReferenceCountedObjectPtr<ChipInstrumentPlugin>;

    ChipInstrumentPlugin(tracktion::PluginCreationInfo);
    ~ChipInstrumentPlugin() override;

    //==============================================================================
    static const char* getPluginName() { return "μZX Instrument"; }
    static const char* xmlTypeName;

    String getName() const override { return String::fromUTF8(getPluginName()); }
    String getPluginType() override { return xmlTypeName; }
    String getShortName(int) override { return "Instr"; }
    String getSelectableDescription() override { return "Chiptune Instrument Plugin"; }

    void reset() override;
    void midiPanic() override;

    // void applyToBuffer(const tracktion::PluginRenderContext&) override;

    //==============================================================================
    // bool isSynth() override { return true; }
    // double getTailLength() const override { return instrument.getTailLength(); }

    void restorePluginStateFromValueTree(const ValueTree&) override;

    //==============================================================================
    /** If it's a synth that names its notes, this can return the name it uses for this note 0-127.
        Midi channel is 1-16
    */
    bool hasNameForMidiNoteNumber(int note, int midiChannel, String& name) override;

    /** Returns the name for a midi program, if there is one.
        programNum = 0 to 127.
    */
    bool hasNameForMidiProgram(int programNum, int bank, String& name) override;
    bool hasNameForMidiBank(int bank, String& name) override;

    float getLevel(int channel);


    // Amplitude envelope automatable parameters
    tracktion::AutomatableParameter::Ptr ampAttack, ampDecay, ampSustain, ampRelease, ampVelocity;
    // pitch envelope automatable parameters
    // tracktion::AutomatableParameter::Ptr pitchAttack, pitchDecay, pitchSustain, pitchRelease;

    ChipInstrumentFx instrument;

private:
    //==============================================================================
    void valueTreeChanged() override;
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void flushPluginStateToValueTree() override;

    // Params
    template <typename Type>
    tracktion::AutomatableParameter* addParam(const String& paramID,
                                              const String& name,
                                              NormalisableRange<Type> valueRange,
                                              String label = {});

    template <typename Type>
    tracktion::AutomatableParameter* addParam(ParameterDef<Type> def) {
        return addParam<Type>(def.paramID, def.description, def.valueRange, def.paramID);
    }

    template <typename Type>
    tracktion::AutomatableParameter* addAttachParam(ValueWithDef<Type>& vd) {
        auto ap = addParam(vd.definition);
        ap->attachToCurrentValue(vd.value);
        return ap;
    }

    template <typename Type>
    std::unique_ptr<TracktionParamSource> addAttachParamSource(ValueWithDef<Type>& vd) {
        return std::make_unique(addAttachParam(vd));
    }

    // Parameter factory
    std::function<tracktion::AutomatableParameter*(ValueWithDef<float>&)> paramSourceFactory =
        [this](ValueWithDef<float>& vd) {
            return addAttachParam(vd);
        };

    //==============================================================================
    bool flushingState = false;
    std::unordered_map<String, String> paramLabels;
    tracktion::LevelMeasurer levelMeasurer;
    tracktion::DbTimePair levels[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentPlugin)
};

}  // namespace MoTool::uZX
