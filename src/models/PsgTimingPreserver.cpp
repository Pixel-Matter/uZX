#include "PsgTimingPreserver.h"
#include "PsgClip.h"

#include <unordered_set>
#include <vector>

namespace MoTool {

namespace {

struct PsgFrameTimingSnapshot {
    PsgParamFrame* frame = nullptr;
    te::TimePosition editTime;
};

struct PsgClipTimingSnapshot {
    PsgClip* clip = nullptr;
    std::vector<PsgFrameTimingSnapshot> frames;
};

//==============================================================================
/** Records the raw (unquantised) edit time of every PSG frame in the edit, so they
    can be restored after a tempo change. Each PsgClip is visited at most once. */
class PsgTimingSnapshot {
public:
    explicit PsgTimingSnapshot(te::Edit& e)
        : edit(e)
    {
        for (auto t : te::getClipTracks(edit)) {
            for (auto c : t->getClips())
                addClip(c);

            if (auto at = dynamic_cast<te::AudioTrack*>(t)) {
                for (auto slot : at->getClipSlotList().getClipSlots()) {
                    if (auto c = slot->getClip())
                        addClip(c);
                }
            }
        }
    }

    void remapPsgFramesToSavedTimes() {
        auto* um = &edit.getUndoManager();

        for (auto& clipSnapshot : clips) {
            if (clipSnapshot.clip == nullptr)
                continue;

            for (auto& frameSnapshot : clipSnapshot.frames) {
                if (frameSnapshot.frame != nullptr)
                    frameSnapshot.frame->setRawEditTime(*clipSnapshot.clip, frameSnapshot.editTime, um);
            }

            clipSnapshot.clip->changed();
        }
    }

private:
    te::Edit& edit;
    std::vector<PsgClipTimingSnapshot> clips;
    std::unordered_set<PsgClip*> seenClips;

    void addClip(te::Clip* clip) {
        if (auto psgClip = dynamic_cast<PsgClip*>(clip)) {
            if (seenClips.insert(psgClip).second) {
                PsgClipTimingSnapshot clipSnapshot;
                clipSnapshot.clip = psgClip;

                for (auto frame : psgClip->getPsg().getFrames())
                    clipSnapshot.frames.push_back({frame, frame->getRawEditTime(*psgClip)});

                if (!clipSnapshot.frames.empty())
                    clips.push_back(std::move(clipSnapshot));
            }
        }

        if (auto childOwner = dynamic_cast<te::ClipOwner*>(clip)) {
            for (auto childClip : childOwner->getClips())
                addClip(childClip);
        }
    }
};

}  // namespace

namespace PsgTiming {

void setTempoBpmPreservingFrames(te::Edit& edit, te::TempoSetting& tempo, double bpm,
                                 bool preserveAbsoluteFrameTimes) {
    std::optional<PsgTimingSnapshot> snapshot;

    if (preserveAbsoluteFrameTimes)
        snapshot.emplace(edit);

    if (preserveAbsoluteFrameTimes)
        tempo.set(tempo.getStartBeat(), bpm, tempo.getCurve(), false);
    else
        tempo.setBpm(bpm);

    if (snapshot)
        snapshot->remapPsgFramesToSavedTimes();
}

}  // namespace PsgTiming

}  // namespace MoTool
