#pragma once

#include <JuceHeader.h>

#include "PsgParameter.h"

#include <array>
#include <span>
#include <vector>

namespace MoTool {

class ClipTimers;
struct TimerSpan;

/** How loud a channel's volume sounds, 0 to 1: halving every three steps, as the AY's and YM's DACs fall
    in steps of a few dB rather than evenly; in full under the envelope. */
float getPsgVolumeLevel(int volume, bool envelope) noexcept;

/** How loud a timer sounds: as a tone at the loudest volume it writes, in full if it writes none. */
float getTimerLevel(const TimerSpan&) noexcept;

//==============================================================================
/** A pitch sounding in a state: a channel's tone, the envelope's, or a timer's. */
struct PsgSoundingNote
{
    float pitch;                      // normalized to the tone-period scale
    float level;                      // how loud, 0 to 1: a timer's at the loudest volume it writes, 1 if it writes none
    uint8_t channelMask;              // bit 0=A, bit 1=B, bit 2=C: the channels that play it
    bool isEnvelopePitch;
    bool hasEnvMod;                   // a tone whose channel is on the envelope
    const TimerSpan* timer = nullptr; // the timer whose note it is; else the frames'
};

/** What sounds over a stretch of machine frames in which the chip's parameters and the running timers stay
    the same, as PsgClip::forEachSoundingState has them: everything a view of the stretch needs that depends
    neither on its scale nor on how it draws. */
struct PsgSoundingState
{
    int from = 0, to = 0;           // machine frames; `to` is TimerSpan::toTheEnd for the clip's last stretch
    int storedFrame = 0;            // the stored frame whose parameters these are, by its place in the list

    uint8_t envelopeShape = 0;
    bool envelopeRestarts = false;  // the stored frame writes the shape, and this is its first stretch
    /** The one channel on the envelope, unless its tone sounds too at another pitch; -1 with several channels
        on it, or none: whose the envelope's pitch is. */
    int8_t envelopeChannel = -1;
    /** The channels on an envelope too slow to be a pitch, its period past the tone scale's. */
    uint8_t lowEnvelopeChannels = 0;

    uint8_t noisePeriod = 0;
    std::array<float, 3> noiseLevels {};  // how loud each channel's noise is; 0 where it has none
    float noiseLevel = 0.0f;              // the loudest of them

    uint8_t toneChannels = 0;   // the channels with a tone's note
    uint8_t timerChannels = 0;  // the channels with a timer's note
    uint8_t pitchChannels = 0;  // the channels with any note

    uint32_t firstNote = 0, firstBar = 0;
    uint8_t numNotes = 0, numBars = 0;
};

//==============================================================================
/**
    The sounding states of a PSG clip in frame order, each with its notes, highest pitch first, and its
    bars: the running timers that write no pitch.

    Notes at one pitch come tones first, then the timers by number, then the envelope.
*/
class PsgSoundingStates
{
public:
    /** Adds the state of frames `from` to before `to`: `data` is the stored frame's parameters with the
        volumes the timers running then hold, `storedFrame` its place in the list and `startsFrame` whether
        the stretch is its first. */
    void add(int from, int to, int storedFrame, bool startsFrame, const PsgParamFrameData& data, const ClipTimers&);

    void clear() noexcept;
    void reserve(size_t numStates);

    std::span<const PsgSoundingState> getStates() const noexcept { return states_; }
    std::span<const PsgSoundingNote> getNotes(const PsgSoundingState& s) const noexcept {
        return {notes_.data() + s.firstNote, s.numNotes};
    }
    std::span<const TimerSpan* const> getBars(const PsgSoundingState& s) const noexcept {
        return {bars_.data() + s.firstBar, s.numBars};
    }

private:
    std::vector<PsgSoundingState> states_;
    std::vector<PsgSoundingNote> notes_;
    std::vector<const TimerSpan*> bars_;
};

}  // namespace MoTool
