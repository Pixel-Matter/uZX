#pragma once

#include <JuceHeader.h>

#include <array>
#include <memory>

#include "../../../models/PsgCC.h"
#include "../../../models/PsgRecorder.h"
#include "../../../models/tuning/TuningSystemBase.h"
#include "../midi_effects/MidiEffect.h"
#include "../midi_effects/NoteFrame.h"

namespace MoTool::uZX {

using namespace MoTool;

//==============================================================================
/**
    The chip mixer's logic: the parameters the chip needs to play a frame of voices. The voices that fit take
    the chip's channels and generators, the tuning turns pitch into periods, timbre becomes the mixer and
    envelope switches. A channel without a voice is silenced. See docs/design/midi-ay-pipeline.md.

    Who plays, the higher priority first and then the later voice in the frame:
    - a chip channel: the first three voices with tone, noise or envelope on. Which one: the channel that
      sounds nearest to the voice's pan, if it has one and no better voice asked for the same; else the channel
      it had, since moving is heard in stereo; else the lowest free one.
    - the envelope: the first voice whose timbre says its pitch drives the envelope. It gives the period, the
      shape and the restart, and takes no channel unless it sounds on one as well.
    - the noise: the first voice with the noise on among those that got a channel. It gives the noise period.
*/
class ChipMixer
{
public:
    static constexpr int numToneChannels = 3;

    PsgParamFrameData mix(const NoteFrame& frame, const TuningSystem& tuning) noexcept;

    /** Where the chip's channels A, B and C sound, 0 left .. 1 right, which is what a voice's pan is matched to. */
    void setChannelPans(const std::array<double, numToneChannels>& pans) noexcept { pans_ = pans; }

private:
    /** Which voice plays each of the chip's channels and generators in a frame; none where it is null. */
    struct Voices
    {
        std::array<const NoteVoice*, numToneChannels> channels{};
        const NoteVoice* envelope = nullptr;
        const NoteVoice* noise = nullptr;
    };

    struct VoiceId
    {
        int channel = 0;  // 0 for none
        int key = 0;
        bool operator==(const VoiceId&) const = default;
    };

    Voices allocate(const NoteFrame& frame) noexcept;
    static PsgParamFrameData render(const Voices& voices, const TuningSystem& tuning) noexcept;

    std::array<VoiceId, numToneChannels> held_{};  // the voice on each chip channel in the last frame
    std::array<double, numToneChannels> pans_{0.0, 0.5, 1.0};
};

//==============================================================================
/**
    The chip mixer as a MIDI effect. It reads the chip-note stream on every MIDI channel and writes PSG
    controllers on four channels from its base channel. The note domain ends here: nothing that came in on a
    channel goes further.

    All MIDI state is in the decoder and the encoder. A frame is wherever the input has messages.
*/
class NotesToPsgMapper
{
public:
    NotesToPsgMapper();

    // Configuration
    TuningSystem& getTuningSystem();
    void setTuningSystem(std::shared_ptr<TuningSystem> tuning);

    /** Where the chip after the mixer puts its channels A, B and C, 0 left .. 1 right. */
    void setChannelPans(const std::array<double, ChipMixer::numToneChannels>& pans);

    /** Moves the PSG output to the four channels from `channel`, silencing the chip on the ones it leaves. */
    void setBaseChannel(int channel);

    /** Gives the PSG it writes from now on to a recorder as well, at each message's time from the start of
        the edit; null stops that. The caller keeps the recorder alive until it has set another or none. */
    void setRecorder(PsgRecorder* recorder);

    /** Forgets the held notes. The next block starts by silencing the chip, and what follows is sent whole. */
    void reset();

    // MIDI fx processor callback
    void operator()(MidiBufferContext& c);

private:
    void render(tracktion::MidiMessageArray& out, double time);

    NoteDecoder decoder_;
    ChipMixer mixer_;
    PsgEncoder encoder_;
    int baseChannel_ = 1;
    tracktion::MidiMessageArray pending_;  // what a reset left for the start of the next block
    PsgRecorder* recorder_ = nullptr;

    std::shared_ptr<TuningSystem> tuningSystem_{};

    // Configuration runs on the message thread, operator() on the audio thread; both hold this, so a replaced
    // tuning is destroyed by the thread replacing it, never under a rendering voice.
    // ponytail: the audio thread can wait out a setter, as in AYChipPlugin; measured (2026-11) as a few microseconds
    // at thousands of changes a second, so a block-boundary handoff only if a glitch is ever heard
    juce::CriticalSection lock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NotesToPsgMapper)
};

}  // namespace MoTool::uZX
