#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../ChipAllocator.h"

namespace MoTool::uZX {

/** One sounding occurrence, including those that did not get a chip channel. */
struct TraceVoice
{
    std::uint32_t id;
    std::uint32_t instrument;
    float pitch;
    float level;
};

/** The ownership and sounding voices at one frame's tick. An owner ID of 0 means unassigned. */
struct TraceSnapshot
{
    std::uint32_t sampleLow;
    std::uint32_t sampleHigh;
    std::uint32_t count = 0;
    std::array<std::uint32_t, ChipAllocator::numToneChannels> channels{};
    std::uint32_t noise = 0;
    std::uint32_t envelope = 0;
    std::uint32_t retrigger = 0;
    std::uint32_t gapBefore = 0;
    std::array<TraceVoice, NoteFrame::maxVoices> voices{};
};

static_assert(sizeof(TraceVoice) == 16 && offsetof(TraceVoice, pitch) == 8);
static_assert(sizeof(TraceSnapshot) == 296 && offsetof(TraceSnapshot, voices) == 40);

/** How one note sounds. Every field is a float, so the page writes the record as uzx_sound_length() floats.
    A switch is on from 0.5. Times are in frames, pitches in semitones.
*/
struct NoteSound
{
    static constexpr std::size_t maxOrnamentSteps = 16;
    static constexpr std::size_t maxTableSteps = 64;

    float tone = 1.0f;             ///< a switch: the square wave sounds
    float noise = 0.0f;            ///< a switch: the noise sounds
    float envelope = 0.0f;         ///< a switch: the chip's envelope shapes the level
    float drivesEnvelope = 0.0f;   ///< a switch: the note's pitch is the pitch of the chip's envelope
    float restartEnvelope = 0.0f;  ///< a switch: the note restarts the envelope it drives when it starts
    float envelopeShape = 0.0f;    ///< 0..7, in EnvelopeShapeEnum's order
    float noisePeriod = 0.0f;      ///< 0..31
    float noiseHold = 0.0f;        ///< the noise sounds only this long from the start; 0 for the whole note
    float priority = 0.0f;         ///< who plays when the chip has too few channels
    float pan = -1.0f;             ///< where it asks to sound, 0 left .. 1 right; anywhere when negative

    float attack = 0.0f;   ///< the level rises from 0 to full
    float decay = 0.0f;    ///< then falls to the sustain level
    float sustain = 1.0f;  ///< 0..1; a note that decays to 0 ends there
    float release = 0.0f;  ///< the level falls to 0 after the gate closes

    float pitchStart = 0.0f;   ///< the pitch offset the note starts at
    float pitchPeak = 0.0f;    ///< the offset at the end of the pitch attack
    float pitchAttack = 0.0f;  ///< from the start offset to the peak
    float pitchDecay = 0.0f;   ///< from the peak to the note's own pitch

    float vibratoRate = 0.0f;   ///< cycles in a frame; 0 for none
    float vibratoDepth = 0.0f;  ///< semitones each way
    float vibratoDelay = 0.0f;  ///< the vibrato starts this long after the note

    float noiseVibratoRate = 0.0f;   ///< cycles in a frame; 0 for none
    float noiseVibratoDepth = 0.0f;  ///< semitones each way from the noise period: 12 up halves the period
    float noiseVibratoDelay = 0.0f;  ///< the noise's vibrato starts this long after the note

    float tremoloRate = 0.0f;   ///< cycles in a frame; 0 for none
    float tremoloDepth = 0.0f;  ///< 0..1: the share of the level that the tremolo takes away at its lowest

    float ornamentStep = 0.0f;    ///< how long each ornament offset lasts; one frame at least
    float ornamentLength = 0.0f;  ///< how many offsets loop; 0 for none

    // A table has a value for each frame of the note, and its last value holds.
    float levelSteps = 0.0f;       ///< how many levels the table has; 0 for attack, decay and sustain instead
    float toneSteps = 0.0f;        ///< how many tones the table has; 0 for the note's own pitch
    float tonesArePeriods = 0.0f;  ///< a switch: the tones are the chip's tone periods, otherwise pitches

    std::array<float, maxOrnamentSteps> ornament{};  ///< pitch offsets
    std::array<float, maxTableSteps> levels{};       ///< 0..1; a note whose table ends at 0 ends there
    std::array<float, maxTableSteps> tones{};        ///< the pitch on the MIDI note scale, or the tone period
};

static_assert(sizeof(NoteSound) == 175 * sizeof(float) && offsetof(NoteSound, ornament) == 31 * sizeof(float));

/** The chip's noise period for a note of this sound that is `age` frames old: its own, moved by its vibrato. */
int noisePeriodAt(const NoteSound& sound, std::uint32_t age) noexcept;

}  // namespace MoTool::uZX

/** One engine per module instance. All audio and trace buffers are owned by the engine. */
extern "C" {

/** Configures the output and resets the clock. Pass 0 for AY or 1 for YM. The frame rate, 1 to 1000 a second, is
    how often the sound changes: a note starts on a frame, and its times are in frames. Returns 1 on success.
*/
int uzx_init(std::uint32_t sampleRate, int isYm, std::uint32_t frameRate) noexcept;

/** Silences the chip, clears notes and trace, and resets the clock to sample 0. */
void uzx_reset() noexcept;

/** The sound of the next note-on. Any note-on, accepted or not, sets it back to the default: a plain tone. */
MoTool::uZX::NoteSound* uzx_note_sound() noexcept;

/** How many floats the sound is, for the page to check that it writes this engine's record. */
std::uint32_t uzx_sound_length() noexcept;

/** Opens a gate at the current sample, sounding as uzx_note_sound() says. ID 0 is reserved.
    The instrument is a tag for the trace. Pitch is on the MIDI note scale, including fractional semitones.
    Velocity is in 0..1. Returns 1 if accepted. A rejected note-on does not create a gate.
*/
int uzx_note_on(std::uint32_t id, std::uint32_t instrument, float pitch, float velocity) noexcept;

/** Closes an occurrence's gate, independent of any other gate for the same pitch. Its release then plays out. */
void uzx_note_off(std::uint32_t id) noexcept;

/** Advances by up to 128 samples. Copy the output buffers before the next render call. */
void uzx_render(std::uint32_t count) noexcept;
const float* uzx_audio_left() noexcept;
const float* uzx_audio_right() noexcept;

/** Returns up to 32 contiguous 296-byte records, oldest first.
    Overflow drops the oldest record and marks the new oldest record's gapBefore flag.
    Samples are relative to the most recent init or reset.
*/
const MoTool::uZX::TraceSnapshot* uzx_trace_data() noexcept;
std::uint32_t uzx_trace_count() noexcept;
void uzx_trace_clear() noexcept;

/** The number of rejected note-on commands since the last reset. */
std::uint32_t uzx_dropped_note_count() noexcept;

}  // extern "C"
