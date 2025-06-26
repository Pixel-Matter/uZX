#pragma once

#include <JuceHeader.h>

#include "../../../formats/psg/PsgData.h"
#include "../../../models/PsgMidi.h"
#include "../../../controllers/ParamAttachments.h"
#include "aychip.h"

#include <atomic>
#include <array>
#include <cstddef>

namespace te = tracktion;

namespace MoTool {

namespace IDs {
    #define DECLARE_ID(name)  const juce::Identifier name(#name);
    DECLARE_ID(chip)
    DECLARE_ID(clock)
    DECLARE_ID(layout)
    DECLARE_ID(stereo)
    DECLARE_ID(noDC)
    DECLARE_ID(midi)
    #undef DECLARE_ID
}  // namespace IDs

namespace uZX {

class AYChipPlugin : public te::Plugin {
public:
    AYChipPlugin (te::PluginCreationInfo);
    ~AYChipPlugin() override;

    //==============================================================================
    static const char* getPluginName()                  { return "AY Chip"; }
    static const char* xmlTypeName;

    String getName() const override               { return "AY Chip"; }
    String getPluginType() override               { return xmlTypeName; }
    String getShortName (int) override            { return "AY"; }
    String getSelectableDescription() override    { return "AY Chip plugin based on Ayumi emulator"; }
    bool isSynth() override                       { return true; }

    int getNumOutputChannelsGivenInputs(int numInputChannels) override { return jmin (numInputChannels, 2); }
    void initialise(const te::PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer(const te::PluginRenderContext&) noexcept override;
    void midiPanic() override;
    void reset() override;

    //==============================================================================
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return false; }
    bool producesAudioWhenNoAudioInput() override       { return false; }
    void restorePluginStateFromValueTree (const ValueTree&) override;
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;

    struct Params {
        ParamAttachment<ChipType> chipTypeValue;
        ParamAttachment<double> clockValue;
        ParamAttachment<ChannelsLayout> channelsLayoutValue;
        ParamAttachment<double> stereoWidthValue;
        ParamAttachment<bool> removeDCValue;
        ParamAttachment<int> baseMidiChannelValue;

        Params(te::Plugin& p)
            : chipTypeValue(p.state, p.getUndoManager())
            , clockValue(p.state, p.getUndoManager())
            , channelsLayoutValue(p.state, p.getUndoManager())
            , stereoWidthValue(p.state, p.getUndoManager())
            , removeDCValue(p.state, p.getUndoManager())
            , baseMidiChannelValue(p.state, p.getUndoManager())
        {
            initialise();
        }

        void initialise();
        void restoreFromTree(const juce::ValueTree& v);
    };

    Params staticParams {*this};

    enum class MidiReaderMode {
        Params,
        Regs
    };

    MidiReaderMode midiReaderMode = MidiReaderMode::Params;

private:
    //==============================================================================

    Colour colour;
    CriticalSection lock;

    PsgParamsMidiReader midiParamsReader;
    PsgRegsMidiReader midiRegsReader;
    PsgRegsFrame registersFrame;
    std::unique_ptr<AYInterface> chip;

    double timeFromReset;

    struct PendingChanges {
        std::unique_ptr<AYInterface> chip;
    };

    PendingChanges pendingChanges;
    std::atomic<bool> isProcessing {false};

    void valueTreeChanged() override;
    void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override;
    void updateRegistersFromMidiParams() noexcept;
    void updateRegistersFromMidiRegs() noexcept;
    void updateChip() noexcept;
    void readMidi(const te::MidiMessageWithSource& m) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AYChipPlugin)
};

} // namespace MoTool::uZX

} // namespace MoTool