#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

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
    DECLARE_ID(FRAME)
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

    #undef DECLARE_ID
}  // namespace IDs



}  // namespace MoTool