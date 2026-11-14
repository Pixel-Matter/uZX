#pragma once

#include <JuceHeader.h>

#include "TimerBank.h"

#include <taym/taym.h>
#include <taym/taym_timers.h>

#include <array>
#include <limits>
#include <optional>
#include <vector>

namespace MoTool {

class PsgParamFrameData;

//==============================================================================
/** A timer command at a frame of a clip's PSG list. */
struct TimerEvent {
    int frameIndex = 0;
    int timer = 0;      ///< among the chip's timers, as the wire numbers them
    TaymMods command {};
};

//==============================================================================
/**
    What one timer does from one of its commands to its next: a stretch of frames in which it holds the same
    registers and writes them in the same repeating way. What a clip draws and describes of its timers.
*/
struct TimerSpan {
    struct Target {
        uint8_t id = 0;      ///< TAYM's target: a register R0..R13, or 0x80 for a sample amplitude
        int lane = -1;       ///< the value lane it reads, or -1 for one value
        uint32_t value = 0;  ///< that one value
    };

    int timer = 0;
    int startFrame = 0;
    int endFrame = 0;       ///< the frame of the timer's next command; `toTheEnd` if it has none
    bool starts = false;    ///< began by a start; else by a modulate of the running timer

    static constexpr int toTheEnd = std::numeric_limits<int>::max();

    uint32_t base = 0;      ///< the base timer value in effect
    int timerLane = -1;     ///< the timer lane in effect, or -1
    std::array<Target, TAYM_MAX_TIMER_TARGETS> targets {};
    size_t numTargets = 0;

    uint16_t registers = 0; ///< a bit per register R0..R13 it holds
    uint8_t channels = 0;   ///< a bit per channel A, B, C whose volume or tone period it holds
    bool envelope = false;  ///< holds the envelope's shape or period
    bool noise = false;     ///< holds the noise period
    bool mixer = false;     ///< holds the mixer

    /** For each channel whose volume register it holds, that register at its loudest: the largest volume it
        writes, with bit 4 set if a write of it puts the channel on the envelope. */
    std::array<uint8_t, 3> loudestVolumes {};

    /** The chip cycles one repeat of what it writes takes: its values and intervals come round again after
        that long, so it is the period of what is heard. 0 if it is no pitch: a timer lane that ends, no
        rate, or every expiry alike, unless each restarts the envelope. */
    double loopCycles = 0.0;
    /** The part of a repeat spent on the highest volume it writes, or on its first interval when its volumes
        are all one: a pulse's duty. Negative if it writes no volume, or every step is alike. */
    float duty = -1.0f;
    /** The envelope shape it writes most often, if it holds R13; else -1. */
    int envelopeShape = -1;

    /** The pitch of what it writes on the tone scale, normalized as a frame's notes are: a tone of period
        `loopCycles / 16` sounds the same. None if it is no pitch, or past the chip's tone periods. */
    std::optional<float> getPitch() const noexcept;

    /** Puts its volumes into a frame's parameters: what the chip's volume registers hold while it runs. */
    void applyVolumes(PsgParamFrameData&) const;
};

/** Each of the bank's timers' spans in frame order, as the chip's timers take `events`, which are in frame
    order. TAYM's own engine runs the commands, so a command it refuses changes nothing here either. */
std::vector<std::vector<TimerSpan>> findTimerSpans(const TimerBank&, const std::vector<TimerEvent>&);

}  // namespace MoTool
