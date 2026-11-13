#include "ClipTimers.h"

#include "Ids.h"
#include "PsgClip.h"
#include "PsgList.h"
#include "TimerCC.h"

#include <algorithm>
#include <numeric>
#include <tuple>

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
namespace {

/** A lane's values from where it loops to its end: the part that repeats. A lane that ends holds its last. */
template <typename Lane>
std::pair<uint32_t, uint32_t> loopPart(const Lane& lane) noexcept {
    if (lane.length == 0)
        return {0, 0};
    if (lane.loop_index == TAYM_NO_LOOP || lane.loop_index >= lane.length)
        return {lane.length - 1, 1};
    return {lane.loop_index, lane.length - lane.loop_index};
}

uint32_t laneValue(const TaymTimerTables& t, const TaymLane& lane, uint32_t step) noexcept {
    const auto index = static_cast<size_t>(lane.value_offset) + step;
    switch (lane.value_type) {
        case TAYM_VT_U8:  return index < t.vu08_count ? t.vu08[index] : 0;
        case TAYM_VT_U16: return index < t.vu16_count ? t.vu16[index] : 0;
        case TAYM_VT_U32: return index < t.vu32_count ? t.vu32[index] : 0;
        default:          return 0;
    }
}

/** A timer value as chip cycles between expiries, as the engine counts them (TAYM S10). */
double intervalCycles(const TaymTimerClock& clock, uint32_t value, double clockHz) noexcept {
    if (clock.clock_mode == TAYM_CLOCK_ABS_RATE_HZ) {
        const double hz = taym_from_fix16(value);
        return hz > 0.0 ? clockHz / hz : 0.0;
    }
    return static_cast<double>(clock.clock_divider) * static_cast<double>(value);
}

/** Works out a span's repeat: how long its values and intervals take to come round, and its duty. */
void describeRepeat(ClipTimers::Span& span, const TimerBank& bank) {
    constexpr size_t maxSteps = 256;  // repeats longer than this are taken as that many steps
    const auto& t = bank.getTables();
    const auto& clock = bank.getTimerClocks()[span.timer];
    const double clockHz = bank.getTaym().chips[bank.getChipIndex()].clock_hz;
    const double baseInterval = intervalCycles(clock, span.base, clockHz);

    // The interval of each step, and how many steps before they repeat
    const TaymTlan* timerLane = span.timerLane >= 0 && static_cast<size_t>(span.timerLane) < t.tlane_count
                                    ? &t.tlanes[span.timerLane] : nullptr;
    if (timerLane != nullptr && timerLane->loop_index == TAYM_NO_LOOP)
        return;  // it ends: nothing repeats
    const auto [intervalFirst, intervalCount] = timerLane != nullptr ? loopPart(*timerLane) : std::pair<uint32_t, uint32_t> {0, 1};
    const auto intervalAt = [&](size_t step) {
        if (timerLane == nullptr)
            return baseInterval;
        const auto index = static_cast<size_t>(timerLane->value_offset) + intervalFirst + step % intervalCount;
        const uint32_t value = index < t.vu32_count ? t.vu32[index] : 0;
        if (timerLane->timing_mode == TAYM_TM_ABSOLUTE)
            return clock.clock_mode == TAYM_CLOCK_CHIP_PERIOD && value == 0 ? baseInterval : intervalCycles(clock, value, clockHz);
        const double multiplier = taym_from_fix16(value);
        return multiplier > 0.0 ? baseInterval / multiplier : 0.0;
    };

    // The value of each target at each step
    struct Values { uint32_t first = 0, count = 1; const TaymLane* lane = nullptr; uint32_t value = 0; };
    std::array<Values, TAYM_MAX_TIMER_TARGETS> values;
    size_t steps = intervalCount;
    for (size_t i = 0; i < span.numTargets; ++i) {
        const auto& target = span.targets[i];
        if (target.lane >= 0 && static_cast<size_t>(target.lane) < t.lane_count) {
            values[i].lane = &t.lanes[target.lane];
            std::tie(values[i].first, values[i].count) = loopPart(*values[i].lane);
        } else {
            values[i].value = target.value;
        }
        steps = std::lcm(steps, static_cast<size_t>(juce::jmax(uint32_t {1}, values[i].count)));
        if (steps > maxSteps)
            break;
    }
    steps = juce::jlimit(size_t {1}, maxSteps, steps);
    const auto valueAt = [&](size_t target, size_t step) {
        const auto& v = values[target];
        return v.lane != nullptr ? laneValue(t, *v.lane, v.first + static_cast<uint32_t>(step % v.count)) : v.value;
    };
    const auto sameStep = [&](size_t a, size_t b) {
        if (! juce::approximatelyEqual(intervalAt(a), intervalAt(b)))
            return false;
        for (size_t i = 0; i < span.numTargets; ++i)
            if (valueAt(i, a) != valueAt(i, b))
                return false;
        return true;
    };

    // The shortest stretch of steps the whole repeats in
    size_t period = steps;
    for (size_t p = 1; p < steps; ++p) {
        if (steps % p != 0)
            continue;
        bool repeats = true;
        for (size_t i = p; repeats && i < steps; ++i)
            repeats = sameStep(i, i - p);
        if (repeats) {
            period = p;
            break;
        }
    }
    for (size_t i = 0; i < period; ++i)
        span.loopCycles += intervalAt(i);
    if (span.loopCycles <= 0.0 || period < 2) {
        span.loopCycles = juce::jmax(0.0, span.loopCycles);
        return;
    }

    // Duty: the time on the highest value of the target that is heard most plainly, a volume if there is one
    size_t heard = 0;
    for (size_t i = 0; i < span.numTargets; ++i)
        if (span.targets[i].id >= 8 && span.targets[i].id <= 10)
            heard = i;
    uint32_t highest = 0;
    bool varies = false;
    for (size_t i = 0; i < period; ++i) {
        highest = juce::jmax(highest, valueAt(heard, i));
        varies = varies || valueAt(heard, i) != valueAt(heard, 0);
    }
    double on = 0.0;
    for (size_t i = 0; i < period; ++i)
        if (varies ? valueAt(heard, i) == highest : i == 0)
            on += intervalAt(i);
    span.duty = static_cast<float>(on / span.loopCycles);
}

}  // namespace

