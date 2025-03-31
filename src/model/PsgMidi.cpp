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

void loadMidiListStateFrom(const te::Edit& edit, ValueTree &seqState, const uZX::PsgFile &psgFile) {
    auto &data = psgFile.getData();
    for (size_t i = 0; i < data.frames.size(); i++) {
        auto &frame = data.frames[i];
        auto timeSec = psgFile.frameNumToSeconds(i);
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

void addToSequence(
    juce::MidiMessageSequence& seq,
    const PsgClip& clip,
    PsgList::TimeBase tb,
    const PsgParamFrame& frame,
    int channelNumber
) {
    const auto time = [&] {
        switch (tb) {
            case PsgList::TimeBase::beatsRaw:  return frame.getBeatPosition().inBeats();
            case PsgList::TimeBase::beats:     return std::max(0_bp, frame.getEditBeats(clip) - toDuration (clip.getStartBeat())).inBeats();
            case PsgList::TimeBase::seconds:   [[ fallthrough ]];
            default:                            return std::max(0_tp, frame.getEditTime(clip) - toDuration (clip.getPosition().getStart())).inSeconds();
        }
    }();

    int psgChan = 0;
    uint8_t valueCoarse = 0;
    uint8_t valueFine = 0;
    for (auto [type, value] : frame.getData().getParams()) {
        switch (type) {
            case PsgParamType::VolumeA:
                psgChan = 0;
            case PsgParamType::VolumeB:
                psgChan = 1;
            case PsgParamType::VolumeC:
                psgChan = 2;
                seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(MidiCCType::Volume), value), time);
                break;
            case PsgParamType::TonePeriodA:
                psgChan = 0;
            case PsgParamType::TonePeriodB:
                psgChan = 1;
            case PsgParamType::TonePeriodC:
                psgChan = 2;
                // period has 12 bits, coarse 7 bit + fine 5 bits
                valueCoarse = (value >> 7) & 0x7F;
                valueFine = value & 0x7F;
                seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(MidiCCType::CC20PeriodCoarse), valueCoarse), time);
                seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(MidiCCType::CC20PeriodFine),   valueFine), time);
                break;
            case PsgParamType::ToneIsOnA:
                psgChan = 0;
            case PsgParamType::ToneIsOnB:
                psgChan = 1;
            case PsgParamType::ToneIsOnC:
                psgChan = 2;
                seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(MidiCCType::GPC5), value), time);
                break;
            case PsgParamType::NoiseIsOnA:
                psgChan = 0;
            case PsgParamType::NoiseIsOnB:
                psgChan = 1;
            case PsgParamType::NoiseIsOnC:
                psgChan = 2;
                seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(MidiCCType::GPC6), value), time);
                break;
            case PsgParamType::EnvelopeIsOnA:
                psgChan = 0;
            case PsgParamType::EnvelopeIsOnB:
                psgChan = 1;
            case PsgParamType::EnvelopeIsOnC:
                psgChan = 2;
                seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(MidiCCType::GPC7), value), time);
                break;
            case PsgParamType::RetriggerA:
                psgChan = 0;
            case PsgParamType::RetriggerB:
                psgChan = 1;
            case PsgParamType::RetriggerC:
                psgChan = 2;
                seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(MidiCCType::GPC8), value), time);
                break;
            case PsgParamType::NoisePeriod:
                psgChan = 3;
                seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(MidiCCType::Breath), value), time);
                break;
            case PsgParamType::EnvelopePeriod:
                psgChan = 3;
                // period can has max 14 bits, coarse 7 bit + fine 7 bits
                valueCoarse = (value >> 7) & 0x7F;
                valueFine = value & 0x7F;
                seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(MidiCCType::CC20PeriodCoarse), valueCoarse), time);
                seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(MidiCCType::CC20PeriodFine),   valueFine), time);
                break;
            case PsgParamType::EnvelopeShape:
                psgChan = 3;
                seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber + psgChan, static_cast<int>(MidiCCType::SoundVariation), value), time);
                break;
            case PsgParamType::SIZE:
            default:
                jassertfalse;
                break;
        }
    }
}

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
PsgRegsMidiCCSequenceReader::MaybeRegPair PsgRegsMidiCCSequenceReader::read(const te::MidiMessageWithSource& m) {
    MaybeRegPair result {-1, 0};
    if (m.isNoteOn()) {
        // note based PSG-MIDI mapping is not implemented yet
        // but can be used with MPE
        // const int note = m.getNoteNumber();
        // const size_t reg = static_cast<size_t>(note - 60);
        // const unsigned char val = m.getVelocity();
        // chip->setRegister(reg, val);
        // DBG("setRegister(" << reg << ", " << val << ")");
    } else if (m.isController()) {
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