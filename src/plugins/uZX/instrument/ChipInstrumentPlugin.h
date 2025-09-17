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

    ChipInstrumentFx instrument;
    // Amplitude envelope automatable parameters
    tracktion::AutomatableParameter::Ptr ampAttack, ampDecay, ampSustain, ampRelease, ampVelocity;
    // pitch envelope automatable parameters
    // tracktion::AutomatableParameter::Ptr pitchAttack, pitchDecay, pitchSustain, pitchRelease;
    //==============================================================================

private:
    void valueTreeChanged() override;
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void flushPluginStateToValueTree() override;

    // Params
    tracktion::AutomatableParameter::Ptr addParam(const String& paramID,
                                                  const String& name,
                                                  NormalisableRange<float> valueRange,
                                                  String label = {});

    template <typename Type>
    tracktion::AutomatableParameter::Ptr addParam(ParameterDef<Type> def) {
        static_assert(std::is_same<Type, float>::value, "Only float parameters are supported");
        // TODO but any Range can be converted to float
        return addParam(def.paramID, def.description, def.valueRange, def.paramID);
    }

    template <typename Type>
    std::unique_ptr<TracktionParamSource> addParamSource(ValueWithDef<Type>& vd) {
        return std::make_unique<TracktionParamSource>(addParam(vd.definition));
    }

    template <typename Type>
    tracktion::AutomatableParameter::Ptr addAttachParamSource(ValueWithDef<Type>& vd) {
        auto source = addParamSource(vd);
        vd.attachSource(std::move(source));
        return vd.source->parameter;
    }

    // Source factory
    std::function<std::unique_ptr<TracktionParamSource>(ValueWithDef<float>&)> paramSourceFactory =
        [this](ValueWithDef<float>& vd) {
            return std::make_unique<TracktionParamSource>(addParam(vd.definition));
        };

    //==============================================================================
    bool flushingState = false;
    std::unordered_map<String, String> paramLabels;
    tracktion::LevelMeasurer levelMeasurer;
    tracktion::DbTimePair levels[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentPlugin)
};

}  // namespace MoTool::uZX