//==============================================================================
const std::vector<ClipTimers::Span>& ClipTimers::getSpans(int timer) const noexcept {
    static const std::vector<Span> none;
    return juce::isPositiveAndBelow(timer, getNumTimers()) ? spans_[static_cast<size_t>(timer)] : none;
}

const ClipTimers::Span* ClipTimers::findSpan(int timer, int frameIndex) const noexcept {
    const auto& spans = getSpans(timer);
    auto after = std::upper_bound(spans.begin(), spans.end(), frameIndex,
                                  [](int frame, const Span& span) { return frame < span.startFrame; });
    if (after == spans.begin())
        return nullptr;
    const auto& span = *std::prev(after);
    return frameIndex < span.endFrame ? &span : nullptr;
}

void ClipTimers::buildSpans() {
    spans_.clear();
    if (bank_ == nullptr || events_.empty())
        return;
    const auto& tables = bank_->getTables();
    spans_.resize(bank_->getNumTimers());

    // Each timer's state as its commands change it, as the chip's timers take them (TAYM S12)
    std::vector<Span> running(bank_->getNumTimers());
    std::vector<bool> active(bank_->getNumTimers(), false);
    const auto close = [&](size_t timer, int frame) {
        if (active[timer] && ! spans_[timer].empty())
            spans_[timer].back().endFrame = frame;
    };
    const auto setTargets = [&](Span& span, const TaymMods& command, bool replace) {
        for (size_t i = 0; i < command.action_count && command.first_action + i < tables.action_count; ++i) {
            const auto& action = tables.actions[command.first_action + i];
            const Span::Target target {action.target_id,
                                       action.source_mode == TAYM_SRC_BIND_LANE ? static_cast<int>(action.operand) : -1,
                                       action.source_mode == TAYM_SRC_BIND_LANE ? 0 : action.operand};
            if (replace) {
                if (span.numTargets < span.targets.size())
                    span.targets[span.numTargets++] = target;
            } else {
                // a modulate changes the source of a target the timer has, and no more
                for (size_t j = 0; j < span.numTargets; ++j)
                    if (span.targets[j].id == target.id)
                        span.targets[j] = target;
            }
        }
    };

    for (const auto& event : events_) {
        if (! juce::isPositiveAndBelow(event.timer, static_cast<int>(running.size())))
            continue;
        const auto timer = static_cast<size_t>(event.timer);
        const auto& command = event.command;
        auto& span = running[timer];

        if (command.command == TAYM_CMD_STOP) {
            close(timer, event.frameIndex);
            active[timer] = false;
            continue;
        }
        if (command.command == TAYM_CMD_MODULATE && ! active[timer])
            continue;  // the chip ignores it
        if (command.command != TAYM_CMD_START && command.command != TAYM_CMD_MODULATE)
            continue;

        close(timer, event.frameIndex);
        if (command.command == TAYM_CMD_START) {
            span = {};
            span.timer = event.timer;
            span.startedFrame = event.frameIndex;
            span.base = command.base_timer_value;
            span.timerLane = command.timer_lane_ref == TAYM_TLAN_NONE || command.timer_lane_ref == TAYM_TLAN_UNCHANGED
                                 ? -1 : static_cast<int>(command.timer_lane_ref);
            setTargets(span, command, true);
        } else {
            if (command.base_timer_value != 0)
                span.base = command.base_timer_value;
            if (command.timer_lane_ref == TAYM_TLAN_NONE)
                span.timerLane = -1;
            else if (command.timer_lane_ref != TAYM_TLAN_UNCHANGED)
                span.timerLane = static_cast<int>(command.timer_lane_ref);
            setTargets(span, command, false);
        }
        span.starts = command.command == TAYM_CMD_START;
        span.startFrame = event.frameIndex;
        span.endFrame = Span::toTheEnd;
        span.registers = 0;
        span.channels = 0;
        span.envelope = span.noise = span.mixer = false;
        for (size_t i = 0; i < span.numTargets; ++i) {
            const int id = span.targets[i].id;
            if (id <= 13)
                span.registers = static_cast<uint16_t>(span.registers | (1u << id));
            if (id <= 5)
                span.channels = static_cast<uint8_t>(span.channels | (1u << (id / 2)));
            else if (id >= 8 && id <= 10)
                span.channels = static_cast<uint8_t>(span.channels | (1u << (id - 8)));
            span.noise = span.noise || id == 6;
            span.mixer = span.mixer || id == 7;
            span.envelope = span.envelope || (id >= 11 && id <= 13);
        }
        span.loopCycles = 0.0;
        span.duty = -1.0f;
        describeRepeat(span, *bank_);
        spans_[timer].push_back(span);
        active[timer] = true;
    }
}

