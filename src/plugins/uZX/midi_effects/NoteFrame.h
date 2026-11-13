#pragma once

#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include "../aychip/EnvelopeShape.h"

namespace MoTool::uZX {

//==============================================================================
/**
    The chip-note stream: what an instrument has already played, a frame at a time. Each frame names
    every sounding voice with its pitch and level; gates, velocities and envelopes are gone.
    See docs/design/midi-ay-pipeline.md.
*/

// ponytail: fixed 50 Hz; the project's or track's frame rate when that becomes a setting
inline constexpr double noteFrameRate = 50.0;

/** Semitones a full pitch wheel bends the voices of a channel from their keys, for everything that writes or
    reads the stream. The stream is an MPE lower zone: 48 on its member channels, 2 to 16, where each voice has
    a channel of its own, and 2 on channel 1, where a keyboard's wheel plays. */
constexpr int notePitchBendRange(int channel) noexcept { return channel == 1 ? 2 : 48; }

/** The controllers of the stream, on a voice's channel: its timbre, the envelope's restart, its place and priority.

    They are read as MIDI reads its own, not as chip numbers: a switch is on from 64, and a value with fewer
    steps than a controller has takes the controller's whole range, an equal part of it for each step.
*/
enum class NoteCC
{
    NoisePeriod = 2,       ///< the 32 noise periods
    Pan = 10,
    EnvelopeShape = 70,    ///< the 8 envelope shapes
    Tone = 80,             ///< a switch
    Noise = 81,            ///< a switch
    Envelope = 82,         ///< a switch
    EnvelopeRestart = 83,  ///< an event, from 64
    EnvelopeDriver = 85,   ///< a switch
    Priority = 86,
};

/** A controller's value for the step `value` of `steps`, and the step a controller's value means. */
constexpr int stepToController(int value, int steps) noexcept { return value * 127 / (steps - 1); }
constexpr int controllerToStep(int controller, int steps) noexcept { return controller * steps / 128; }

inline constexpr int numNoisePeriods = 32;

/** How a voice sounds on the chip. */
struct NoteTimbre
{
    bool tone = true;       ///< the square wave sounds
    bool noise = false;     ///< the noise sounds
    bool envelope = false;  ///< the chip's envelope shapes the level
    bool drivesEnvelope = false;  ///< its pitch is the pitch of the chip's envelope
    EnvelopeShape envelopeShape{};  ///< the chip's envelope shape, from the voice that drives the envelope
    int noisePeriod = 0;    ///< the chip's noise period, 0..31, from a voice with the noise on
};

/** One voice sounding in a frame. */
struct NoteVoice
{
    int channel = 1;      ///< the MIDI channel it was played or read on; with the key, which voice this is
    int key = 0;          ///< the MIDI note it started as
    double pitch = 0.0;   ///< what sounds, in semitones on the MIDI note scale
    float level = 0.0f;   ///< 0..1
    bool attack = false;  ///< starts in this frame, even if the same voice sounded in the frame before
    bool restartEnvelope = false;  ///< restarts the chip's envelope in this frame, if it drives the envelope
    int priority = 0;  ///< who plays when the chip has too few channels: the higher, then the later in the frame
    std::optional<float> pan{};  ///< where it asks to sound, 0 left .. 1 right; anywhere without one
    NoteTimbre timbre{};
};

/** The voices sounding in one frame, in the order they were added. */
class NoteFrame
{
public:
    static constexpr size_t maxVoices = 16;

    /** A full frame drops the voice. */
    void add(const NoteVoice& voice) noexcept {
        if (count_ < maxVoices)
            voices_[count_++] = voice;
    }

    template <typename Predicate>
    void removeIf(Predicate&& matches) noexcept {
        count_ = static_cast<size_t>(std::remove_if(begin(), end(), matches) - begin());
    }

    void remove(int channel, int key) noexcept {
        removeIf([=](const NoteVoice& v) { return v.channel == channel && v.key == key; });
    }

    const NoteVoice* find(int channel, int key) const noexcept {
        for (const auto& v : *this)
            if (v.channel == channel && v.key == key)
                return &v;
        return nullptr;
    }

    NoteVoice* find(int channel, int key) noexcept {
        return const_cast<NoteVoice*>(std::as_const(*this).find(channel, key));
    }

