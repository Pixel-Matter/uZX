#include "Midi.h"

namespace MoTool::Util {

MidiMessageSequence readMidi(const std::string& data, int track) {
    auto stream = juce::MemoryInputStream(&data[0], data.length(), false);
    juce::MidiFile midiFile;
    midiFile.readFrom(stream);
    MidiMessageSequence sequence = *midiFile.getTrack(track);
    for (int j = sequence.getNumEvents(); --j >= 0;) {
        auto& m = sequence.getEventPointer(j)->message;
        m.setTimeStamp(m.getTimeStamp() * 0.001);
    }
    return sequence;
}

}  // namespace MoTool::Util