//==============================================================================
void ClipTimers::set(const juce::MemoryBlock& bank, int chipIndex, const std::vector<Event>& events, juce::UndoManager* um) {
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
    reload();
}

void ClipTimers::reload() {
    events_.clear();
    spans_.clear();
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
        Event event;
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
    std::stable_sort(events_.begin(), events_.end(), [](const Event& a, const Event& b) { return a.frameIndex < b.frameIndex; });
    buildSpans();
}

void ClipTimers::addToPlaybackSequence(juce::MidiMessageSequence& sequence, const PsgClip& clip,
                                       tracktion::MidiList::TimeBase timeBase) const {
    const int bankNumber = getBankNumber();
    if (events_.empty() || bankNumber == 0)
        return;

    const int channel = clip.getPsg().getMidiChannel().getChannelNumber();
    juce::MidiMessageSequence timers;
    const auto add = [&](double time, const TimerCC::Messages& messages) {
        for (const auto& m : messages)
            timers.addEvent(juce::MidiMessage::controllerEvent(channel + m.channel, static_cast<int>(m.controller), m.value), time);
    };

    // The bank where the sequence begins: the clip's start, or in raw beats the first frame, from where
    // Tracktion chases the controllers to the clip's start
    add(timeBase == tracktion::MidiList::TimeBase::beatsRaw ? PsgList::getFrameTimeInBase(0, clip, timeBase) : 0.0,
        TimerCC::encodeBank(bankNumber));

    TimerEncoder encoder;
    for (const auto& event : events_) {
        if (PsgList::isFrameBeforeClip(event.frameIndex, clip, timeBase))
            continue;
        add(PsgList::getFrameTimeInBase(event.frameIndex, clip, timeBase), encoder.encode(event.timer, event.command));
    }

    // Appended in time order above; merged with the frames' messages in one sort, which keeps equal times
    // in their order
    sequence.addSequence(timers, 0.0);
}

}  // namespace MoTool
