#include "InstrumentVoice.h"

namespace uZX {

MPEChipInstrumentVoice::MPEChipInstrumentVoice() {}

MPEChipInstrumentVoice::~MPEChipInstrumentVoice() {}

//==============================================================================
bool MPEChipInstrumentVoice::isCurrentlyPlayingNote(MPENote note) const noexcept {
    return isActive() && currentlyPlayingNote.noteID == note.noteID;
}

bool MPEChipInstrumentVoice::isPlayingButReleased() const noexcept {
    return isActive() && currentlyPlayingNote.keyState == MPENote::off;
}

void MPEChipInstrumentVoice::clearCurrentNote() noexcept {
    currentlyPlayingNote = MPENote();
}

}  // namespace uZX
