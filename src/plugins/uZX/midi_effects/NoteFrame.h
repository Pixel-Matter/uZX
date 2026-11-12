#pragma once

#include <JuceHeader.h>

#include <algorithm>
#include <array>
#include <utility>

namespace MoTool::uZX {

//==============================================================================
/**
    The chip-note stream: what an instrument has already played, a frame at a time. Each frame names
    every sounding voice with its pitch and level; gates, velocities and envelopes are gone.
    See docs/design/midi-ay-pipeline.md.
*/

// ponytail: fixed 50 Hz; the project's or track's frame rate when that becomes a setting
inline constexpr double noteFrameRate = 50.0;

/** Semitones a full pitch wheel bends a voice from its key, for everything that writes or reads the stream. */
// TODO as a parameter, for a deeper range, sent downstream as the pitch bend sensitivity RPN
inline constexpr int notePitchBendRange = 2;

/** The controllers that carry a voice's timbre, and the envelope's restart, on the voice's channel. */
enum class NoteTimbreCC
{
    NoisePeriod = 2,
    EnvelopeShape = 70,
    Tone = 80,
    Noise = 81,
    Envelope = 82,
    EnvelopeRestart = 83,  ///< an event, for any value but 0
};

/** How a voice sounds on the chip. */
struct NoteTimbre
{
    bool tone = true;       ///< the square wave sounds
    bool noise = false;     ///< the noise sounds
    bool envelope = false;  ///< the chip's envelope shapes the level
    int envelopeShape = 0;  ///< the chip's envelope shape, 0..15, from the voice that plays the envelope
    int noisePeriod = 0;    ///< the chip's noise period, 0..31, from the voice that plays the envelope
};

/** One voice sounding in a frame. */
struct NoteVoice
{
    int channel = 1;      ///< MIDI channel
    int key = 0;          ///< the MIDI note it started as; with the channel, which voice this is
    double pitch = 0.0;   ///< what sounds, in semitones on the MIDI note scale
    float level = 0.0f;   ///< 0..1
    bool attack = false;  ///< starts in this frame, even if the same voice sounded in the frame before
    bool restartEnvelope = false;  ///< restarts the chip's envelope in this frame, if it plays the envelope
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
    Writes frames as MIDI: a note on at the level a voice starts at, its level after that as polyphonic
    aftertouch, its pitch as the pitch wheel of its channel, a note off when it is gone.

    It alone remembers what was sent, so the logic that makes the frames keeps no MIDI state.
*/
class NoteEncoder
{
public:
    /** Adds to `out`, at `time`, the messages that turn the last frame sent into `frame`. */
    void encode(const NoteFrame& frame, tracktion::MidiMessageArray& out, double time);

private:
    NoteFrame sent_;
};

//==============================================================================
/**
    Reads the stream back into frames: the notes that sound, each with the pitch wheel and the timbre
    controllers of its channel. A frame is wherever the stream has messages, so a reader takes one after
    the messages of each time.

    It alone remembers which notes are held and what the controllers were last set to.

    ponytail: the timbre it reads comes from clips and keyboards only. NoteEncoder writes none, because
    nothing that makes frames sets it yet; when the instrument or an effect does, the encoder sends these
    controllers and a frame's timbre survives the trip like its pitch and level.
*/
class NoteDecoder
{
public:
    /** Takes a message of the stream. Returns false for one that is no part of it. */
    bool decode(const juce::MidiMessage& m) noexcept;

    /** The voices sounding now, oldest first. A voice has `attack` set in the first frame taken after it started.

        A voice has `restartEnvelope` set in the first frame it is in after its channel got the restart controller
        or another envelope shape, which cannot sound without a restart. A note starting does not set it: the
        envelope's pitch can move, and a note last, with the envelope running on.
    */
    NoteFrame takeFrame() noexcept;

    /** Forgets the notes and the controllers, as an all-notes-off or a transport jump asks. */
    void reset() noexcept { *this = {}; }

private:
    struct Channel
    {
        double bend = 0.0;  // semitones
        NoteTimbre timbre{};
        bool restartEnvelope = false;  // asked for, and no voice of the channel has taken it yet
    };

    std::array<Channel, 16> channels_{};
    NoteFrame held_;  // without pitch and timbre, which a frame gets from its channel as it is taken
};

}  // namespace MoTool::uZX
