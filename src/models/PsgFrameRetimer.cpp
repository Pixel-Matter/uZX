#include "PsgFrameRetimer.h"
#include "PsgClip.h"
#include "EditUtilities.h"

#include <unordered_set>

namespace MoTool {

namespace {

template <typename Fn>
void forEachPsgClip(te::Edit& edit, Fn&& fn) {
    std::unordered_set<PsgClip*> seen;

    auto visit = [&] (auto&& self, te::Clip* clip) -> void {
        if (auto* psgClip = dynamic_cast<PsgClip*>(clip))
            if (seen.insert(psgClip).second)
                fn(*psgClip);

        if (auto* childOwner = dynamic_cast<te::ClipOwner*>(clip))
            for (auto* childClip : childOwner->getClips())
                self(self, childClip);
    };

    for (auto track : te::getClipTracks(edit)) {
        for (auto clip : track->getClips())
            visit(visit, clip);

        if (auto* audioTrack = dynamic_cast<te::AudioTrack*>(track))
            for (auto slot : audioTrack->getClipSlotList().getClipSlots())
                if (auto clip = slot->getClip())
                    visit(visit, clip);
    }
}

void scalePsgFramesPerBeat(te::Edit& edit, double scale) {
    if (scale <= 0.0)
        return;

    auto* um = &edit.getUndoManager();

    forEachPsgClip(edit, [um, scale] (PsgClip& clip) {
        auto& psg = clip.getPsg();
        const auto framesPerBeat = psg.getFramesPerBeat();
        if (framesPerBeat > 0.0) {
            psg.setFramesPerBeat(framesPerBeat * scale, um);
            clip.changed();
        }
    });
}

te::TempoSetting& tempoAtTransport(te::Edit& edit) {
    return edit.tempoSequence.getTempoAt(edit.getTransport().getPosition());
}

double beatSecondsAt(te::Edit& edit, double bpm) {
    return 240.0 / (bpm * tempoAtTransport(edit).getMatchingTimeSig().denominator);
}

double bpmForBeatSeconds(te::Edit& edit, double seconds) {
    return 240.0 / (seconds * tempoAtTransport(edit).getMatchingTimeSig().denominator);
}

}  // namespace

namespace PsgTiming {

void setTempoBpmRetimingFrames(te::Edit& edit, te::TempoSetting& tempo, double bpm,
                               bool preserveAbsoluteFrameTimes) {
    const auto oldBeatLength = preserveAbsoluteFrameTimes ? tempo.getApproxBeatLength().inSeconds() : 0.0;

    if (preserveAbsoluteFrameTimes)
        tempo.set(tempo.getStartBeat(), bpm, tempo.getCurve(), false);
    else
        tempo.setBpm(bpm);

    if (preserveAbsoluteFrameTimes) {
        const auto newBeatLength = tempo.getApproxBeatLength().inSeconds();
        if (oldBeatLength > 0.0 && newBeatLength > 0.0)
            scalePsgFramesPerBeat(edit, newBeatLength / oldBeatLength);
    }
}

double getFramesPerBeat(te::Edit& edit) {
    return Helpers::getProjectFps(edit) * beatSecondsAt(edit, tempoAtTransport(edit).getBpm());
}

void setFramesPerBeat(te::Edit& edit, int framesPerBeat, bool preserveAbsoluteFrameTimes) {
    jassert(framesPerBeat > 0);
    const auto bpm = bpmForBeatSeconds(edit, framesPerBeat / Helpers::getProjectFps(edit));
    setTempoBpmRetimingFrames(edit, tempoAtTransport(edit), jlimit(te::TempoSetting::minBPM, te::TempoSetting::maxBPM, bpm),
                              preserveAbsoluteFrameTimes);
}

double setBpmSnappedToFrames(te::Edit& edit, double bpm, bool preserveAbsoluteFrameTimes) {
    const auto fps = Helpers::getProjectFps(edit);
    const auto snappedBpm = bpmForBeatSeconds(edit, roundToInt(fps * beatSecondsAt(edit, bpm)) / fps);
    setTempoBpmRetimingFrames(edit, tempoAtTransport(edit), snappedBpm, preserveAbsoluteFrameTimes);
    return snappedBpm;
}

}  // namespace PsgTiming

}  // namespace MoTool
