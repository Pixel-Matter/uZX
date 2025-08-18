#include "ChipInstrument.h"

namespace MoTool::uZX {

ChipInstrument::ChipInstrument()
    : MPEEffect<ChipInstrumentVoice>()
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

}  // namespace MoTool::uZX