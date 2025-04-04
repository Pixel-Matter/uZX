#include <JuceHeader.h>
#include <sys/types.h>

#include "PsgMidi.h"
#include "PsgClip.h"


using namespace tracktion;

namespace MoTool {

//==============================================================================

inline static constexpr int MIDI_PSG_CC_COARSE_START = 20;
inline static constexpr int MIDI_PSG_CC_FINE_START = 40;

namespace {

static double roundTo(double value, int decimalPlaces = 2) {
    double factor = std::pow(10.0, decimalPlaces);
    return std::round(value * factor) / factor;
}

// inline static ValueTree createRegValueTree(te::BeatPosition pos, int reg, int val) {
//     // N.B. Tracktion store controller values in edit's MidiList with extra precision
//     // but then rounds them to 7 bits
//     return te::createValueTree (
//         te::IDs::CONTROL,
//         te::IDs::b,     roundTo(pos.inBeats()),
//         te::IDs::type,  MIDI_PSG_CC_COARSE_START + reg,
//         te::IDs::val,   (val & 255)  // store as is, then break down to 2 times by 4 bits
//     );
// }

}

// static void loadMidiListStateFrom(const te::Edit& edit, ValueTree &seqState, const uZX::PsgData &data) {
//     for (size_t i = 0; i < data.frames.size(); i++) {
//         auto &frame = data.frames[i];
//         auto timeSec = data.frameNumToSeconds(i);
//         auto startBeat = edit.tempoSequence.toBeats(te::TimePosition::fromSeconds(timeSec));
//         // DBG("Frame " << i << " time=" << timeBeat);
//         for (size_t j = 0; j < frame.registers.size(); j++) {
//             if (frame.mask[j]) {
//                 // DBG("Register " << j << " = " << reg);
//                 auto regVal = frame.registers[j];
//                 // NOTE It is too slow to call seq.addControllerEvent
//                 auto v = createRegValueTree(startBeat, static_cast<int>(j), regVal);
//                 seqState.appendChild(std::move(v), nullptr); // no need for um here
//             }
//         }
//     }
// }

// Implementation based on third_party/tracktion_engine/modules/tracktion_engine/midi/tracktion_MidiList.cpp

static void addToSequence(juce::MidiMessageSequence& seq, const MidiClip& clip, MidiList::TimeBase tb,
                          const MidiControllerEvent& controller, int channelNumber) {
    const auto time = [&] {
        switch (tb) {
            case MidiList::TimeBase::beatsRaw:  return controller.getBeatPosition().inBeats();
            case MidiList::TimeBase::beats:     return std::max(0_bp, controller.getEditBeats(clip) - toDuration (clip.getStartBeat())).inBeats();
            case MidiList::TimeBase::seconds:   [[ fallthrough ]];
            default:                            return std::max(0_tp, controller.getEditTime(clip) - toDuration (clip.getPosition().getStart())).inSeconds();
        }
    }();

    const auto type = controller.getType();
    const auto value = controller.getControllerValue();

    if (juce::isPositiveAndBelow(type, 128)) {
        if (type >= MIDI_PSG_CC_COARSE_START && type < MIDI_PSG_CC_COARSE_START + 14) {
            seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber, type, value >> 4), time);       // Add the coarse value
            // DBG("Add coarse " << type - MIDI_PSG_CC_START << ", " << (value >> 4));
            seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber, type - MIDI_PSG_CC_COARSE_START + MIDI_PSG_CC_FINE_START, value & 15), time);  // Add the fine value
            // DBG("Add fine " << type - MIDI_PSG_CC_START << ", " << (value & 15));
        }
    }
}

