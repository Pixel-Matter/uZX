#include "NoteFrame.h"

namespace MoTool::uZX {

void NoteEncoder::encode(const NoteFrame& frame, tracktion::MidiMessageArray& out, double time) {
    for (const auto& old : sent_) {
        const auto* now = frame.find(old.channel, old.key);
        if (now == nullptr || now->attack)
            out.addMidiMessage(MidiMessage::noteOff(old.channel, old.key, 0.0f), time, 0);
    }

    for (const auto& voice : frame) {
        const auto* last = voice.attack ? nullptr : sent_.find(voice.channel, voice.key);

        if (last == nullptr || ! approximatelyEqual(last->pitch, voice.pitch)) {
            const auto bend = (voice.pitch - voice.key) / notePitchBendRange;
            out.addMidiMessage(MidiMessage::pitchWheel(voice.channel, jlimit(0, 16383, 8192 + roundToInt(bend * 8191.0))),
                               time, 0);
        }

        if (last == nullptr)
            out.addMidiMessage(MidiMessage::noteOn(voice.channel, voice.key, voice.level), time, 0);
        else if (! approximatelyEqual(last->level, voice.level))
            out.addMidiMessage(MidiMessage::aftertouchChange(voice.channel, voice.key, roundToInt(voice.level * 127.0f)),
                               time, 0);
    }

    sent_ = frame;
}

}  // namespace MoTool::uZX
