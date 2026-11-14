#include "ClipTimers.h"

#include "Ids.h"
#include "PsgClip.h"
#include "PsgList.h"
#include "PsgParameter.h"
#include "TimerCC.h"

namespace MoTool {

namespace {

// A timer lane reference in the state: an index, or one of TAYM's two markers as a small negative number
constexpr int laneNone = -1;
constexpr int laneUnchanged = -2;

int laneToState(uint32_t ref) noexcept {
    return ref == TAYM_TLAN_NONE ? laneNone : ref == TAYM_TLAN_UNCHANGED ? laneUnchanged : static_cast<int>(ref);
}

uint32_t laneFromState(int lane) noexcept {
    return lane == laneNone ? TAYM_TLAN_NONE : lane == laneUnchanged ? TAYM_TLAN_UNCHANGED : static_cast<uint32_t>(lane);
}

bool hasValues(const TaymMods& command) noexcept {
    return command.command == TAYM_CMD_START || command.command == TAYM_CMD_MODULATE;
}

}  // namespace

//==============================================================================
ClipTimers::ClipTimers(const juce::ValueTree& clipState) : clipState_(clipState) {
    reload();
    clipState_.addListener(this);
}

ClipTimers::~ClipTimers() {
    clipState_.removeListener(this);
}

void ClipTimers::valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) {
    if (parent == clipState_ && child.hasType(IDs::TIMERS))
        reload();
}

void ClipTimers::valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int) {
    if (parent == clipState_ && child.hasType(IDs::TIMERS))
        reload();
}

//==============================================================================
const std::vector<TimerSpan>& ClipTimers::getSpans(int timer) const noexcept {
    static const std::vector<TimerSpan> none;
    return juce::isPositiveAndBelow(timer, getNumTimers()) ? spans_[static_cast<size_t>(timer)] : none;
}

const TimerSpan* ClipTimers::findSpan(int timer, int frameIndex) const noexcept {
    const auto& spans = getSpans(timer);
    auto after = std::upper_bound(spans.begin(), spans.end(), frameIndex,
                                  [](int frame, const TimerSpan& span) { return frame < span.startFrame; });
    if (after == spans.begin())
        return nullptr;
    const auto& span = *std::prev(after);
    return frameIndex < span.endFrame ? &span : nullptr;
}

void ClipTimers::applyVolumes(PsgParamFrameData& data, int frameIndex) const {
    for (int timer = 0; timer < getNumTimers(); ++timer)
        if (const auto* span = findSpan(timer, frameIndex))
            span->applyVolumes(data);
}

//==============================================================================
void ClipTimers::set(const juce::MemoryBlock& bank, int chipIndex, const std::vector<TimerEvent>& events, juce::UndoManager* um) {
    // Built detached, then attached in one step: one undo, and one change for whoever listens
    juce::ValueTree timers {IDs::TIMERS};
    timers.setProperty(IDs::bank, bank, nullptr);
    timers.setProperty(IDs::chip, chipIndex, nullptr);
    for (const auto& event : events) {
        juce::ValueTree v {IDs::TIMER};
        v.setProperty(IDs::i, event.frameIndex, nullptr);
        v.setProperty(IDs::timer, event.timer, nullptr);
        v.setProperty(IDs::cmd, static_cast<int>(event.command.command), nullptr);
        if (hasValues(event.command)) {
            v.setProperty(IDs::base, static_cast<juce::int64>(event.command.base_timer_value), nullptr);
            v.setProperty(IDs::lane, laneToState(event.command.timer_lane_ref), nullptr);
            v.setProperty(IDs::first, static_cast<juce::int64>(event.command.first_action), nullptr);
            v.setProperty(IDs::count, static_cast<int>(event.command.action_count), nullptr);
        }
        timers.appendChild(v, nullptr);
    }

    clipState_.removeChild(clipState_.getChildWithName(IDs::TIMERS), um);
    if (! events.empty())
        clipState_.addChild(timers, -1, um);
}

