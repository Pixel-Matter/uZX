#pragma once

#include <JuceHeader.h>

#include "../../../formats/psg/PsgData.h"
#include "../../../models/PsgMidi.h"
#include "../../../controllers/BindedAutoParameter.h"
#include "aychip.h"
#include "ChannelMuter.h"

#include <atomic>
#include <array>
#include <cstddef>
#include <vector>

namespace te = tracktion;

namespace MoTool {

namespace uZX {

namespace IDs {
    #define DECLARE_ID(name)  inline const juce::Identifier name(#name);
    DECLARE_ID(chip)
    DECLARE_ID(clock)
    DECLARE_ID(layout)
    DECLARE_ID(stereo)
    DECLARE_ID(monitor)
    DECLARE_ID(noDC)
    DECLARE_ID(midi)
    DECLARE_ID(volume)
    DECLARE_ID(numChannels)
    #undef DECLARE_ID
}  // namespace IDs


class AYChipPlugin : public PluginBase {
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

    int getNumOutputChannelsGivenInputs(int /*numInputChannels*/) override;
    void initialise(const te::PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer(const te::PluginRenderContext&) noexcept override;
    void midiPanic() override;
    void reset() override;
    void getChannelNames(juce::StringArray*, juce::StringArray*) override;

    //==============================================================================
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return false; }
    bool producesAudioWhenNoAudioInput() override       { return false; }
    void restorePluginStateFromValueTree (const ValueTree&) override;
    std::unique_ptr<te::Plugin::EditorComponent> createEditor() override;

    class StaticParams : public ParamsBase<StaticParams> {
    public:
        using ParamsBase<StaticParams>::ParamsBase;

        template<typename Visitor>
        void visit(Visitor&& visitor) {
            visitor(baseMidiChannel);
            visitor(chipType);
            visitor(chipClock);
            visitor(removeDC);
            visitor(numOutputChannels);
        }

        ParameterValue<int> baseMidiChannel {{"midi",  IDs::midi,  "MIDI",  "MIDI channel range", 1,   {1, 16 - 3, 1}}};
        ParameterValue<ChipType> chipType   {{"chip",  IDs::chip,  "Chip",  "Chip type",      ChipType::AY}};
        ParameterValue<double> chipClock    {{"clock", IDs::clock, "Clock", "Clock frequncy", 1.7734, {0.894887, 2.0, 0.01}, "MHz"}};
        ParameterValue<bool> removeDC       {{"noDC",  IDs::noDC,  "Remove DC", "Remove DC from output", true}};
        ParameterValue<int> numOutputChannels {{"numChannels", IDs::numChannels, "Output Channels", "Number of output channels", 2, {1, 5, 1}}};
    };

    StaticParams staticParams;

    class DynamicParams : public ParamsBase<DynamicParams> {
    public:
        using ParamsBase<DynamicParams>::ParamsBase;

        template<typename Visitor>
        void visit(Visitor&& visitor) {
            visitor(volume);
            visitor(layout);
            visitor(stereoWidth);
            visitor(monitorMode);
        }

        ParameterValue<float> volume          {{"volume", IDs::volume, "Volume", "Output volume", 0.5f, {0.f, 1.0f}}};
        ParameterValue<ChannelsLayout> layout {{"layout", IDs::layout, "Layout", "Stereo layout", ChannelsLayout::ACB}};
        ParameterValue<float> stereoWidth     {{"stereo", IDs::stereo, "Width",  "Stereo width",  0.5f, {0.f, 1.0f}}};
        ParameterValue<bool> monitorMode      {{"monitor", IDs::monitor, "Monitor", "Chip monitor mode",  false}};
    };

    DynamicParams dynamicParams;
    ChannelMuter channelMuter;

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

    void updateDynamicParams();

    void renderChannels(const te::PluginRenderContext& fc, int currentSample, int timeSample);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AYChipPlugin)
};


} // namespace MoTool::uZX

} // namespace MoTool
