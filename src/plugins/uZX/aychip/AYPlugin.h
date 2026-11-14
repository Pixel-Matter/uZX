#pragma once

#include <JuceHeader.h>

#include "../../../formats/psg/PsgData.h"
#include "../../../models/PsgCC.h"
#include "../../../controllers/BindedAutoParameter.h"
#include "../scope/ScopeBuffer.h"
#include "../scope/ScopeSettings.h"
#include "aychip.h"
#include "ChannelMuter.h"
#include "ChipTimers.h"

#include <array>
#include <atomic>
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
    BusLayout getBusses() const override          { return BusLayout::singleStereoInOut(); }

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

    /** The ZX Spectrum 128's clock, a chip's until it is set. */
    static constexpr double defaultClockMhz = 1.7734;

    /** A chip's stereo layout until it is set. */
    static constexpr auto defaultLayout = LayoutEnum::ACB;

    class StaticParams : public ParamsBase<StaticParams> {
    public:
        using ParamsBase<StaticParams>::ParamsBase;

        template<typename Visitor>
        void visit(Visitor&& visitor) {
            visitor(baseMidiChannel);
            visitor(chipType);
            visitor(chipClock);
            visitor(removeDC);
            visitor(monitorMode);
        }

        ParameterValue<int> baseMidiChannel   {{"midi",        IDs::midi,        "MIDI",            "MIDI channel range",             1,      {1, 16 - 3, 1}}};
        ParameterValue<ChipType> chipType     {{"chip",        IDs::chip,        "Chip",            "Chip type",                      ChipType::AY}};
        ParameterValue<double> chipClock      {{"clock",       IDs::clock,       "Clock",           "Clock frequncy",                 defaultClockMhz, {0.894887, 2.0, 0.01}, "MHz"}};
        ParameterValue<bool> removeDC         {{"noDC",        IDs::noDC,        "Remove DC",       "Remove DC from output",          true}};
        ParameterValue<bool> monitorMode      {{"monitor",     IDs::monitor,     "Monitor",         "Chip debug to console",         false}};
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
        }

        ParameterValue<float> volume          {{"volume", IDs::volume, "Volume", "Output volume", 0.5f, {0.f, 1.0f}}};
        ParameterValue<ChannelsLayout> layout {{"layout", IDs::layout, "Layout", "Stereo layout", defaultLayout}};
        ParameterValue<float> stereoWidth     {{"stereo", IDs::stereo, "Width",  "Stereo width",  0.5f, {0.f, 1.0f}}};
    };

    DynamicParams dynamicParams;
    ChannelMuter channelMuter;
    ScopeSettings scopeSettings {ScopeSettings::Signal::ChipLevels};

    /**
     * Get visualization buffer for raw channel output (0=A, 1=B, 2=C): the chip's DAC levels 0..1,
     * unfiltered and before the stereo mix and the volume.
     */
    const ScopeBuffer* getVizChannelBuffer(int channel) const {
        if (channel >= 0 && channel < 3)
            return &vizBuffers_[static_cast<size_t>(channel)];
        return nullptr;
    }

    /** The emulated chip's registers R0-R13 as playback has set them. */
    PsgRegsFrame getChipRegisters() const;

    /** Timer commands the chip could not play, each a bit. They stay until the chip is reset or, for the
        two about the bank, until a bank is found: a tune that goes wrong once shows it after it stops. */
    enum TimerWarning : uint32_t {
        timerBankNotFound = 1,    ///< the bank chosen for the timers is not registered
        timerCommandNoBank = 2,   ///< a timer was started with no bank chosen
        timerCommandIgnored = 4,  ///< a command named a timer, lane or action its bank does not have
    };
    uint32_t getTimerWarnings() const noexcept { return timerWarnings.load(std::memory_order_relaxed); }

    /** A bit for each channel A, B, C a timer drives now: one that holds its volume or tone period, the
        mixer, the envelope while the channel uses it, or the noise while the channel has it on. */
    uint8_t getTimerChannels() const noexcept { return timerChannels.load(std::memory_order_relaxed); }

    /** What the warnings say, a line each; empty when there are none. */
    static juce::String describeTimerWarnings(uint32_t warnings);

    /** Solos channel 0-2, turning it on if muted, or unsolos it; several can be soloed, on any chips of the edit. */
    void toggleSolo(size_t channel);

