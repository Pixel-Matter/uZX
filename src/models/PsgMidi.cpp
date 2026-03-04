
#include "PsgMidi.h"
#include "PsgClip.h"

using namespace tracktion;

namespace MoTool {

//==============================================================================

inline static constexpr int MIDI_PSG_CC_COARSE_START = 20;
inline static constexpr int MIDI_PSG_CC_FINE_START = 40;

//===============================================================================
// MidiList helpers for PSG loading
//===============================================================================
namespace {

static double roundTo(double value, int decimalPlaces = 3) {
    double factor = std::pow(10.0, decimalPlaces);
    return std::round(value * factor) / factor;
}

inline static ValueTree createRegValueTree(te::BeatPosition pos, int reg, int val) {
    // N.B. Tracktion store controller values in edit's MidiList with extra precision
    // but then rounds them to 7 bits
    return te::createValueTree (
        te::IDs::CONTROL,
        te::IDs::b,     roundTo(pos.inBeats()),
        te::IDs::type,  MIDI_PSG_CC_COARSE_START + reg,
        te::IDs::val,   (val & 255)  // store as is, then break down to 2 times by 4 bits
    );
}

}

void loadMidiListStateFrom(const te::Edit& edit, ValueTree &seqState, const uZX::PsgData &data) {
    for (size_t i = 0; i < data.frames.size(); i++) {
        auto &frame = data.frames[i];
        auto timeSec = data.frameNumToSeconds(i);
        auto startBeat = edit.tempoSequence.toBeats(te::TimePosition::fromSeconds(timeSec));
        // DBG("Frame " << i << " time=" << timeBeat);
        for (size_t j = 0; j < frame.registers.size(); j++) {
            if (frame.mask[j]) {
                // DBG("Register " << j << " = " << reg);
                auto regVal = frame.registers[j];
                // NOTE It is too slow to call seq.addControllerEvent
                auto v = createRegValueTree(startBeat, static_cast<int>(j), regVal);
                seqState.appendChild(std::move(v), nullptr); // no need for um here
            }
        }
    }
}

//===============================================================================
// Add PSG registers controller events to MIDI sequence
//===============================================================================

void PsgRegsMidiWriter::write(double time, int type, int value) {
    jassertfalse;
    if (juce::isPositiveAndBelow(type, 128)) {
        if (type >= MIDI_PSG_CC_COARSE_START && type < MIDI_PSG_CC_COARSE_START + 14) {
            sequence.addEvent(juce::MidiMessage::controllerEvent(channelNumber, type, value >> 4), time);
            // Add the coarse value
            // DBG("Add coarse " << type - MIDI_PSG_CC_START << ", " << (value >> 4));
            sequence.addEvent(juce::MidiMessage::controllerEvent(channelNumber, type - MIDI_PSG_CC_COARSE_START + MIDI_PSG_CC_FINE_START, value & 15), time);  // Add the fine value
            // DBG("Add fine " << type - MIDI_PSG_CC_START << ", " << (value & 15));
        }
    }
}

inline static double getTimeInBase(const MidiControllerEvent& controller, const MidiClip& clip, MidiList::TimeBase tb) {
    switch (tb) {
        case MidiList::TimeBase::beatsRaw:  return controller.getBeatPosition().inBeats();
        case MidiList::TimeBase::beats:     return std::max(0_bp, controller.getEditBeats(clip) - toDuration (clip.getStartBeat())).inBeats();
        case MidiList::TimeBase::seconds:   [[ fallthrough ]];
        default:                           return std::max(0_tp, controller.getEditTime(clip) - toDuration (clip.getPosition().getStart())).inSeconds();
    }
}

juce::MidiMessageSequence createPsgPlaybackMidiSequence(const MidiList& list, const MidiClip& clip, MidiList::TimeBase timeBase) {
    PsgRegsMidiWriter writer {list.getMidiChannel().getChannelNumber()};
    for (auto c : list.getControllerEvents()) {
        writer.write(getTimeInBase(*c, clip, timeBase), c->getType(), c->getControllerValue());
    }
    return writer.getSequence();
}

//=============================================================================
// PsgRegsMidiReader class
//===============================================================================
PsgRegsMidiReader::MaybeRegPair PsgRegsMidiReader::read(const te::MidiMessageWithSource& m) {
    MaybeRegPair result {-1, 0};
    if (m.isController()) {
        const int ctrlNum = m.getControllerNumber();
        const uint8_t val = static_cast<uint8_t>(m.getControllerValue());
        size_t reg = 0;
        if (MIDI_PSG_CC_COARSE_START <= ctrlNum && ctrlNum < MIDI_PSG_CC_COARSE_START + 14) {
            // coarse value
            reg = static_cast<size_t>(ctrlNum - MIDI_PSG_CC_COARSE_START);
            registers.registers[reg] = (registers.registers[reg] & 0x0f) | static_cast<uint8_t>(val << 4);
            registers.mask[reg] = true;
        } else if (MIDI_PSG_CC_FINE_START <= ctrlNum && ctrlNum < MIDI_PSG_CC_FINE_START + 14) {
            // fine value
            reg = static_cast<size_t>(ctrlNum - MIDI_PSG_CC_FINE_START);
            registers.registers[reg] = (registers.registers[reg] & 0xf0) | val;
            registers.mask[reg] = true;
        } else {
            jassertfalse;
        }
        if (registers.mask[reg]) {
            result = {static_cast<int>(reg), registers.registers[reg]};
        }
    }
    return result;
}


//===============================================================================
// PsgParamsMidiWriter class
//===============================================================================
inline void PsgParamsMidiWriter::addEvent(double time, int psgChan, MidiCCType type, int value) {
    sequence.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(type), value), time);
}

