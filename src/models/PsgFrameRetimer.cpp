#include "PsgFrameRetimer.h"
#include "PsgClip.h"

#include <functional>
#include <unordered_set>

namespace MoTool {

namespace {

//==============================================================================
/** Re-derives every PSG frame's beat from its tempo-independent machine-frame
    index, so imported PSG timing stays fixed in wall-clock time after a tempo
    change. Each PsgClip is visited at most once. */
void retimeAllPsgClips(te::Edit& edit) {
    auto* um = &edit.getUndoManager();
    std::unordered_set<PsgClip*> seen;

    std::function<void(te::Clip*)> visit = [&](te::Clip* clip) {
        if (auto psgClip = dynamic_cast<PsgClip*>(clip)) {
            if (seen.insert(psgClip).second) {
                psgClip->getPsg().updateBeatsFromFrameIndices(*psgClip, um);
                psgClip->changed();
            }
        }

        if (auto childOwner = dynamic_cast<te::ClipOwner*>(clip)) {
            for (auto childClip : childOwner->getClips())
                visit(childClip);
        }
    };

    for (auto t : te::getClipTracks(edit)) {
        for (auto c : t->getClips())
            visit(c);

        if (auto at = dynamic_cast<te::AudioTrack*>(t)) {
            for (auto slot : at->getClipSlotList().getClipSlots()) {
                if (auto c = slot->getClip())
                    visit(c);
            }
        }
    }
}

}  // namespace

namespace PsgTiming {

void setTempoBpmRetimingFrames(te::Edit& edit, te::TempoSetting& tempo, double bpm) {
    tempo.setBpm(bpm);
    retimeAllPsgClips(edit);
}

}  // namespace PsgTiming

}  // namespace MoTool
