#pragma once

#include <JuceHeader.h>

namespace MoTool {

/*
    VolumeA,
    VolumeB,
    VolumeC,
    TonePitchA,
    TonePitchB,
    TonePitchC,
    ToneIsOnA,
    ToneIsOnB,
    ToneIsOnC,
    NoiseIsOnA,
    NoiseIsOnB,
    NoiseIsOnC,
    EnvelopeIsOnA,
    EnvelopeIsOnB,
    EnvelopeIsOnC,
    RetriggerA,
    RetriggerB,
    RetriggerC,
    NoisePitch,
    EnvelopePitch,
    EnvelopeShape,
*/

namespace IDs {
    #define DECLARE_ID(name)  const juce::Identifier name(#name);

    DECLARE_ID(PSGTRACK)
    DECLARE_ID(PSGCLIP)
    DECLARE_ID(PSG)
    DECLARE_ID(frameRate)  // PSG list machine frame rate (Hz), anchors frame indices
    DECLARE_ID(framesPerBeat) // PSG frames per beat at the current project tempo
    DECLARE_ID(FRAME)
    DECLARE_ID(i)          // integer machine-frame index — the canonical frame timestamp
    DECLARE_ID(va)
    DECLARE_ID(vb)
    DECLARE_ID(vc)
    DECLARE_ID(pa)
    DECLARE_ID(pb)
    DECLARE_ID(pc)
    DECLARE_ID(ta)
    DECLARE_ID(tb)
    DECLARE_ID(tc)
    DECLARE_ID(na)
    DECLARE_ID(nb)
    DECLARE_ID(nc)
    DECLARE_ID(ea)
    DECLARE_ID(eb)
    DECLARE_ID(ec)
    DECLARE_ID(ra)
    DECLARE_ID(rb)
    DECLARE_ID(rc)
    DECLARE_ID(re)
    DECLARE_ID(n)
    DECLARE_ID(e)
    DECLARE_ID(s)
    DECLARE_ID(paintMode)
    DECLARE_ID(clipTuningTable) // user-chosen BuiltinTuningType name for a PSG clip; absent = detected
    DECLARE_ID(clipTuningA4)    // user-chosen fitted tuning's A4 (Hz at clipTuningClock) for a PSG clip
    DECLARE_ID(clipTuningClock) // chip clock (Hz) of clipTuningA4; absent = the ZX clock
    DECLARE_ID(clipTuningSystem) // TuningSystemType name of clipTuningA4; absent = equal temperament
    DECLARE_ID(clipTuningTonic) // just-intonation tonic (0 = C .. 11 = B) of clipTuningA4; absent = C
    DECLARE_ID(clipKeyTonic)    // user-chosen key tonic (0 = C .. 11 = B) for a PSG clip; absent = detected
    DECLARE_ID(clipKeyMinor)    // user-chosen key mode for a PSG clip (true = minor)
    DECLARE_ID(psgTune)         // id shared by PSG clips imported as one tune (Turbo Sound chips); absent = alone
    DECLARE_ID(TIMERS)          // a PSG clip's timers (ClipTimers): the bank, and a TIMER for each command
    DECLARE_ID(TIMER)
    DECLARE_ID(bank)            // TIMERS: the bank's bytes, a TAYM
    DECLARE_ID(chip)            // TIMERS: the chip in the bank whose timers these are
    DECLARE_ID(timer)           // TIMER: the chip's timer the command is for; its frame is `i`
    DECLARE_ID(cmd)             // TIMER: TAYM's command: 1 start, 2 modulate, 3 stop
    DECLARE_ID(base)            // TIMER: base timer value
    DECLARE_ID(lane)            // TIMER: timer lane index; -1 none, -2 unchanged
    DECLARE_ID(first)           // TIMER: first action
    DECLARE_ID(count)           // TIMER: number of actions
    DECLARE_ID(projectFps)
    DECLARE_ID(timecodeDisplayMode) // TimecodeDisplayMode of the timeline ruler/grid
    DECLARE_ID(gridSubdivision) // space-separated frame counts per subdivision group within a beat

    #undef DECLARE_ID
}  // namespace IDs



}  // namespace MoTool
