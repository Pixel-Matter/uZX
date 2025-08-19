#include "ChipInstrument.h"
#include "tracktion_engine/tracktion_engine.h"

namespace MoTool::uZX {

namespace te = tracktion;

ChipInstrument::ChipInstrument(te::Edit& e)
    : MPEEffect<ChipInstrumentVoice>()
    , edit(e)
{
    mpeInstrument.enableLegacyMode();
    mpeInstrument.setPitchbendTrackingMode(juce::MPEInstrument::allNotesOnChannel);
    // voiceManager.setVoiceStealingEnabled(true);
}

ChipInstrument::~ChipInstrument() = default;

void ChipInstrument::reset() {
    currentTempo = 120.0f;  // Reset to default tempo
    mpeInstrument.releaseAllNotes();
    // voiceManager.reset();
    // TODO see 4OSC code
}

CriticalSection& ChipInstrument::getVoiceLock() {
    return voiceLock;
}

double ChipInstrument::getTailLength() const {
    // TODO
    return 0.0;
}

void ChipInstrument::setCurrentTempo(float newTempo) {
    currentTempo = newTempo;
}

void ChipInstrument::restoreStateFromValueTree(const ValueTree& state) {
    // TODO: Implement state restoration from ValueTree
}

void ChipInstrument::renderFrame(te::MidiMessageArray& midiBuffer, int startSample, int numSamples) {
}

void ChipInstrument::applyToBuffer(te::MidiMessageArray& midiBuffer, int startSample, int numSamples, double midiOffset) {
    te::MidiMessageArray midiOut;

    // TODO store edit reference or edit.state[IDs::fps] (custom property)
    // TODO slice buffer according to current edit FPS or fixed instrument FPS
    // TODO how to pass start, number and offset, in double seconds or in int samples?

    // Examples
    // [0  ...881][882...1761]  len of frame if sampleRate = 44100
    //
    // [0.0...0.01997732426][0.02...0.03997732426]  len of frame in seconds
    jassert(midiOffset == 0.0);

    int pos = startSample;
    int todo = numSamples;
    // constexpr int fps = 50;
    // int frameSize = sampleRate / fps;
    int frameSize = 882;  // TODO remove hardcoded const
    auto midiIt = midiBuffer.begin();

    while (todo > 0) {
        int thisBlock = std::min(frameSize, todo);

        // process MIDI messages from timestamp pos untill pos + thisBlock
        while (midiIt != midiBuffer.end() && midiIt->getTimeStamp() < pos + thisBlock) {
            DBG("MIDI Event: " << midiIt->getDescription() << " at " << midiIt->getTimeStamp() * 48000 << " < " << pos + thisBlock);
            handleMidiEvent(*midiIt);
            ++midiIt;
        }

        renderFrame(midiOut, pos, thisBlock);

        todo -= thisBlock;
        pos += thisBlock;
    }

    // for (auto& m : midiBuffer) {
    //     DBG("m.ts=" << m.getTimeStamp());
    //     handleMidiEvent(m);
    //     // TODO where to get messages for midiOut?
    // }
    // midiOut.sortByTimestamp();
    // midiBuffer.swapWith(midiOut);
}

}  // namespace MoTool::uZX