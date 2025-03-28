#include <JuceHeader.h>

#include "PsgMidi.h"


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

inline static ValueTree createRegValueTree(te::BeatRange range, int reg, int val) {
    // N.B. Tracktion store controller values in edit's MidiList with extra precision
    // but then rounds them to 7 bits
    return te::createValueTree (
        te::IDs::CONTROL,
        te::IDs::b,     roundTo(range.getStart().inBeats()),
        te::IDs::type,  MIDI_PSG_CC_COARSE_START + reg,
        te::IDs::val,   (val & 255)  // store as is, then break down to 2 times by 4 bits
    );
}

}

void loadMidiListStateFrom(const te::Edit& edit, ValueTree &seqState, const uZX::PsgFile &psgFile) {
    auto &data = psgFile.getData();
    const double frameDurSec = 1.0 / psgFile.getFrameRate();
    for (size_t i = 0; i < data.frames.size(); i++) {
        auto &frame = data.frames[i];
        auto timeSec = psgFile.frameNumToSeconds(i);
        auto startBeat = edit.tempoSequence.toBeats(te::TimePosition::fromSeconds(timeSec));
        auto endBeat = edit.tempoSequence.toBeats(te::TimePosition::fromSeconds(timeSec + frameDurSec));
        // auto startBeat = getContentBeatAtTime(te::TimePosition::fromSeconds(timeSec));
        // auto endBeat = getContentBeatAtTime(te::TimePosition::fromSeconds(timeSec + 1.0 / frameRate));
        // DBG("Frame " << i << " time=" << timeBeat);
        for (size_t j = 0; j < frame.registers.size(); j++) {
            if (frame.mask[j]) {
                // DBG("Register " << j << " = " << reg);
                auto regVal = frame.registers[j];
                // NOTE It is too slow to call seq.addControllerEvent
                auto v = createRegValueTree({startBeat, endBeat}, static_cast<int>(j), regVal);
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
        return;
    }
    // No other controller types should be present in PSG fake MIDI
    return;
}

juce::MidiMessageSequence createPsgPlaybackMidiSequence(const MidiList& list, const MidiClip& clip, MidiList::TimeBase timeBase, bool /*generateMPE*/) {
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

    // Then the note events
    // But no notes for PSG

    // Add the SysEx events:
    // But no SysEx for PSG

    return destSequence;
}

// =============================================================================

PsgMidiCCSequenceReader::MaybeRegPair PsgMidiCCSequenceReader::read(const te::MidiMessageWithSource& m) {
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
        if (20 <= ctrlNum && ctrlNum < 34) {
            // coarse value
            reg = static_cast<size_t>(ctrlNum - 20);
            registers.registers[reg] = static_cast<unsigned char>((val << 4) | registers.registers[reg]);
            registers.mask[reg] = !registers.mask[reg];
            // DBG("register coarse " << reg << ", " << m.getControllerValue() << ", mask " << (regs.mask[reg] ? "on" : "off"));
        } else if (40 <= ctrlNum && ctrlNum < 54) {
            // fine value
            reg = static_cast<size_t>(ctrlNum - 40);
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