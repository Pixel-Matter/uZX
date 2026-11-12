#pragma once

#include <JuceHeader.h>

#include <memory>

#include "../../../models/PsgCC.h"
#include "../../../models/tuning/TuningSystemBase.h"
#include "../midi_effects/MidiEffect.h"
#include "../midi_effects/NoteFrame.h"

namespace MoTool::uZX {

using namespace MoTool;

//==============================================================================
/**
    The chip mixer's logic: the parameters the chip needs to play a frame of voices. Voices take the chip's
    channels, the tuning turns pitch into periods, timbre becomes the mixer and envelope switches.
    A channel without a voice is silenced. See docs/design/midi-ay-pipeline.md.

    ponytail: no allocation. The voice on MIDI channel `baseChannel + n` plays chip channel n, the last one to
    start if there are several, and the voice on `baseChannel + 3` plays the envelope and carries its shape and
    the noise period. To come: voices on any channel assigned by priority, and a timbre field saying the voice's
    pitch drives the envelope, in place of the fourth channel.
*/
PsgParamFrameData mixVoices(const NoteFrame& frame, const TuningSystem& tuning, int baseChannel);

//==============================================================================
/**
    The chip mixer as a MIDI effect. It reads the chip-note stream on four MIDI channels from its base channel
    and writes PSG controllers on the same four; what arrives on other channels passes through.

    All MIDI state is in the decoder and the encoder. A frame is wherever the input has messages.
*/
class NotesToPsgMapper
{
public:
    NotesToPsgMapper();

    // Configuration
    TuningSystem& getTuningSystem();
    void setTuningSystem(std::shared_ptr<TuningSystem> tuning);

    /** Moves the four channels, silencing the chip on the ones it leaves. */
    void setBaseChannel(int channel);

    /** Forgets the held notes. The next block starts by silencing the chip, and what follows is sent whole. */
    void reset();

    // MIDI fx processor callback
    void operator()(MidiBufferContext& c);

private:
    bool isChannelInRange(int channel) const;
    void render(tracktion::MidiMessageArray& out, double time);

    NoteDecoder decoder_;
    PsgEncoder encoder_;
    int baseChannel_ = 1;
    tracktion::MidiMessageArray pending_;  // what a reset left for the start of the next block

    std::shared_ptr<TuningSystem> tuningSystem_{};

    // Configuration runs on the message thread, operator() on the audio thread; both hold this, so a replaced
    // tuning is destroyed by the thread replacing it, never under a rendering voice.
    // ponytail: the audio thread can wait out a setter, as in AYChipPlugin; measured (2026-11) as a few microseconds
    // at thousands of changes a second, so a block-boundary handoff only if a glitch is ever heard
    juce::CriticalSection lock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotesToPsgMapper)
};

}  // namespace MoTool::uZX