void ClipTimers::reload() {
    events_.clear();
    spans_.clear();
    edges_.clear();
    registration_.reset();
    bank_.reset();

    const auto timers = clipState_.getChildWithName(IDs::TIMERS);
    if (! timers.isValid())
        return;

    if (const auto* bytes = timers[IDs::bank].getBinaryData())
        bank_ = TimerBank::fromTaym(bytes->getData(), bytes->getSize(),
                                    static_cast<size_t>(juce::jmax(0, static_cast<int>(timers[IDs::chip]))));
    if (bank_ == nullptr)
        return;  // commands without their bank index nothing
    registration_ = TimerBankRegistry::getInstance().add(bank_);

    events_.reserve(static_cast<size_t>(timers.getNumChildren()));
    for (const auto& v : timers) {
        if (! v.hasType(IDs::TIMER))
            continue;
        TimerEvent event;
        event.frameIndex = v[IDs::i];
        event.timer = v[IDs::timer];
        event.command.command = static_cast<uint8_t>(static_cast<int>(v[IDs::cmd]));
        if (hasValues(event.command)) {
            event.command.base_timer_value = static_cast<uint32_t>(static_cast<juce::int64>(v[IDs::base]));
            event.command.timer_lane_ref = laneFromState(v[IDs::lane]);
            event.command.first_action = static_cast<uint32_t>(static_cast<juce::int64>(v[IDs::first]));
            event.command.action_count = static_cast<uint8_t>(static_cast<int>(v[IDs::count]));
        }
        events_.push_back(event);
    }
    std::stable_sort(events_.begin(), events_.end(), [](const auto& a, const auto& b) { return a.frameIndex < b.frameIndex; });

    spans_ = findTimerSpans(*bank_, events_);
    for (const auto& spans : spans_) {
        for (const auto& span : spans) {
            edges_.push_back(span.startFrame);
            if (span.endFrame != TimerSpan::toTheEnd)
                edges_.push_back(span.endFrame);
        }
    }
    std::sort(edges_.begin(), edges_.end());
    edges_.erase(std::unique(edges_.begin(), edges_.end()), edges_.end());
}

void ClipTimers::addToPlaybackSequence(juce::MidiMessageSequence& sequence, const PsgClip& clip,
                                       tracktion::MidiList::TimeBase timeBase) const {
    const int channel = clip.getPsg().getMidiChannel().getChannelNumber();
    juce::MidiMessageSequence timers;
    const auto add = [&](double time, const TimerCC::Messages& messages) {
        for (const auto& m : messages)
            timers.addEvent(juce::MidiMessage::controllerEvent(channel + m.channel, static_cast<int>(m.controller), m.value), time);
    };

    // The bank where the sequence begins: the clip's start, or in raw beats the first frame, from where
    // Tracktion chases the controllers to the clip's start. A clip without timers chooses none, which stops
    // whatever timers the clip before it left running
    const int bankNumber = getBankNumber();
    add(timeBase == tracktion::MidiList::TimeBase::beatsRaw ? PsgList::getFrameTimeInBase(0, clip, timeBase) : 0.0,
        TimerCC::encodeBank(bankNumber));

    if (bankNumber != 0) {
        // A clip trimmed at its start leaves out the commands before it, and starts the timers they leave
        // running where it starts, as play that resumes there does. (In raw beats nothing is left out:
        // Tracktion chases the controllers, and the chip starts a timer stated as on.)
        TimerEncoder encoder;
        auto event = events_.begin();
        for (; event != events_.end() && PsgList::isFrameBeforeClip(event->frameIndex, clip, timeBase); ++event)
            encoder.skip(event->timer, event->command);
        for (int timer = 0; timer < TimerCC::maxTimers; ++timer)
            add(0.0, encoder.restart(timer));
        for (; event != events_.end(); ++event)
            add(PsgList::getFrameTimeInBase(event->frameIndex, clip, timeBase), encoder.encode(event->timer, event->command));
    }

    // Appended in time order above; merged with the frames' messages in one sort, which keeps equal times
    // in their order
    sequence.addSequence(timers, 0.0);
}

}  // namespace MoTool
