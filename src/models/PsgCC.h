#pragma once

#include <JuceHeader.h>
#include <array>

#include "../formats/psg/PsgData.h"
#include "PsgClip.h"

namespace te = tracktion;

namespace MoTool {

//==============================================================================
/**
 * MIDI CC types used for PSG parameter control
 * TODO: use ChoiceEnum<> CRTP
 */
enum class MidiCCType {
    BankSelectMSB     = 0,   ///< Bank Select MSB
    ModWheel          = 1,   ///< Modulation Wheel (Coarse)
    Breath            = 2,   ///< Breath controller (Coarse) uZX: Used for noise pitch control
    // unused         = 3,
    Foot              = 4,   ///< Foot Controller (Coarse)
    PortaTime         = 5,   ///< Portamento Time (Coarse)
    DataEntryMSB      = 6,   ///< Data Entry MSB
    Volume            = 7,   ///< Channel Volume (formerly Main Volume) (Coarse)
    Balance           = 8,   ///< Balance (Coarse)
    // unused         = 9,
    Pan               = 10,  ///< Pan (Coarse)
    Expression        = 11,  ///< Expression (Coarse)
    Effect1           = 12,  ///< Effect Control 1 (Coarse)
    Effect2           = 13,  ///< Effect Control 2 (Coarse)

    // unused         = 14, 15

    //---General Purpose Controllers #1 to #4---
    GPC1              = 16,  ///< General Purpose Controller #1 (Slider)
    GPC2              = 17,  ///< General Purpose Controller #2 (Slider)
    GPC3              = 18,  ///< General Purpose Controller #3 (Slider)
    GPC4              = 19,  ///< General Purpose Controller #4 (Slider)

    CC20PeriodCoarse  = 20,  ///< Tone period (Coarse)
    // unused         = 21-31 (10)

    //---Main controllers, fine tuning (LSB for CC 0-31)---
    BankSelectLSB     = 32,  ///< Bank Select LSB
    ModWheelFine      = 33,  ///< Modulation Wheel (Fine)
    BreathFine        = 34,  ///< Breath controller (Fine)
    // unused         = 35,
    FootFine          = 36,  ///< Foot Controller (Fine)
    PortaTimeFine     = 37,  ///< Portamento Time (Fine)
    DataEntryLSB      = 38,  ///< Data Entry LSB
    VolumeFine        = 39,  ///< Channel Volume (formerly Main Volume) (Fine)
    BalanceFine       = 40,  ///< Balance (Fine)
    // unused         = 41,
    PanFine           = 42,  ///< Pan (Fine)
    ExpressionFine    = 43,  ///< Expression (Fine)
    Effect1Fine       = 44,  ///< Effect Control 1 (Fine)
    Effect2Fine       = 45,  ///< Effect Control 2 (Fine)

    // unused         = 46, 47
    //---General Purpose Controllers #1 to #4 LSB---
    GPC1Fine          = 48,  ///< General Purpose Controller #1 (Slider Fine)
    GPC2Fine          = 49,  ///< General Purpose Controller #2 (Slider Fine)
    GPC3Fine          = 50,  ///< General Purpose Controller #3 (Slider Fine)
    GPC4Fine          = 51,  ///< General Purpose Controller #4 (Slider Fine)

    CC52PeriodFine    = 52,  ///< Tone period (Fine)
    // unused         = 53-63 (10)

    //---Channel Play Mode---
    SustainOnOff      = 64,  ///< Damper Pedal On/Off (Sustain)
    PortaOnOff        = 65,  ///< Portamento On/Off
    SustenutoOnOff    = 66,  ///< Sustenuto On/Off
    SoftPedalOnOff    = 67,  ///< Soft Pedal On/Off
    LegatoFootSwOnOff = 68,  ///< Legato Footswitch On/Off
    Hold2OnOff        = 69,  ///< Hold 2 On/Off

    //---Sound Controllers #1 to #10---
    SoundVariation    = 70,  ///< Sound Variation
    FilterCutoff      = 71,  ///< Filter Cutoff (Timbre/Harmonic Intensity)
    ReleaseTime       = 72,  ///< Release Time
    AttackTime        = 73,  ///< Attack Time
    FilterResonance   = 74,  ///< Filter Resonance (Brightness)
    DecayTime         = 75,  ///< Decay Time (Sound Control 6)
    VibratoRate       = 76,  ///< Vibrato Rate (Sound Control 7)
    VibratoDepth      = 77,  ///< Vibrato Depth (Sound Control 8)
    VibratoDelay      = 78,  ///< Vibrato Delay (Sound Control 9)
    SoundCtrler10     = 79,  ///< undefined (Sound Control 10)

    //---General Purpose Controllers #5 to #8---
    GPB1ToneSwitch    = 80,  ///< General Purpose Switch #1 (on/off)
    GPB2NoiseSwitch   = 81,  ///< General Purpose Switch #2 (on/off)
    GPB3EnvSwitch     = 82,  ///< General Purpose Switch #3 (on/off)
    GPB4RetriggerSwitch = 83,  ///< General Purpose Switch #4 (on/off)
    PortaControl      = 84,  ///< Portamento Control

    // unused         = 85-90 (6)

    //---Effect Controllers---
    Eff1Depth         = 91,  ///< Effect 1 Depth (Reverb Send Level)
    Eff2Depth         = 92,  ///< Effect 2 Depth (Tremolo Level)
    Eff3Depth         = 93,  ///< Effect 3 Depth (Chorus Send Level)
    Eff4Depth         = 94,  ///< Effect 4 Depth (Delay/Celeste/Variation/Detune Level)
    Eff5Depth         = 95,  ///< Effect 5 Depth (Phaser Level)

