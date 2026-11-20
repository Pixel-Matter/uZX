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

/** The ownership and sounding voices at one 50 Hz tick. An owner ID of 0 means unassigned. */
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

}  // namespace MoTool::uZX

/** One engine per module instance. All audio and trace buffers are owned by the engine. */
extern "C" {

/** Configures the output and resets the clock. Pass 0 for AY or 1 for YM. Returns 1 on success. */
int uzx_init(std::uint32_t sampleRate, int isYm) noexcept;

/** Silences the chip, clears notes and trace, and resets the clock to sample 0. */
void uzx_reset() noexcept;

/** Opens a gate at the current sample. ID 0 is reserved. Instrument 0 is bass and 1 is pluck.
    Pitch is on the MIDI note scale, including fractional semitones. Velocity is in 0..1.
    Returns 1 if accepted. A rejected note-on does not create a gate.
*/
int uzx_note_on(std::uint32_t id, std::uint32_t instrument, float pitch, float velocity) noexcept;

/** Releases an occurrence, independent of any other gate for the same pitch. */
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
