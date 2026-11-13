#pragma once

#include <JuceHeader.h>

#include "TimerBank.h"

#include <taym/taym.h>

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
};

}  // namespace MoTool
