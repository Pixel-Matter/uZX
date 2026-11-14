#include "TimerSpans.h"

#include "PsgParameter.h"

#include <algorithm>
#include <numeric>

namespace MoTool {

namespace {

/** The steps of a lane that repeat: from where it loops to its end. A lane that does not loop holds its last
    element. Never empty, even for a lane that is. */
struct Loop {
    uint32_t first = 0, count = 1;
};

Loop loopOf(uint32_t length, uint32_t loopIndex) noexcept {
    if (length == 0)
        return {};
    if (loopIndex == TAYM_NO_LOOP || loopIndex >= length)
        return {length - 1, 1};
    return {loopIndex, length - loopIndex};
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
void describeRepeat(TimerSpan& span, const TimerBank& bank, double baseInterval) {
    constexpr size_t maxSteps = 256;  // repeats longer than this are taken as that many steps
    const auto& t = bank.getTables();
    const auto& clock = bank.getTimerClocks()[span.timer];
    const double clockHz = bank.getClockHz();

    // The interval of each step, and how many steps before they repeat
    const TaymTlan* timerLane = span.timerLane >= 0 ? &t.tlanes[span.timerLane] : nullptr;
    if (timerLane != nullptr && timerLane->loop_index == TAYM_NO_LOOP)
        return;  // it ends: nothing repeats
    const auto intervals = timerLane != nullptr ? loopOf(timerLane->length, timerLane->loop_index) : Loop {};
    const auto intervalAt = [&](size_t step) {
        if (timerLane == nullptr)
            return baseInterval;
        const auto value = bank.getPoolValue(TAYM_VT_U32, size_t {timerLane->value_offset} + intervals.first
                                                              + step % intervals.count);
        if (timerLane->timing_mode == TAYM_TM_ABSOLUTE)
            return clock.clock_mode == TAYM_CLOCK_CHIP_PERIOD && value == 0 ? baseInterval : intervalCycles(clock, value, clockHz);
        const double multiplier = taym_from_fix16(value);
        return multiplier > 0.0 ? baseInterval / multiplier : 0.0;
    };

    // The value of each target at each step
    struct Values {
        Loop loop;
        const TaymLane* lane = nullptr;
        uint32_t value = 0;
    };
    std::array<Values, TAYM_MAX_TIMER_TARGETS> values;
    size_t steps = intervals.count;
    for (size_t i = 0; i < span.numTargets; ++i) {
        const auto& target = span.targets[i];
        if (target.lane >= 0) {
            values[i].lane = &t.lanes[target.lane];
            values[i].loop = loopOf(values[i].lane->length, values[i].lane->loop_index);
        } else {
            values[i].value = target.value;
        }
        steps = std::lcm(steps, size_t {values[i].loop.count});
        if (steps > maxSteps)
            break;
    }
    steps = juce::jlimit(size_t {1}, maxSteps, steps);
    const auto valueAt = [&](size_t target, size_t step) {
        const auto& v = values[target];
        if (v.lane == nullptr)
            return v.value;
        return bank.getPoolValue(v.lane->value_type, size_t {v.lane->value_offset} + v.loop.first + step % v.loop.count);
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

    // Every expiry alike writes one steady value, which is no pitch, unless each write restarts the envelope
    const bool restartsEnvelope = (span.registers & (1u << 13)) != 0;
    if (period < 2 && ! restartsEnvelope)
        return;
    double cycles = 0.0;
    for (size_t i = 0; i < period; ++i)
        cycles += intervalAt(i);
    if (cycles <= 0.0)
        return;
    span.loopCycles = cycles;
    if (period < 2)
        return;

    // Duty: the time on the highest value of a volume it writes; what other registers hold has none
    std::optional<size_t> volume;
    for (size_t i = 0; i < span.numTargets; ++i)
        if (span.targets[i].id >= 8 && span.targets[i].id <= 10)
            volume = i;
    if (! volume)
        return;
    const size_t heard = *volume;
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

/** What a span holds, from the targets of its timer as the engine has them. */
void describeTargets(TimerSpan& span, const TaymTimerState& state, const TimerBank& bank) {
    const auto& t = bank.getTables();
    bool amplitude = false;
    for (size_t i = 0; i < state.target_count; ++i) {
        const auto& target = state.targets[i];
        span.targets[i] = {target.target_id, target.has_lane ? static_cast<int>(target.lane_index) : -1, target.inline_value};
        const int id = target.target_id;
        if (id <= 13)
            span.registers = static_cast<uint16_t>(span.registers | (1u << id));
        if (id <= 5)
            span.channels = static_cast<uint8_t>(span.channels | (1u << (id / 2)));
        else if (id >= 8 && id <= 10)
            span.channels = static_cast<uint8_t>(span.channels | (1u << (id - 8)));
        span.noise = span.noise || id == 6;
        span.mixer = span.mixer || id == 7;
        span.envelope = span.envelope || (id >= 11 && id <= 13);
        amplitude = amplitude || id == TAYM_TGT_SAMPLE_AMPLITUDE;

        // Every value it may write, the lane's start too: a sample plays it once
        const auto forEachValue = [&](auto&& take) {
            if (target.has_lane) {
                const auto& lane = t.lanes[target.lane_index];
                for (uint32_t step = 0; step < lane.length; ++step)
                    take(bank.getPoolValue(lane.value_type, size_t {lane.value_offset} + step));
            } else {
                take(target.inline_value);
            }
        };
        if (id >= 8 && id <= 10) {
            auto& loudest = span.loudestVolumes[static_cast<size_t>(id - 8)];
            forEachValue([&loudest](uint32_t value) {
                loudest = static_cast<uint8_t>(juce::jmax(loudest & 0x0F, static_cast<int>(value & 0x0F)) | ((loudest | value) & 0x10));
            });
        } else if (id == 13) {
            std::array<int, 16> written {};
            forEachValue([&written](uint32_t value) { ++written[value & 0x0F]; });
            span.envelopeShape = static_cast<int>(std::max_element(written.begin(), written.end()) - written.begin());
        }
    }
    span.numTargets = state.target_count;
    if (amplitude)  // a sample amplitude scales a volume as a level of the chip's curve, which has no envelope switch
        for (auto& loudest : span.loudestVolumes)
            loudest &= 0x0F;
}

}  // namespace

//==============================================================================
std::optional<float> TimerSpan::getPitch() const noexcept {
    const auto toneScale = PsgParamType {PsgParamType::TonePeriodA}.getScale();
    const double tonePeriod = loopCycles / 16.0;
    if (tonePeriod < 1.0 || tonePeriod > static_cast<double>(toneScale.end))
        return std::nullopt;
    return toneScale.valueToNormalized(static_cast<float>(tonePeriod));
}

void TimerSpan::applyVolumes(PsgParamFrameData& data) const {
    for (size_t chan = 0; chan < loudestVolumes.size(); ++chan) {
        if ((registers & (1u << (8 + chan))) == 0)
            continue;
        const auto offset = static_cast<int>(chan);
        data.set(PsgParamType::VolumeA + offset, static_cast<uint16_t>(loudestVolumes[chan] & 0x0F));
        data.set(PsgParamType::EnvelopeIsOnA + offset, static_cast<uint16_t>((loudestVolumes[chan] >> 4) & 1));
    }
}

std::vector<std::vector<TimerSpan>> findTimerSpans(const TimerBank& bank, const std::vector<TimerEvent>& events) {
    std::vector<std::vector<TimerSpan>> spans(bank.getNumTimers());
    TaymTimerBank engine {};
    if (taym_timers_init(&engine, &bank.getTables(), bank.getTimerClocks(), bank.getNumTimers(), bank.getClockHz()) != TAYM_OK)
        return spans;

    std::vector<uint32_t> bases(bank.getNumTimers());  // the base value each timer was last given
    for (const auto& event : events) {
        if (! juce::isPositiveAndBelow(event.timer, static_cast<int>(spans.size())))
            continue;
        const auto timer = static_cast<size_t>(event.timer);
        const auto& command = event.command;
        const auto& state = engine.timers[timer];
        const bool ran = state.active;
        if (taym_timers_command(&engine, timer, &command, 0) != TAYM_OK || command.command == TAYM_CMD_EMPTY)
            continue;  // refused: the chip's timers ignore it as well
        if (ran && ! spans[timer].empty())
            spans[timer].back().endFrame = event.frameIndex;
        if (! state.active)
            continue;  // stopped, or a modulate of a timer that was not running

        const bool starts = command.command == TAYM_CMD_START;
        if (starts || command.base_timer_value != 0)
            bases[timer] = command.base_timer_value;
        TimerSpan span;
        span.timer = event.timer;
        span.startFrame = event.frameIndex;
        span.endFrame = TimerSpan::toTheEnd;
        span.starts = starts;
        span.base = bases[timer];
        span.timerLane = state.has_tlan ? static_cast<int>(state.tlan_index) : -1;
        describeTargets(span, state, bank);
        describeRepeat(span, bank, state.base_interval);
        spans[timer].push_back(span);
    }
    return spans;
}

}  // namespace MoTool