    bool isEmpty() const noexcept { return count_ == 0; }

    const NoteVoice* begin() const noexcept { return voices_.data(); }
    const NoteVoice* end() const noexcept { return voices_.data() + count_; }
    NoteVoice* begin() noexcept { return voices_.data(); }
    NoteVoice* end() noexcept { return voices_.data() + count_; }

private:
    std::array<NoteVoice, maxVoices> voices_{};
    size_t count_ = 0;
};

//==============================================================================
/**
    Writes frames as MIDI, each voice on a MIDI channel of its own, so its pitch wheel and its controllers
    are its alone: the member channels of an MPE lower zone, 2 to 16. A voice keeps its channel while it
    sounds, and a new voice takes the one that has been free the longest. The channel a voice has in a frame
    only says which voice it is; what it meant goes with the voice as its priority.

    A voice starts with a reset of its channel's controllers, the controllers that differ from that, the
    pitch wheel and a note on at the level it starts at. While it sounds its level is its channel's pressure,
    as MPE has it, and pitch and controllers go out as they change. A note off when it is gone.

    It alone remembers what was sent, so the logic that makes the frames keeps no MIDI state.
*/
class NoteEncoder
{
public:
    /** The voices it sends at once. */
    static constexpr int maxVoices = 15;

    /** Adds to `out`, at `time`, the messages that turn the last frame sent into `frame`. */
    void encode(const NoteFrame& frame, tracktion::MidiMessageArray& out, double time);

private:
    static constexpr int firstChannel = 2;

    std::array<std::optional<NoteVoice>, maxVoices> sent_{};  // the voice on each channel, from firstChannel
    size_t next_ = 0;  // where the search for a free channel starts
};

//==============================================================================
/**
    The stream's controllers as they were last set on each MIDI channel, for the voices of that channel:
    timbre, pan, priority and the envelope's restart.
*/
class NoteControllers
{
public:
    /** Takes a controller of the stream; a reset of all controllers returns its channel to tone on, noise and
        envelope off, no pan and the channel's own priority. Returns false for a message that is neither. */
    bool decode(const juce::MidiMessage& m) noexcept;

    /** Gives each voice of a frame what the controllers of its channel say. Its priority is its channel's
        number until the priority controller sets another, and it asks for no place until the pan controller
        names one.

        A voice has `restartEnvelope` set in the first frame it is in after its channel got the restart
        controller. A note starting does not set it: the envelope's pitch can move, and a note last, with the
        envelope running on. Nor does another envelope shape, which is a value here; the chip restarts by itself
        when the shape it is given is another, since that cannot sound without a write.
    */
    void applyTo(NoteFrame& frame) noexcept;

private:
    struct Channel
    {
        NoteTimbre timbre{};
        bool restartEnvelope = false;  // asked for, and no voice of the channel has taken it yet
        int priority = -1;             // the channel's number until the controller sets one
        std::optional<float> pan{};
    };

    std::array<Channel, 16> channels_{};
};

//==============================================================================
/**
    Reads the stream back into frames: the notes that sound, each with the pitch wheel and the controllers
    of its channel. A frame is wherever the stream has messages, so a reader takes one after the messages of
    each time.

    A note starts at its velocity. After the frame it starts in, its level is the pressure of its channel on
    channels 2 to 16, where a voice has a channel of its own, or its polyphonic aftertouch on any channel.

    It alone remembers which notes are held and what the controllers were last set to.

*/
class NoteDecoder
{
public:
    /** Takes a message of the stream. Returns false for one that is no part of it. */
    bool decode(const juce::MidiMessage& m) noexcept;

    /** The voices sounding now, oldest first, each with what NoteControllers gives it. A voice has `attack`
        set in the first frame taken after it started. */
    NoteFrame takeFrame() noexcept;

    /** Forgets the notes and the controllers, as an all-notes-off or a transport jump asks. */
    void reset() noexcept { *this = {}; }

private:
    std::array<double, 16> bends_{};  // semitones, by channel
    NoteControllers controllers_;
    NoteFrame held_;  // without pitch and timbre, which a frame gets from its channel as it is taken
};

}  // namespace MoTool::uZX
