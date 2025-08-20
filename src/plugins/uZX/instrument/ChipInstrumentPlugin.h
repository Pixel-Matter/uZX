#pragma once

#include <JuceHeader.h>

#include "MidiEffect.h"
#include "ChipInstrument.h"


namespace MoTool::uZX {

//==============================================================================
class ChipInstrumentPlugin :
                             public MidiFxPluginBase<ChipInstrumentFx>,
                            //  public tracktion::Plugin,
                             private tracktion::LevelMeasurer::Client
{
public:
    ChipInstrumentPlugin(tracktion::PluginCreationInfo);
    ~ChipInstrumentPlugin() override;

    //==============================================================================
    static const char* getPluginName() { return "uZX Instrument"; }
    static const char* xmlTypeName;

    String getName() const override { return "uZX Instrument"; }
    String getPluginType() override { return xmlTypeName; }
    String getShortName(int) override { return "uZXinst"; }
    String getSelectableDescription() override { return "uZX Instrument Plugin"; }

    void initialise(const tracktion::PluginInitialisationInfo&) override;
    void deinitialise() override;
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
    bool hasNameForMidiNoteNumber(int note, int midiChannel, juce::String& name) override;

    /** Returns the name for a midi program, if there is one.
        programNum = 0 to 127.
    */
    bool hasNameForMidiProgram(int programNum, int bank, juce::String& name) override;
    bool hasNameForMidiBank(int bank, juce::String& name) override;

    float getLevel(int channel);

private:
    //==============================================================================
    void valueTreeChanged() override;
    void valueTreePropertyChanged(ValueTree&, const Identifier&) override;
    void valueTreeChildAdded(ValueTree&, ValueTree&) override;
    void valueTreeChildRemoved(ValueTree&, ValueTree&, int) override;
    void flushPluginStateToValueTree() override;

    // Params
    tracktion::AutomatableParameter* addParam(const juce::String& paramID,
                                              const juce::String& name,
                                              juce::NormalisableRange<float> valueRange,
                                              juce::String label);

    // ChipInstrument instrument;
    // tracktion::tempo::Sequence::Position currentPos{createPosition(edit.tempoSequence)};
    bool flushingState = false;
    std::unordered_map<String, String> paramLabels;
    tracktion::LevelMeasurer levelMeasurer;
    tracktion::DbTimePair levels[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipInstrumentPlugin)
};

}  // namespace MoTool::uZX
