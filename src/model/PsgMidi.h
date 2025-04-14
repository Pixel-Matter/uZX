#pragma once

#include <JuceHeader.h>
#include <limits>

#include "../formats/psg/PsgFile.h"
#include "../formats/psg/PsgData.h"
#include "PsgClip.h"
#include "juce_audio_basics/juce_audio_basics.h"

namespace te = tracktion;

namespace MoTool {

//===================================================================
// TODO use ChoiceEnum<> CRTP
enum class MidiCCType {
    BankSelectMSB     = 0,   ///< Bank Select MSB
    ModWheel          = 1,   ///< Modulation Wheel (Coarse)
    Breath            = 2,   ///< Breath controller (Coarse)
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
    GPB1              = 80,  ///< General Purpose Switch #1 (on/off)
    GPB2              = 81,  ///< General Purpose Switch #2 (on/off)
    GPB3              = 82,  ///< General Purpose Switch #3 (on/off)
    GPB4              = 83,  ///< General Purpose Switch #4 (on/off)
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
    2    | 1    | CC 75 | Sound Controller 6
    2    | 1    | CC 79 | Sound Controller 10
    2    | 1    | CC 72 | Sound Controller 3, Release Time
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


//===================================================================

class PsgClip;
class PsgParamFrame;

//==============================================================================

void loadMidiListStateFrom(const te::Edit& edit, ValueTree &seqState, const uZX::PsgData& data);

//==============================================================================
class PsgRegsMidiWriter {
public:
    PsgRegsMidiWriter(int chan) noexcept
        : channelNumber(chan)
    {}

    double getTimeInBase(const te::MidiControllerEvent& controller) const;

    juce::MidiMessageSequence& getSequence() noexcept { return sequence; }
    inline void write(double time, int type, int value);

private:
    int channelNumber = 1;
    juce::MidiMessageSequence sequence {};

};

juce::MidiMessageSequence createPsgPlaybackMidiSequence(const te::MidiList& list, const te::MidiClip& clip, te::MidiList::TimeBase timeBase);

//===================================================================
class PsgRegsMidiSequenceReader {
public:
    struct MaybeRegPair {
        int reg = -1;
        uint8_t value = 0;

        bool isValid() const noexcept { return reg >= 0; }
        operator bool() const noexcept { return isValid(); }
    };

    MaybeRegPair read(const te::MidiMessageWithSource& m);

    void reset() noexcept { registers = {}; }

private:
    uZX::PsgRegsFrame registers = {};
};

//===================================================================
class PsgParamsMidiSequenceWriter {
public:
    PsgParamsMidiSequenceWriter(int chan) noexcept
        : channelNumber(chan)
    {}

    void write(double time, const PsgParamFrameData& data);

    juce::MidiMessageSequence& getSequence() noexcept { return sequence; }

private:
    int channelNumber = 1;
    juce::MidiMessageSequence sequence {};

    inline void addEvent(double time, int psgChan, MidiCCType type, int value);

};

//===================================================================
class PsgParamsMidiReader {
public:
    PsgParamsMidiReader(int chan) noexcept
        : baseChannel(chan)
    {
        params.resetMixer();
    }

    std::optional<PsgParamFrameData> read(const juce::MidiMessage& m);

    PsgParamFrameData& getParams() noexcept { return params; }
    const PsgParamFrameData& getParams() const noexcept { return params; }

    void nextFrame() noexcept;

    void reset() noexcept {
        params.clearAll();
    }

    void setBaseChannel(int chan) noexcept { baseChannel = chan; }

private:
    int baseChannel = 1;
    double currentTimestamp = std::numeric_limits<double>::lowest();
    PsgParamFrameData params {};
};

}  // namespace MoTool