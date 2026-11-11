#pragma once

#include <JuceHeader.h>

#include <array>

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

/** One voice sounding in a frame. */
struct NoteVoice {
    int channel = 1;      ///< MIDI channel
    int key = 0;          ///< the MIDI note it started as; with the channel, which voice this is
    double pitch = 0.0;   ///< what sounds, in semitones on the MIDI note scale
    float level = 0.0f;   ///< 0..1
    bool attack = false;  ///< starts in this frame, even if the same voice sounded in the frame before
};

/** The voices sounding in one frame. */
class NoteFrame {
public:
    static constexpr size_t maxVoices = 16;

    void add(const NoteVoice& voice) noexcept {
        jassert(count_ < maxVoices);
        if (count_ < maxVoices)
            voices_[count_++] = voice;
    }

    const NoteVoice* find(int channel, int key) const noexcept {
        for (const auto& v : *this)
            if (v.channel == channel && v.key == key)
                return &v;
        return nullptr;
    }

    const NoteVoice* begin() const noexcept { return voices_.data(); }
    const NoteVoice* end() const noexcept { return voices_.data() + count_; }

private:
    std::array<NoteVoice, maxVoices> voices_ {};
    size_t count_ = 0;
};

//==============================================================================
/**
    Writes frames as MIDI: a note on at the level a voice starts at, its level after that as polyphonic
    aftertouch, its pitch as the pitch wheel of its channel, a note off when it is gone.

    It alone remembers what was sent, so the logic that makes the frames keeps no MIDI state.
*/
class NoteEncoder {
public:
    /** Adds to `out`, at `time`, the messages that turn the last frame sent into `frame`. */
    void encode(const NoteFrame& frame, tracktion::MidiMessageArray& out, double time);

private:
    NoteFrame sent_;
};

}  // namespace MoTool::uZX
