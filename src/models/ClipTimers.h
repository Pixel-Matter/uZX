#pragma once

#include <JuceHeader.h>

#include "TimerBank.h"
#include "TimerSpans.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace MoTool {

class PsgClip;
class PsgParamFrameData;

//==============================================================================
/**
    The timers of a PSG clip: what a TAYM has besides its frames. A bank of lanes and actions, kept as the
    TAYM it came in, and the commands that start, change and stop the chip's timers, each at a frame of the
    clip's PSG list. A clip without them is a plain PSG clip.

    The commands are on the list's frame grid by index, so they stay with its frames when the tempo or the
    clip's frame rate changes, and a clip trimmed at its start plays those from its start on.

    In the clip's state, which this follows as it changes:

        TIMERS  bank="<a TAYM without frame data and commands>"  chip="<its chip in that TAYM>"
            TIMER  i="<frame>"  timer="<the chip's timer>"  cmd="<1 start, 2 modulate, 3 stop>"
                   base  lane  first  count        (start and modulate; lane -1 is none, -2 unchanged)
*/
class ClipTimers : private juce::ValueTree::Listener {
public:
    explicit ClipTimers(const juce::ValueTree& clipState);
    ~ClipTimers() override;

    /** Gives the clip these timers in place of any it has: `bank` is a TAYM's bytes, `chipIndex` the chip
        in it whose timers these are, `events` its commands in frame order. */
    void set(const juce::MemoryBlock& bank, int chipIndex, const std::vector<TimerEvent>& events, juce::UndoManager*);

    bool isEmpty() const noexcept { return events_.empty(); }

    /** The commands in frame order. */
    const std::vector<TimerEvent>& getEvents() const noexcept { return events_; }

    /** The bank the commands index; null if the clip has none or its bytes cannot be read. */
    TimerBank::Ptr getBank() const noexcept { return bank_; }

    /** What each timer does, a run of frames at a time, in frame order (findTimerSpans). */
    const std::vector<TimerSpan>& getSpans(int timer) const noexcept;
    int getNumTimers() const noexcept { return static_cast<int>(spans_.size()); }

    /** The span of `timer` that frame `frameIndex` is in, or null if the timer is not running then. */
    const TimerSpan* findSpan(int timer, int frameIndex) const noexcept;

    /** Puts into a frame's parameters the volumes the timers running at frame `frameIndex` hold: what the
        chip's volume registers have there, where the frames' own may say silence. */
    void applyVolumes(PsgParamFrameData&, int frameIndex) const;

    /** Calls fn(from, to) for each stretch of the frames from `from` to before `to` in which no timer
        starts, changes or stops: where the frames' parameters with the timers' volumes stay the same. */
    template <typename Fn>
    void forEachStretch(int from, int to, Fn&& fn) const {
        for (auto edge = std::upper_bound(edges_.begin(), edges_.end(), from); edge != edges_.end() && *edge < to; ++edge) {
            fn(from, *edge);
            from = *edge;
        }
        fn(from, to);
    }

    /** The number the bank goes by on the wire while this clip lives; 0 for none. */
    int getBankNumber() const noexcept { return registration_ != nullptr ? registration_->getNumber() : 0; }

    /** Adds the messages that play the timers to a clip's playback sequence: the bank's number where the
        sequence begins, 0 for a clip without timers, so that the timers of a clip before it stop; then each
        command at its frame's time. A timer running where a clip trimmed at its start begins is started
        there, as it is where play resumes inside its run. */
    void addToPlaybackSequence(juce::MidiMessageSequence&, const PsgClip&, tracktion::MidiList::TimeBase) const;

private:
    juce::ValueTree clipState_;
    std::vector<TimerEvent> events_;
    TimerBank::Ptr bank_;
    std::unique_ptr<TimerBankRegistry::Registration> registration_;
    std::vector<std::vector<TimerSpan>> spans_;  // by timer
    std::vector<int> edges_;                     // the frames spans start or end at, in order, once each

    /** Reads the clip's state again. */
    void reload();

    void valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) override;
    void valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int) override;
};

}  // namespace MoTool
