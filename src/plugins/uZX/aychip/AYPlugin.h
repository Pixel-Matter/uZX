#pragma once

#include <JuceHeader.h>

#include "../../../formats/psg/PsgData.h"
#include "../../../models/PsgMidi.h"
#include "../../../controllers/ParamAttachments.h"
#include "../../../controllers/Parameters.h"
#include "aychip.h"

#include <atomic>
#include <array>
#include <cstddef>

namespace te = tracktion;

namespace MoTool {

namespace uZX {

namespace IDs {
    #define DECLARE_ID(name)  inline const juce::Identifier name(#name);
    DECLARE_ID(chip)
    DECLARE_ID(clock)
    DECLARE_ID(layout)
    DECLARE_ID(stereo)
    DECLARE_ID(noDC)
    DECLARE_ID(midi)
    DECLARE_ID(volume)
    #undef DECLARE_ID
}  // namespace IDs


class AYChipPlugin : public te::Plugin {
public:
    AYChipPlugin (te::PluginCreationInfo);
    ~AYChipPlugin() override;

    //==============================================================================
    static const char* getPluginName()                  { return "μZX AY Emulator"; }
    static const char* xmlTypeName;

    String getVendor() override                   { return "PixelMatter"; }
    String getName() const override               { return String::fromUTF8(getPluginName()); }
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

    // // New struct to migrate to
    // struct StaticParams : public ParamsBase<StaticParams> {
    //     template<typename Visitor>
    //     void visit(Visitor&& visitor) {
    //         visitor(baseMidiChannel);
    //     }

    //     // 0-16, 0 = omni
    //     ParameterValue<int> baseMidiChannel {{"baseMidi", IDs::midi, "MIDI", "Base MIDI channel", 1, {0, 15 - 4, 1}}};
    // };

    // StaticParams staticParams {*this};

    // New struct to migrate to
    class DynamicParams : public ParamsBase<DynamicParams> {
    public:
        using ParamsBase<DynamicParams>::ParamsBase;

        template<typename Visitor>
        void visit(Visitor&& visitor) {
            visitor(volume);
            visitor(layout);
            visitor(stereoWidth);
        }

        ParameterValue<float> volume          {{"volume", IDs::volume, "Volume", "Output volume", 0.5f, {0.f, 1.0f}}};
        ParameterValue<ChannelsLayout> layout {{"layout", IDs::layout, "Layout", "Stereo layout", ChannelsLayout::ACB}};
        ParameterValue<float> stereoWidth     {{"stereo", IDs::stereo, "Width",  "Stereo width",  0.5f, {0.f, 1.0f}}};
    };

    DynamicParams dynamicParams;

    // legacy
    struct Params {
        // static
        RangedParamAttachment<int> baseMidiChannel;
        RangedParamAttachment<double> clock;
        ChoiceParamAttachment<ChipType> chipType;
        ParamAttachment<bool> removeDC;

        Params(te::Plugin& p)
            : baseMidiChannel(p.state, p.getUndoManager())
            , clock(p.state, p.getUndoManager())
            , chipType(p.state, p.getUndoManager())
            , removeDC(p.state, p.getUndoManager())
        {
            initialise();
        }

        void initialise();
        void restoreFromTree(const juce::ValueTree& v);
    };

    Params legacyParams {*this};

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
    // PsgRegsMidiReader midiRegsReader;  // old version
    PsgRegsFrame registersFrame;
    std::unique_ptr<AYInterface> chip;

    // double timeFromReset;

    // struct PendingChanges {
    //     std::unique_ptr<AYInterface> chip;
    // };

    // PendingChanges pendingChanges;
    std::atomic<bool> isProcessing {false};

    void valueTreeChanged() override;
    void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override;
    void updateRegistersFromMidiParams() noexcept;
    void updateRegistersFromMidiRegs() noexcept;
    void updateChip() noexcept;
    void handleMidiEvent(const te::MidiMessageWithSource& m) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AYChipPlugin)
};

} // namespace MoTool::uZX

} // namespace MoTool