private:
    //==============================================================================

    Colour colour;
    CriticalSection lock;

    // What the current frame's messages set, over the values the chip held when the frame began. The registers
    // are the chip's only lasting state: this is empty between frames.
    PsgDecoder frameDecoder;  // the controllers since the chip was last written
    std::unique_ptr<AYInterface> chip;

    // Timed writes between frames. The timers' controllers are read a frame at a time as the chip's are;
    // what they hold lasts until a reset, as the timers do
    TimerDecoder timerDecoder;
    ChipTimers timers;
    ChipTime chipTime;           // the chip's clock against the samples rendered
    uint64_t samplesRendered = 0;  // since the last reset: where a frame or a timer's write is in chip time
    std::atomic<uint32_t> timerWarnings {0};  // TimerWarning bits, written on the audio thread
    std::atomic<uint8_t> timerChannels {0};   // the channels a timer drives, written on the audio thread

    // The scopes' trigger on a channel a timer drives. A timer's writes leave the tone and the envelope out of
    // step with what the channel plays, or stop them, so such a channel's sync rises where the timer's output
    // repeats instead. A repeat too slow to be a pitch is a modulation: the chip's own sync stays
    struct TimerSync {
        int8_t timer = -1;          // the timer triggered on: the lowest that drives the channel, not muted
        bool due = false;           // the next sample rendered starts a repeat
        bool pitched = false;       // the last repeat was short enough to be a pitch
        uint32_t sinceRepeat = 0;   // samples since the last repeat started
    };
    std::array<TimerSync, 3> timerSync_;
    uint8_t timerSyncChannels_ = 0;  // a bit per channel that a timer, not muted, drives

    //==============================================================================
    // Scope buffer access for visualization (used by ScopePlugin)
    static constexpr int kNumVizChannels = 3;  // A, B, C

    // Scope buffers for A, B, C channel visualization
    std::array<ScopeBuffer, kNumVizChannels> vizBuffers_;
    // The current render call's scope lanes on their way to the scope buffers: levels, sync and amplitude, one
    // channel per chip channel each
    AudioBuffer<float> scopeLanes_;

    //==============================================================================
    /**
    * Copy separate channel buffer to visualization buffers.
    */
    void copyTempBufferToVizBuffer();

    void valueTreeChanged() override;
    void valueTreePropertyChanged(ValueTree& v, const Identifier& id) override;
    PsgRegsFrame readRegisters() const noexcept;
    /** The registers as the frames left them, which a frame adds to: where a timer owns one, the frames'
        value held back for it, not the timer's. */
    PsgRegsFrame readBackground() const noexcept;
    void updateChip() noexcept;
    void handleMidiEvent(const te::MidiMessageWithSource& m) noexcept;

    void updateDynamicParams();

    /** Renders the block's samples from `from` up to `to`, with the timers' writes that fall among them
        each at its sample, and returns where the block is rendered to. */
    int renderTo(const te::PluginRenderContext& fc, int from, int to);
    /** Renders up to the last of the timers' writes before sample `to`, and returns where that is. */
    int renderTimerWrites(const te::PluginRenderContext& fc, int from, int to);
    /** Writes the timers' writes before `cycle` to the chip where it is rendered up to. */
    void writeTimersBefore(uint64_t cycle);
    void writeTimerRegister(const TaymTimerWrite&) noexcept;
    /** The channels that timers on `registers`, a bit each, drive, by what the chip holds. */
    uint8_t findTimerChannels(uint16_t registers) const noexcept;
    /** Puts the timers' repeats in place of the chip's sync in the scope lanes just rendered. */
    void writeTimerSync(float* const* sync, size_t numSamples) noexcept;
    /** Stops the timers and forgets what their controllers held. */
    void resetTimers() noexcept;
    /** The chip's clock in whole Hz, as chip time counts it. */
    uint32_t getClockHz() const;
    void renderChannels(const te::PluginRenderContext& fc, int currentSample, int timeSample);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AYChipPlugin)
};


} // namespace MoTool::uZX

} // namespace MoTool