void PsgParamsMidiWriter::write(double time, const PsgParamFrameData& data) {
    int psgChan = 0;
    for (auto [type, value] : data.getParams()) {
        switch (type.asEnum()) {
            case PsgParamType::VolumeA:
                psgChan = 0;
                addEvent(time, psgChan, MidiCCType::Volume, value);
                break;
            case PsgParamType::VolumeB:
                psgChan = 1;
                addEvent(time, psgChan, MidiCCType::Volume, value);
                break;
            case PsgParamType::VolumeC:
                psgChan = 2;
                addEvent(time, psgChan, MidiCCType::Volume, value);
                break;
            case PsgParamType::TonePeriodA:
                psgChan = 0;
                // period has 12 bits, coarse 7 bit + fine 5 bits
                addEvent(time, psgChan, MidiCCType::CC20PeriodCoarse, (value >> 7) & 0x7F);
                addEvent(time, psgChan, MidiCCType::CC52PeriodFine,  value & 0x7F);
                break;
            case PsgParamType::TonePeriodB:
                psgChan = 1;
                // period has 12 bits, coarse 7 bit + fine 5 bits
                addEvent(time, psgChan, MidiCCType::CC20PeriodCoarse, (value >> 7) & 0x7F);
                addEvent(time, psgChan, MidiCCType::CC52PeriodFine,  value & 0x7F);
                break;
            case PsgParamType::TonePeriodC:
                psgChan = 2;
                // period has 12 bits, coarse 7 bit + fine 5 bits
                addEvent(time, psgChan, MidiCCType::CC20PeriodCoarse, (value >> 7) & 0x7F);
                addEvent(time, psgChan, MidiCCType::CC52PeriodFine,  value & 0x7F);
                break;
            case PsgParamType::ToneIsOnA:
                psgChan = 0;
                addEvent(time, psgChan, MidiCCType::GPB1ToneSwitch, value);
                break;
            case PsgParamType::ToneIsOnB:
                psgChan = 1;
                addEvent(time, psgChan, MidiCCType::GPB1ToneSwitch, value);
                break;
            case PsgParamType::ToneIsOnC:
                psgChan = 2;
                addEvent(time, psgChan, MidiCCType::GPB1ToneSwitch, value);
                break;
            case PsgParamType::NoiseIsOnA:
                psgChan = 0;
                addEvent(time, psgChan, MidiCCType::GPB2NoiseSwitch, value);
                break;
            case PsgParamType::NoiseIsOnB:
                psgChan = 1;
                addEvent(time, psgChan, MidiCCType::GPB2NoiseSwitch, value);
                break;
            case PsgParamType::NoiseIsOnC:
                psgChan = 2;
                addEvent(time, psgChan, MidiCCType::GPB2NoiseSwitch, value);
                break;
            case PsgParamType::EnvelopeIsOnA:
                psgChan = 0;
                addEvent(time, psgChan, MidiCCType::GPB3EnvSwitch, value);
                break;
            case PsgParamType::EnvelopeIsOnB:
                psgChan = 1;
                addEvent(time, psgChan, MidiCCType::GPB3EnvSwitch, value);
                break;
            case PsgParamType::EnvelopeIsOnC:
                psgChan = 2;
                addEvent(time, psgChan, MidiCCType::GPB3EnvSwitch, value);
                break;
            case PsgParamType::RetriggerToneA:
                psgChan = 0;
                addEvent(time, psgChan, MidiCCType::GPB4RetriggerSwitch, value);
                break;
            case PsgParamType::RetriggerToneB:
                psgChan = 1;
                addEvent(time, psgChan, MidiCCType::GPB4RetriggerSwitch, value);
                break;
            case PsgParamType::RetriggerToneC:
                psgChan = 2;
                addEvent(time, psgChan, MidiCCType::GPB4RetriggerSwitch, value);
                break;
            case PsgParamType::RetriggerEnvelope:
                psgChan = 3;
                addEvent(time, psgChan, MidiCCType::GPB4RetriggerSwitch, value);
                break;
            case PsgParamType::NoisePeriod:
                psgChan = 3;
                addEvent(time, psgChan, MidiCCType::Breath, value);
                break;
            case PsgParamType::EnvelopePeriod:
                psgChan = 3;
                // period can has max 14 bits, coarse 7 bit + fine 7 bits
                addEvent(time, psgChan, MidiCCType::CC20PeriodCoarse, (value >> 7) & 0x7F);
                addEvent(time, psgChan, MidiCCType::CC52PeriodFine,  value & 0x7F);
                break;
            case PsgParamType::EnvelopeShape:
                psgChan = 3;
                addEvent(time, psgChan, MidiCCType::SoundVariation, value);
                break;
            default:
                jassertfalse;
                break;
        }
    }
}