    DataIncrement     = 96,  ///< Data Increment (+1)
    DataDecrement     = 97,  ///< Data Decrement (-1)
    NRPNSelectLSB     = 98,  ///< NRPN Select LSB
    NRPNSelectMSB     = 99,  ///< NRPN Select MSB
    RPNSelectLSB      = 100, ///< RPN Select LSB
    RPNSelectMSB      = 101, ///< RPN Select MSB

    //---Other Channel Mode Messages---
    AllSoundsOff      = 120, ///< All Sounds Off
    ResetAllCtrlers   = 121, ///< Reset All Controllers
    LocalCtrlOnOff    = 122, ///< Local Control On/Off
    AllNotesOff       = 123, ///< All Notes Off
    OmniModeOff       = 124, ///< Omni Mode Off + All Notes Off
    OmniModeOn        = 125, ///< Omni Mode On  + All Notes Off
    PolyModeOnOff     = 126, ///< Poly Mode On/Off + All Sounds Off
    PolyModeOn        = 127, ///< Poly Mode On

    /*
    M-Audio Oxygen 8 Knobs
    ----------------------------------------------------------------
    1 | CC 02 | Breath Controller
    2 | CC 10 | Pan
    3 | CC 81 | General Purpose 6
    4 | CC 91 | Effects 1 Depth, usually controls reverb send amount
    5 | CC 16 | General Purpose 1
    6 | CC 80 | General Purpose 5
    7 | CC 71 | Sound Controller 2, Timbre/Harmonic Content
    8 | CC 74 | Sound Controller 5, Brightness

    Arturia KeyStep 37 Knobs
    ----------------------------------------------------------------
    Bank | Dial | CC #  | Description
    ----------------------------------------------------------------
    1    | 1    | CC 74 | Sound Controller 5, Brightness
    1    | 2    | CC 71 | Sound Controller 2, Timbre
    1    | 3    | CC 76 | Sound Controller 7
    1    | 4    | CC 77 | Sound Controller 8
    2    | 1    | CC 73 | Sound Controller 4, Attack time
    2    | 2    | CC 75 | Sound Controller 6, Decay Time
    2    | 3    | CC 79 | Sound Controller 10
    2    | 4    | CC 72 | Sound Controller 3, Release Time
    3    | 1    | CC 18 | General Purpose Slider 3
    3    | 2    | CC 19 | General Purpose Slider 4
    3    | 3    | CC 16 | General Purpose Slider 1
    3    | 4    | CC 17 | General Purpose Slider 2
    4    | 1    | CC 80 | General Purpose Switch 1
    4    | 2    | CC 81 | General Purpose Switch 2
    4    | 3    | CC 82 | General Purpose Switch 3
    4    | 4    | CC 83 | General Purpose Switch 4
    */
};


//==============================================================================
/**
 * How PSG parameters travel as MIDI controllers: the one description of that wire, for everything that
 * writes or reads it. A chip takes four consecutive MIDI channels from its base channel: A, B, C, then one
 * for the envelope and the noise.
 */
namespace PsgCC {

inline constexpr int numChannels = 4;

/** A controller message on a channel counted from the chip's base channel. */
struct Message {
    int channel;
    MidiCCType controller;
    int value;
};

/** The one to three messages that carry a parameter. */
struct Messages {
    std::array<Message, 3> items {};
    size_t count = 0;

    const Message* begin() const noexcept { return items.data(); }
    const Message* end() const noexcept { return items.data() + count; }
};

/** A period goes as two controllers, 7 high bits and 7 low bits, and the envelope period's two bits above
    those in a third, sent first; anything else as its raw value in one.
    The envelope retrigger goes pressed and released, and as nothing when it is 0. */
Messages encode(PsgParamType param, int value) noexcept;

/** Stores what a controller message carries into `into`, which holds the chip's values from before.
    Another envelope shape than `into` holds sets the envelope retrigger as well: it cannot sound without
    a write of the register. Returns false if no parameter travels there. */
bool decode(int channel, int controller, int value, PsgParamFrameData& into) noexcept;

}  // namespace PsgCC

//==============================================================================
/**
 * Writes PSG parameters to MIDI messages
 */
class PsgParamsMidiWriter {
public:
    PsgParamsMidiWriter(int chan) noexcept
        : channelNumber(chan)
    {}

    /** Writes the parameters `data` sets. */
    void write(double time, const PsgParamFrameData& data);

    /** Writes every value of `data`, set or not, as a clip's first frame restates what the chip should
        hold, and the retriggers it sets. */
    void writeAll(double time, const PsgParamFrameData& data);

    juce::MidiMessageSequence& getSequence() noexcept { return sequence; }

private:
    void add(double time, const PsgCC::Messages& messages);

    int channelNumber = 1;
    juce::MidiMessageSequence sequence {};
};

//==============================================================================
/**
    Writes the parameters a chip should hold as controllers, sending those that differ from what it was
    last sent. A retrigger, of a tone or of the envelope, is an event, not a value to hold: it is sent
    every time it is set.

    It alone remembers what was sent, so whatever makes the parameters keeps no MIDI state.
*/
class PsgEncoder {
public:
    /** Adds to `out`, at `time`, the messages that bring the chip on `baseChannel` to `params`. */
    void encode(const PsgParamFrameData& params, tracktion::MidiMessageArray& out, double time, int baseChannel);

    /** Forgets what was sent, so the next parameters go out whole: for a chip that may not hold them any more. */
    void reset() noexcept { sent_ = {}; }

    /** True if the chip was sent anything since the last reset. */
    bool hasSent() const noexcept { return !sent_.isEmpty(); }

private:
    PsgParamFrameData sent_;
};

}  // namespace MoTool
