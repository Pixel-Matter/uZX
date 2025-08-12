#include "MPEEffectVoice.h"

namespace MoTool::uZX {

MPEEffectVoice::MPEEffectVoice() {}

MPEEffectVoice::~MPEEffectVoice() {}

//==============================================================================
bool MPEEffectVoice::isCurrentlyPlayingNote(MPENote note) const noexcept {
    return isActive() && currentlyPlayingNote.noteID == note.noteID;
}

bool MPEEffectVoice::isPlayingButReleased() const noexcept {
    return isActive() && currentlyPlayingNote.keyState == MPENote::off;
}

void MPEEffectVoice::clearCurrentNote() noexcept {
    currentlyPlayingNote = MPENote();
}

}  // namespace MoTool::uZX