//===============================================================================
// PsgParamsMidiReader class
//===============================================================================
void PsgParamsMidiReader::nextFrame() noexcept {
    params.clear();
}

std::optional<PsgParamFrameData> PsgParamsMidiReader::read(const juce::MidiMessage& m) {
    std::optional<PsgParamFrameData> result {};
    const auto ts = m.getTimeStamp();
    if (ts > currentTimestamp) {
        // now we can emit the previous frame
        if (!params.isEmpty()) {
            result = params;
        }
        nextFrame();
    }
    currentTimestamp = ts;
    const auto channel = m.getChannel();
    int psgChan = channel - baseChannel;
    if (m.isController() && 0 <= psgChan && psgChan <= 3) {
        const auto ctrlNum = static_cast<MidiCCType>(m.getControllerNumber());
        const auto val = static_cast<uint16_t>(m.getControllerValue());
        if (psgChan == 3) {  // Envelope and noise channel
            if (ctrlNum == MidiCCType::CC20PeriodCoarse) { // Envelope coarse
                auto raw = params.getRaw(PsgParamType::EnvelopePeriod);
                // DBG("Envelope period raw: " << raw);
                params.set(PsgParamType::EnvelopePeriod, static_cast<uint16_t>((val << 7) | (raw & 0x7F)));
            } else if (ctrlNum == MidiCCType::CC52PeriodFine) { // Envelope fine
                auto raw = params.getRaw(PsgParamType::EnvelopePeriod);
                params.set(PsgParamType::EnvelopePeriod, static_cast<uint16_t>((raw & 0xFF80) | val));
            } else if (ctrlNum == MidiCCType::Breath) { // Noise period
                params.set(PsgParamType::NoisePeriod, val);
            } else if (ctrlNum == MidiCCType::SoundVariation) { // Envelope shape
                // DBG("Envelope shape: " << val);
                params.set(PsgParamType::EnvelopeShape, val);
            }
        } else {  // Tone channels 0..2
            if (ctrlNum == MidiCCType::Volume) { // Tone volume
                params.set(PsgParamType::VolumeA + psgChan, val);
            } else if (ctrlNum == MidiCCType::CC20PeriodCoarse) { // Tone period coarse
                auto raw = params.getRaw(PsgParamType::TonePeriodA + psgChan);
                params.set(PsgParamType::TonePeriodA + psgChan, static_cast<uint16_t>((val << 7) | (raw & 0x7F)));
            } else if (ctrlNum == MidiCCType::CC52PeriodFine) { // Tone period fine
                auto raw = params.getRaw(PsgParamType::TonePeriodA + psgChan);
                params.set(PsgParamType::TonePeriodA + psgChan, static_cast<uint16_t>((raw & 0xFF80) | val));
            } else if (ctrlNum == MidiCCType::GPB1ToneSwitch) { // Tone on/off
                params.set(PsgParamType::ToneIsOnA + psgChan, val);
            } else if (ctrlNum == MidiCCType::GPB2NoiseSwitch) { // Noise on/off
                params.set(PsgParamType::NoiseIsOnA + psgChan, val);
            } else if (ctrlNum == MidiCCType::GPB3EnvSwitch) { // Envelope on/off
                params.set(PsgParamType::EnvelopeIsOnA + psgChan, val);
            } else if (ctrlNum == MidiCCType::GPB4RetriggerSwitch) { // Retrigger on/off
                params.set(PsgParamType::RetriggerToneA + psgChan, val);
            }
        }
    }
    return result;
}


}  // namespace MoTool