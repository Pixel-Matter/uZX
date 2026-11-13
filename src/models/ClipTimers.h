#pragma once

#include <JuceHeader.h>

#include "TimerBank.h"

#include <taym/taym.h>

#include <array>
#include <limits>
#include <memory>
#include <vector>

namespace MoTool {

class PsgClip;

//==============================================================================
/**
    The timers of a PSG clip: what a TAYM has besides its frames. A bank of lanes and actions, kept as the
    TAYM it came in, and the commands that start, change and stop the chip's timers, each at a frame of the
    clip's PSG list. A clip without them is a plain PSG clip.

    The commands are on the list's frame grid by index, so they stay with its frames when the tempo or the
    clip's frame rate changes, and a clip trimmed at its start plays those from its start on.

    In the clip's state:

        TIMERS  bank="<a TAYM without frame data and commands>"  chip="<its chip in that TAYM>"
            TIMER  i="<frame>"  timer="<the chip's timer>"  cmd="<1 start, 2 modulate, 3 stop>"
                   base  lane  first  count        (start and modulate; lane -1 is none, -2 unchanged)
*/
class ClipTimers {
public:
    struct Event {
        int frameIndex = 0;
        int timer = 0;      ///< among the chip's timers, as the wire numbers them
        TaymMods command {};
    };

    /** What one timer does from one of its commands to its next: a stretch of frames in which it holds the
        same registers and writes them in the same repeating way. What the clip draws and describes. */
    struct Span {
        struct Target {
            uint8_t id = 0;      ///< TAYM's target: a register R0..R13, or 0x80 for a sample amplitude
            int lane = -1;       ///< the value lane it reads, or -1 for one value
            uint32_t value = 0;  ///< that one value
        };

        int timer = 0;
        int startFrame = 0;
        int endFrame = 0;       ///< the frame of the timer's next command; `toTheEnd` if it has none
        bool starts = false;    ///< began by a start; else by a modulate of the running timer
        int startedFrame = 0;   ///< the frame of the start this run of the timer began with

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

        /** The chip cycles one repeat of what it writes takes: its values and intervals come round again
            after that long, so it is the period of what is heard. 0 if it does not repeat: a timer lane that
            ends, or no rate. A tone of period `loopCycles / 16` has the same pitch. */
        double loopCycles = 0.0;
        /** The part of a repeat spent on its highest value, or on its first interval when the values are all
            one: a pulse's duty. Negative if every step is alike. */
        float duty = -1.0f;
    };

    explicit ClipTimers(const juce::ValueTree& clipState) : clipState_(clipState) {}

    /** Gives the clip these timers in place of any it has: `bank` is a TAYM's bytes, `chipIndex` the chip
        in it whose timers these are, `events` its commands in frame order. */
    void set(const juce::MemoryBlock& bank, int chipIndex, const std::vector<Event>& events, juce::UndoManager*);

    /** Reads the clip's state again: call after it gains, loses or changes its timers. */
    void reload();

    bool isEmpty() const noexcept { return events_.empty(); }

    /** The commands in frame order. */
    const std::vector<Event>& getEvents() const noexcept { return events_; }

    /** The bank the commands index; null if the clip has none or its bytes cannot be read. */
    TimerBank::Ptr getBank() const noexcept { return bank_; }

    /** What each timer does, a run of frames at a time, in frame order. Worked out from the commands as the
        chip's timers would take them. */
    const std::vector<Span>& getSpans(int timer) const noexcept;
    int getNumTimers() const noexcept { return static_cast<int>(spans_.size()); }

    /** The span of `timer` that frame `frameIndex` is in, or null if the timer is not running then. */
    const Span* findSpan(int timer, int frameIndex) const noexcept;

    /** The number the bank goes by on the wire while this clip lives; 0 for none. */
    int getBankNumber() const noexcept { return registration_ != nullptr ? registration_->getNumber() : 0; }

    /** Adds the messages that play the timers to a clip's playback sequence: the bank's number where the
        sequence begins, then each command at its frame's time. Commands before the clip's start are left out,
        so a timer running across it is silent until its next start. */
    void addToPlaybackSequence(juce::MidiMessageSequence&, const PsgClip&, tracktion::MidiList::TimeBase) const;

private:
    juce::ValueTree clipState_;
    std::vector<Event> events_;
    TimerBank::Ptr bank_;
    std::unique_ptr<TimerBankRegistry::Registration> registration_;
    std::vector<std::vector<Span>> spans_;  // by timer

    void buildSpans();
};

}  // namespace MoTool
