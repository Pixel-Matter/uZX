#pragma once

#include <JuceHeader.h>

#include "PsgCC.h"

#include <taym/taym.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace MoTool {

//==============================================================================
/**
    How a chip's timers travel on the PSG wire: timed register writes between frames, as TAYM has them
    (PWM, sync-buzzer, samples). A timer takes a MIDI channel of its own after the chip's four.

    As everywhere on this wire, what a timer holds is values, and its start is an event: the base value,
    the timer lane and the actions are controllers that hold, a start is controller 83 pressed and released,
    and a value that changes without a start modulates the running timer. A host that states the values
    again where nothing happened restarts nothing. After a reset, which play stopping or jumping is, a timer
    stated as on is started where play resumes, as Tracktion states it when play starts inside a clip: from
    its lanes' first elements, since where it had got to does not travel.

    The lanes and actions themselves do not travel: they are a bank, chosen by number on the chip's fourth
    channel (TimerBankRegistry).
*/
namespace TimerCC {

/** The first timer's channel, counted from the chip's base channel. */
inline constexpr int firstChannel = PsgCC::numChannels;

/** A chip on base channel 1 has the other twelve channels for timers. */
inline constexpr int maxTimers = 16 - firstChannel;

/** What a timer holds. */
struct Values {
    bool on = false;
    uint32_t base = 0;     ///< TAYM's base_timer_value
    int lane = 0;          ///< the timer lane's index + 1; 0 for none
    int actionFirst = 0;   ///< the action slice
    int actionCount = 0;

    bool operator==(const Values&) const = default;
};

/** The largest values the wire carries. */
inline constexpr int maxLane = (1 << 14) - 1;
inline constexpr int maxActionFirst = (1 << 14) - 1;
inline constexpr int maxActionCount = 127;
inline constexpr int maxBank = (1 << 14) - 1;

/** The messages of one timer command, or of a bank choice. */
struct Messages {
    std::array<PsgCC::Message, 13> items {};
    size_t count = 0;

    const PsgCC::Message* begin() const noexcept { return items.data(); }
    const PsgCC::Message* end() const noexcept { return items.data() + count; }
    void add(int channel, MidiCCType controller, int value) noexcept { items[count++] = {channel, controller, value}; }
};

/** Chooses the bank the chip's timers read: 1 to maxBank, or 0 for none. */
Messages encodeBank(int bank) noexcept;

/** True if a command fits the wire: its timer has a channel and its indexes are in range. */
bool fits(int timer, const TaymMods& command) noexcept;

}  // namespace TimerCC

//==============================================================================
/**
    Writes timer commands as controllers, sending the values that differ from what the chip was last sent.
    It alone remembers what was sent.

    A timer's first start states every value, since the chip may hold another clip's. A modulate before that
    start is dropped: the timer it would change was started before these commands began, and stays silent.
*/
class TimerEncoder {
public:
    /** The messages that carry `command` to timer `timer`; none for an empty command or one that does not fit. */
    TimerCC::Messages encode(int timer, const TaymMods& command) noexcept;

    /** Forgets what was sent. */
    void reset() noexcept { *this = {}; }

private:
    std::array<TimerCC::Values, TimerCC::maxTimers> sent_ {};
    std::array<bool, TimerCC::maxTimers> started_ {};
};

//==============================================================================
/**
    Reads the timers' controllers a frame at a time, as PsgDecoder reads the chip's, and gives each frame's
    commands in TAYM's terms.

    A timer that comes on without a start is started: it ran when play last stopped or jumped, and its values
    are stated again where play resumes.
*/
class TimerDecoder {
public:
    /** Takes a controller on a channel counted from the chip's base channel.
        Returns false if nothing about timers travels there. */
    bool decode(int channel, int controller, int value) noexcept;

    /** True if the frame so far changes nothing. */
    bool isEmpty() const noexcept { return touched_ == 0 && ! bankTouched_; }

    /** The frame's command for each timer, empty where none; the next message starts another frame. */
    std::array<TaymMods, TimerCC::maxTimers> takeCommands() noexcept;

    /** The bank, if the frame chose another; call before takeCommands. */
    std::optional<int> takeBank() noexcept;

    /** Forgets everything held: every timer is off. The bank stays. */
    void reset() noexcept;

private:
    std::array<TimerCC::Values, TimerCC::maxTimers> held_ {};
    std::array<TimerCC::Values, TimerCC::maxTimers> frame_ {};
    uint32_t touched_ = 0;  // a bit per timer the frame has a message for
    uint32_t starts_ = 0;   // a bit per timer the frame starts
    int bank_ = 0;
    int frameBank_ = 0;
    bool bankTouched_ = false;

    TimerCC::Values& touch(int timer) noexcept;
};

}  // namespace MoTool
