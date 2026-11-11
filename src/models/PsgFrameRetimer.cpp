#include "PsgFrameRetimer.h"

#include <unordered_set>

#include "EditUtilities.h"
#include "PsgClip.h"

namespace MoTool {

namespace {

template <typename Fn>
void forEachPsgClip(te::Edit& edit, Fn&& fn) {
    std::unordered_set<PsgClip*> seen;

    auto visit = [&](auto&& self, te::Clip* clip) -> void {
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

    forEachPsgClip(edit, [um, scale](PsgClip& clip) {
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

double beatSecondsAt(te::TempoSetting& tempo, double bpm) {
    return 240.0 / (bpm * tempo.getMatchingTimeSig().denominator);
}

double bpmForBeatSeconds(te::TempoSetting& tempo, double seconds) {
    return 240.0 / (seconds * tempo.getMatchingTimeSig().denominator);
}

}  // namespace

namespace PsgTiming {

void setTempoBpmRetimingFrames(te::Edit& edit, te::TempoSetting& tempo, double bpm, bool preserveAbsoluteFrameTimes) {
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

void setClipFrameRate(PsgClip& clip, double fps) {
    auto& psg = clip.getPsg();
    const auto current = psg.getEffectiveFps(clip);
    if (fps <= 0.0 || current <= 0.0)
        return;

    const auto ratio = fps / current;
    auto* um = &clip.edit.getUndoManager();
    psg.setFramesPerBeat(psg.getFramesPerBeat() * ratio, um);
    psg.setFrameRate(fps, um);
    const auto position = clip.getPosition();
    clip.setPosition({{position.getStart(), te::TimeDuration::fromSeconds(position.getLength().inSeconds() / ratio)},
                      te::TimeDuration::fromSeconds(position.getOffset().inSeconds() / ratio)});
    clip.changed();
}

void setTimeSig(te::Edit& edit, te::TimeSigSetting& timeSig, int numerator, int denominator) {
    jassert(numerator > 0 && denominator > 0);
    const int oldDenominator = timeSig.denominator.get();
    timeSig.numerator = numerator;
    timeSig.denominator = denominator;
    // a beat lasts 240 / (bpm * denominator) seconds
    if (oldDenominator > 0 && denominator != oldDenominator)
        scalePsgFramesPerBeat(edit, (double) oldDenominator / denominator);
}

double getFramesPerBeat(te::Edit& edit) {
    auto& tempo = tempoAtTransport(edit);
    return Helpers::getProjectFps(edit) * beatSecondsAt(tempo, tempo.getBpm());
}

void setFramesPerBeat(te::Edit& edit, te::TempoSetting& tempo, int framesPerBeat, bool preserveAbsoluteFrameTimes) {
    jassert(framesPerBeat > 0);
    const auto bpm = bpmForBeatSeconds(tempo, framesPerBeat / Helpers::getProjectFps(edit));
    setTempoBpmRetimingFrames(
        edit, tempo, jlimit(te::TempoSetting::minBPM, te::TempoSetting::maxBPM, bpm), preserveAbsoluteFrameTimes);
}

void setFramesPerBeat(te::Edit& edit, int framesPerBeat, bool preserveAbsoluteFrameTimes) {
    setFramesPerBeat(edit, tempoAtTransport(edit), framesPerBeat, preserveAbsoluteFrameTimes);
}

double setBpmSnappedToFrames(te::Edit& edit, double bpm, bool preserveAbsoluteFrameTimes) {
    const auto fps = Helpers::getProjectFps(edit);
    auto& tempo = tempoAtTransport(edit);
    const auto snappedBpm = bpmForBeatSeconds(tempo, roundToInt(fps * beatSecondsAt(tempo, bpm)) / fps);
    setTempoBpmRetimingFrames(edit, tempo, snappedBpm, preserveAbsoluteFrameTimes);
    return snappedBpm;
}

}  // namespace PsgTiming

}  // namespace MoTool