//===============================================================================
void PsgParamsMidiWriter::write(double time, const PsgParamFrameData& data) {
    int psgChan = 0;
    for (auto [type, value] : data.getParams()) {
        switch (type) {
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
                addEvent(time, psgChan, MidiCCType::GPB1, value);
                break;
            case PsgParamType::ToneIsOnB:
                psgChan = 1;
                addEvent(time, psgChan, MidiCCType::GPB1, value);
                break;
            case PsgParamType::ToneIsOnC:
                psgChan = 2;
                addEvent(time, psgChan, MidiCCType::GPB1, value);
                break;
            case PsgParamType::NoiseIsOnA:
                psgChan = 0;
                addEvent(time, psgChan, MidiCCType::GPB2, value);
                break;
            case PsgParamType::NoiseIsOnB:
                psgChan = 1;
                addEvent(time, psgChan, MidiCCType::GPB2, value);
                break;
            case PsgParamType::NoiseIsOnC:
                psgChan = 2;
                addEvent(time, psgChan, MidiCCType::GPB2, value);
                break;
            case PsgParamType::EnvelopeIsOnA:
                psgChan = 0;
                addEvent(time, psgChan, MidiCCType::GPB3, value);
                break;
            case PsgParamType::EnvelopeIsOnB:
                psgChan = 1;
                addEvent(time, psgChan, MidiCCType::GPB3, value);
                break;
            case PsgParamType::EnvelopeIsOnC:
                psgChan = 2;
                addEvent(time, psgChan, MidiCCType::GPB3, value);
                break;
            case PsgParamType::RetriggerToneA:
                psgChan = 0;
                addEvent(time, psgChan, MidiCCType::GPB4, value);
                break;
            case PsgParamType::RetriggerToneB:
                psgChan = 1;
                addEvent(time, psgChan, MidiCCType::GPB4, value);
                break;
            case PsgParamType::RetriggerToneC:
                psgChan = 2;
                addEvent(time, psgChan, MidiCCType::GPB4, value);
                break;
            case PsgParamType::RetriggerEnvelope:
                psgChan = 3;
                addEvent(time, psgChan, MidiCCType::GPB4, value);
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

// TODO rewrite as PsgRegsMidiWriter, see above
juce::MidiMessageSequence createPsgPlaybackMidiSequence(const MidiList& list, const MidiClip& clip, MidiList::TimeBase timeBase) {
    using TimeBase = te::MidiList::TimeBase;
    juce::MidiMessageSequence destSequence;

    auto& ts = clip.edit.tempoSequence;
    auto midiStartBeat = clip.getContentStartBeat();
    auto channelNumber = list.getMidiChannel().getChannelNumber();

    // NB: allow extra space here in case the notes get quantised or nudged around later on..
    const auto overlapAllowance = 0.5_bd;
    auto firstNoteBeat = timeBase == TimeBase::beatsRaw ? list.getFirstBeatNumber() - overlapAllowance
                                                        : toPosition (ts.toBeats(clip.getPosition().getStart()) - midiStartBeat - overlapAllowance);
    auto lastNoteBeat  = timeBase == TimeBase::beatsRaw ? list.getLastBeatNumber() + overlapAllowance
                                                        : toPosition (ts.toBeats(clip.getPosition().getEnd())   - midiStartBeat + overlapAllowance);

    jassert(list.getNotes().size() == 0);  // No MIDI notes in PSG fake MIDI, only controllers

    // Do controllers first in case they send and program or bank change messages
    auto& controllerEvents = list.getControllerEvents();

    {
        // Add cumulative controller events that are off the start
        juce::Array<int> doneControllers;

        for (auto e : controllerEvents) {
            auto beat = e->getBeatPosition();

            if (beat < firstNoteBeat) {
                if (!doneControllers.contains(e->getType())) {
                    addToSequence(destSequence, clip, timeBase, *e, channelNumber);
                    doneControllers.add (e->getType());
                }
            }
        }
    }

    // Add the real controller events:
    for (auto e : controllerEvents) {
        auto beat = e->getBeatPosition();

        if (beat >= firstNoteBeat && beat < lastNoteBeat)
            addToSequence(destSequence, clip, timeBase, *e, channelNumber);
    }
    return destSequence;
}


// =============================================================================
PsgRegsMidiSequenceReader::MaybeRegPair PsgRegsMidiSequenceReader::read(const te::MidiMessageWithSource& m) {
    MaybeRegPair result {-1, 0};
    if (m.isController()) {
        const int ctrlNum = m.getControllerNumber();
        const int val = static_cast<unsigned char>(m.getControllerValue());
        size_t reg = 0;
        if (MIDI_PSG_CC_COARSE_START <= ctrlNum && ctrlNum < MIDI_PSG_CC_COARSE_START + 14) {
            // coarse value
            reg = static_cast<size_t>(ctrlNum - MIDI_PSG_CC_COARSE_START);
            registers.registers[reg] = static_cast<unsigned char>((val << 4) | registers.registers[reg]);
            registers.mask[reg] = !registers.mask[reg];
            // DBG("register coarse " << reg << ", " << m.getControllerValue() << ", mask " << (regs.mask[reg] ? "on" : "off"));
        } else if (MIDI_PSG_CC_FINE_START <= ctrlNum && ctrlNum < MIDI_PSG_CC_FINE_START + 14) {
            // fine value
            reg = static_cast<size_t>(ctrlNum - MIDI_PSG_CC_FINE_START);
            registers.registers[reg] = static_cast<unsigned char>(val | registers.registers[reg]);
            registers.mask[reg] = !registers.mask[reg];
            // DBG("register fine " << reg << ", " << m.getControllerValue() << ", mask " << (regs.mask[reg] ? "on" : "off") << " reg is " << regs.registers[reg]);
        } else {
            jassertfalse;
        }
        if (!registers.mask[reg]) {
            result = {static_cast<int>(reg), registers.registers[reg]};
            // DBG("setRegister(" << reg << ", " << regs.registers[reg] << ")");
            registers.registers[reg] = 0;
        }
    }
    return result;
}

}  // namespace MoTool