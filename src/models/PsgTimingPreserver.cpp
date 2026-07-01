#include "PsgTimingPreserver.h"
#include "PsgClip.h"

#include <unordered_set>
#include <vector>

namespace MoTool {

namespace {

/** Visits every PsgClip in the edit exactly once (track clips, their container
    children, and audio-track clip-slot clips). */
template <typename Fn>
void forEachPsgClip(te::Edit& edit, Fn&& fn) {
    std::unordered_set<PsgClip*> seen;

    auto visit = [&] (auto&& self, te::Clip* clip) -> void {
        if (auto psgClip = dynamic_cast<PsgClip*>(clip))
            if (seen.insert(psgClip).second)
                fn(*psgClip);

        if (auto childOwner = dynamic_cast<te::ClipOwner*>(clip))
            for (auto childClip : childOwner->getClips())
                self(self, childClip);
    };

    for (auto t : te::getClipTracks(edit)) {
        for (auto c : t->getClips())
            visit(visit, c);

        if (auto at = dynamic_cast<te::AudioTrack*>(t))
            for (auto slot : at->getClipSlotList().getClipSlots())
                if (auto c = slot->getClip())
                    visit(visit, c);
    }
}

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
        forEachPsgClip(edit, [this] (PsgClip& psgClip) {
            PsgClipTimingSnapshot clipSnapshot;
            clipSnapshot.clip = &psgClip;

            for (auto frame : psgClip.getPsg().getFrames())
                clipSnapshot.frames.push_back({frame, frame->getRawEditTime(psgClip)});

            if (! clipSnapshot.frames.empty())
                clips.push_back(std::move(clipSnapshot));
        });
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

    void scaleFramesPerBeat(double scale) {
        if (scale <= 0.0)
            return;

        auto* um = &edit.getUndoManager();

        for (auto& clipSnapshot : clips) {
            if (clipSnapshot.clip == nullptr)
                continue;

            auto& psg = clipSnapshot.clip->getPsg();
            const auto framesPerBeat = psg.getFramesPerBeat();
            if (framesPerBeat > 0.0)
                psg.setFramesPerBeat(framesPerBeat * scale, um);
        }
    }

private:
    te::Edit& edit;
    std::vector<PsgClipTimingSnapshot> clips;
};

}  // namespace

namespace PsgTiming {

void setTempoBpmPreservingFrames(te::Edit& edit, te::TempoSetting& tempo, double bpm,
                                 bool preserveAbsoluteFrameTimes) {
    std::optional<PsgTimingSnapshot> snapshot;
    double oldBeatLength = 0.0;

    if (preserveAbsoluteFrameTimes) {
        oldBeatLength = tempo.getApproxBeatLength().inSeconds();
        snapshot.emplace(edit);
    }

    if (preserveAbsoluteFrameTimes)
        tempo.set(tempo.getStartBeat(), bpm, tempo.getCurve(), false);
    else
        tempo.setBpm(bpm);

    if (snapshot) {
        const auto newBeatLength = tempo.getApproxBeatLength().inSeconds();
        if (oldBeatLength > 0.0 && newBeatLength > 0.0)
            snapshot->scaleFramesPerBeat(newBeatLength / oldBeatLength);

        // Times were held: the frames' beats are rewritten so they keep their real-time spacing.
        // framesPerBeat is scaled by the beat-length ratio, so effective fps stays put too.
        snapshot->remapPsgFramesToSavedTimes();
    }
}

}  // namespace PsgTiming

}  // namespace MoTool
