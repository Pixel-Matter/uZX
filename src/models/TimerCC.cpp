#include "TimerCC.h"

namespace MoTool {

namespace {

// A value wider than a controller goes most significant part first and its least significant part last, as
// a period does, so a device that acts on each message can wait for the last part.
constexpr auto onSwitch = MidiCCType::GPB1ToneSwitch;         // 80
constexpr auto startSwitch = MidiCCType::GPB4RetriggerSwitch; // 83, pressed and released

constexpr auto baseTop = MidiCCType::CC21PeriodTop;      // bits 28 to 31
constexpr auto baseHigh = MidiCCType::GPC3;              // bits 21 to 27
constexpr auto baseHighFine = MidiCCType::GPC3Fine;      // bits 14 to 20
constexpr auto baseLow = MidiCCType::CC20PeriodCoarse;   // bits 7 to 13
constexpr auto baseLowFine = MidiCCType::CC52PeriodFine; // bits 0 to 6: a base under 16384 goes as a period does

constexpr auto laneCoarse = MidiCCType::GPC1;
constexpr auto laneFine = MidiCCType::GPC1Fine;
constexpr auto actionCoarse = MidiCCType::GPC2;
constexpr auto actionFine = MidiCCType::GPC2Fine;
constexpr auto actionCount = MidiCCType::GPC4;

// The bank, on the chip's fourth channel. Not controllers 0 and 32, MIDI's own bank select: a host keeps
// those for the program change they belong to and does not state them again when play starts inside a clip.
constexpr int bankChannel = PsgCC::numChannels - 1;
constexpr auto bankCoarse = MidiCCType::GPC1;
constexpr auto bankFine = MidiCCType::GPC1Fine;

constexpr int coarseOf(int value) noexcept { return (value >> 7) & 0x7F; }
constexpr int fineOf(int value) noexcept { return value & 0x7F; }

/** `held` with the seven bits from `shift` up replaced by a controller's value. */
template <typename Int>
constexpr Int withBits(Int held, int shift, int value) noexcept {
    const auto mask = static_cast<uint32_t>(0x7F) << shift;
    return static_cast<Int>((static_cast<uint32_t>(held) & ~mask) | ((static_cast<uint32_t>(value) << shift) & mask));
}

int laneOf(uint32_t timerLaneRef) noexcept {
    return timerLaneRef == TAYM_TLAN_NONE ? 0 : static_cast<int>(timerLaneRef) + 1;
}

uint32_t timerLaneRefOf(int lane) noexcept {
    return lane == 0 ? TAYM_TLAN_NONE : static_cast<uint32_t>(lane - 1);
}

void addBase(TimerCC::Messages& out, int channel, uint32_t base) {
    out.add(channel, baseTop, static_cast<int>(base >> 28));
    out.add(channel, baseHigh, static_cast<int>((base >> 21) & 0x7F));
    out.add(channel, baseHighFine, static_cast<int>((base >> 14) & 0x7F));
    out.add(channel, baseLow, static_cast<int>((base >> 7) & 0x7F));
    out.add(channel, baseLowFine, static_cast<int>(base & 0x7F));
}

}  // namespace

//==============================================================================
TimerCC::Messages TimerCC::encodeBank(int bank) noexcept {
    Messages out;
    out.add(bankChannel, bankCoarse, coarseOf(bank));
    out.add(bankChannel, bankFine, fineOf(bank));
    return out;
}

bool TimerCC::fits(int timer, const TaymMods& command) noexcept {
    if (! juce::isPositiveAndBelow(timer, maxTimers))
        return false;
    if (command.command != TAYM_CMD_START && command.command != TAYM_CMD_MODULATE)
        return true;
    const bool hasLane = command.timer_lane_ref != TAYM_TLAN_NONE && command.timer_lane_ref != TAYM_TLAN_UNCHANGED;
    return (! hasLane || command.timer_lane_ref < static_cast<uint32_t>(maxLane))
        && command.first_action <= static_cast<uint32_t>(maxActionFirst)
        && command.action_count <= maxActionCount;
}

TimerCC::Values TimerCC::apply(Values held, const TaymMods& command) noexcept {
    switch (command.command) {
        case TAYM_CMD_START:
            return {true, command.base_timer_value, laneOf(command.timer_lane_ref),
                    static_cast<int>(command.first_action), static_cast<int>(command.action_count)};
        case TAYM_CMD_MODULATE:
            if (! held.on)
                return held;  // the chip ignores it
            if (command.base_timer_value != 0)
                held.base = command.base_timer_value;
            if (command.timer_lane_ref != TAYM_TLAN_UNCHANGED)
                held.lane = laneOf(command.timer_lane_ref);
            if (command.action_count != 0) {
                held.actionFirst = static_cast<int>(command.first_action);
                held.actionCount = static_cast<int>(command.action_count);
            }
            return held;
        case TAYM_CMD_STOP:
            held.on = false;
            return held;
        default:
            return held;
    }
}

//==============================================================================
TimerCC::Messages TimerEncoder::encode(int timer, const TaymMods& command) noexcept {
    TimerCC::Messages out;
    if (! TimerCC::fits(timer, command) || command.command == TAYM_CMD_EMPTY)
        return out;

    const auto index = static_cast<size_t>(timer);
    const int channel = TimerCC::firstChannel + timer;
    const auto sent = held_[index];
    const auto next = TimerCC::apply(sent, command);
    const bool starts = command.command == TAYM_CMD_START;

    // What the chip holds is only known once this encoder has told it every value
    const bool all = starts && ! told_[index];
    if (all || next.base != sent.base)
        addBase(out, channel, next.base);
    if (all || next.lane != sent.lane) {
        out.add(channel, laneCoarse, coarseOf(next.lane));
        out.add(channel, laneFine, fineOf(next.lane));
    }
    if (all || next.actionFirst != sent.actionFirst) {
        out.add(channel, actionCoarse, coarseOf(next.actionFirst));
        out.add(channel, actionFine, fineOf(next.actionFirst));
    }
    if (all || next.actionCount != sent.actionCount)
        out.add(channel, actionCount, next.actionCount);
    if (all || next.on != sent.on || (! told_[index] && command.command == TAYM_CMD_STOP))
        out.add(channel, onSwitch, next.on ? 1 : 0);
    if (starts) {
        out.add(channel, startSwitch, 1);
        out.add(channel, startSwitch, 0);
        told_[index] = true;
    }
    held_[index] = next;
    return out;
}

void TimerEncoder::skip(int timer, const TaymMods& command) noexcept {
    if (TimerCC::fits(timer, command))
        held_[static_cast<size_t>(timer)] = TimerCC::apply(held_[static_cast<size_t>(timer)], command);
}

TimerCC::Messages TimerEncoder::restart(int timer) noexcept {
    if (! juce::isPositiveAndBelow(timer, TimerCC::maxTimers) || ! held_[static_cast<size_t>(timer)].on)
        return {};
    const auto index = static_cast<size_t>(timer);
    const auto& held = held_[index];
    TaymMods start {};
    start.command = TAYM_CMD_START;
    start.base_timer_value = held.base;
    start.timer_lane_ref = timerLaneRefOf(held.lane);
    start.first_action = static_cast<uint32_t>(held.actionFirst);
    start.action_count = static_cast<uint8_t>(held.actionCount);
    told_[index] = false;  // the chip was sent none of what was skipped
    return encode(timer, start);
}

//==============================================================================
TimerCC::Values& TimerDecoder::touch(int timer) noexcept {
    const auto index = static_cast<size_t>(timer);
    const auto bit = uint32_t {1} << timer;
    if ((touched_ & bit) == 0) {
        frame_[index] = held_[index];
        touched_ |= bit;
    }
    return frame_[index];
}

bool TimerDecoder::decode(int channel, int controller, int value) noexcept {
    const auto type = static_cast<MidiCCType>(controller);

    if (channel == bankChannel) {
        if (type != bankCoarse && type != bankFine)
            return false;
        if (! bankTouched_) {
            frameBank_ = bank_;
            bankTouched_ = true;
        }
        frameBank_ = withBits(frameBank_, type == bankCoarse ? 7 : 0, value);
        return true;
    }

    const int timer = channel - TimerCC::firstChannel;
    if (! juce::isPositiveAndBelow(timer, TimerCC::maxTimers))
        return false;

    switch (type) {
        case onSwitch:      touch(timer).on = value != 0; break;
        case startSwitch:
            if (value != 0) {  // the release does nothing
                touch(timer);
                starts_ |= uint32_t {1} << timer;
            }
            break;
        case baseTop:       { auto& v = touch(timer); v.base = (v.base & 0x0FFFFFFFu) | (static_cast<uint32_t>(value & 0xF) << 28); break; }
        case baseHigh:      { auto& v = touch(timer); v.base = withBits(v.base, 21, value); break; }
        case baseHighFine:  { auto& v = touch(timer); v.base = withBits(v.base, 14, value); break; }
        case baseLow:       { auto& v = touch(timer); v.base = withBits(v.base, 7, value); break; }
        case baseLowFine:   { auto& v = touch(timer); v.base = withBits(v.base, 0, value); break; }
        case laneCoarse:    { auto& v = touch(timer); v.lane = withBits(v.lane, 7, value); break; }
        case laneFine:      { auto& v = touch(timer); v.lane = withBits(v.lane, 0, value); break; }
        case actionCoarse:  { auto& v = touch(timer); v.actionFirst = withBits(v.actionFirst, 7, value); break; }
        case actionFine:    { auto& v = touch(timer); v.actionFirst = withBits(v.actionFirst, 0, value); break; }
        case actionCount:   touch(timer).actionCount = value; break;
        default:            return false;
    }
    return true;
}

std::array<TaymMods, TimerCC::maxTimers> TimerDecoder::takeCommands() noexcept {
    std::array<TaymMods, TimerCC::maxTimers> commands {};  // TAYM_CMD_EMPTY is 0

    for (int timer = 0; touched_ != 0 && timer < TimerCC::maxTimers; ++timer) {
        const auto bit = uint32_t {1} << timer;
        if ((touched_ & bit) == 0)
            continue;
        const auto index = static_cast<size_t>(timer);
        auto& held = held_[index];
        auto now = frame_[index];
        auto& command = commands[index];

        // A timer that comes on with no start was stopped by a reset while it ran, and its values are stated
        // again where play resumes: it starts there, from its lanes' first elements. A start of a timer the
        // frame leaves off starts nothing
        if (now.on && ((starts_ & bit) != 0 || ! held.on)) {
            command.command = TAYM_CMD_START;
            command.base_timer_value = now.base;
            command.timer_lane_ref = timerLaneRefOf(now.lane);
            command.first_action = static_cast<uint32_t>(now.actionFirst);
            command.action_count = static_cast<uint8_t>(now.actionCount);
        } else if (held.on && ! now.on) {
            command.command = TAYM_CMD_STOP;
        } else if (held.on && now.on && now != held) {
            // Values that changed with no start
            command.command = TAYM_CMD_MODULATE;
            command.base_timer_value = now.base != held.base ? now.base : 0;
            command.timer_lane_ref = now.lane != held.lane ? timerLaneRefOf(now.lane) : TAYM_TLAN_UNCHANGED;
            if (now.actionFirst != held.actionFirst || now.actionCount != held.actionCount) {
                command.first_action = static_cast<uint32_t>(now.actionFirst);
                command.action_count = static_cast<uint8_t>(now.actionCount);
            }
        }
        held = now;
    }
    touched_ = 0;
    starts_ = 0;
    return commands;
}

std::optional<int> TimerDecoder::takeBank() noexcept {
    if (! bankTouched_)
        return {};
    bankTouched_ = false;
    if (frameBank_ == bank_)
        return {};
    bank_ = frameBank_;
    return bank_;
}

void TimerDecoder::reset() noexcept {
    held_ = {};
    touched_ = 0;
    starts_ = 0;
}

}  // namespace MoTool
