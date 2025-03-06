#include <JuceHeader.h>

#include "PsgMidi.h"


using namespace tracktion;

namespace MoTool {

//==============================================================================

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
        if (type >= 20 && type < 34) {
            seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber, type, value >> 4), time);       // Add the coarse value
            // DBG("Add coarse " << type - 20 << ", " << (value >> 4));
            seq.addEvent(juce::MidiMessage::controllerEvent(channelNumber, type + 20, value & 15), time);  // Add the fine value
            // DBG("Add fine " << type - 20 << ", " << (value & 15));
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

}  // namespace MoTool