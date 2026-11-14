#include "ChipTimers.h"

namespace MoTool::uZX {

namespace {
// A timer whose rate is zero fires once a frame, and the chip is not told the frame rate.
// ponytail: 50 Hz; take it from the wire if a tune at another rate ever relies on a rateless timer.
constexpr uint32_t noRateFramesPerSecond = 50;
}  // namespace

void ChipTimers::restart() noexcept {
    // What the timers owned stays owned until the next frame finds them stopped and gives it back
    timers_ = {};
    if (bank_ == nullptr)
        return;
    if (taym_timers_init(&timers_, &bank_->getTables(), bank_->getTimerClocks(), bank_->getNumTimers(), clockHz_) != TAYM_OK) {
        timers_ = {};
        bank_ = {};
        return;
    }
    timers_.no_rate_interval = clockHz_ / noRateFramesPerSecond;
}

void ChipTimers::setBank(TimerBank::Ptr bank) noexcept {
    bank_ = std::move(bank);
    restart();
}

void ChipTimers::setClock(uint32_t clockHz) noexcept {
    if (clockHz == clockHz_)
        return;
    clockHz_ = clockHz;
    restart();
}

void ChipTimers::reset() noexcept {
    taym_timers_reset(&timers_);
    owned_ = 0;
}

bool ChipTimers::isIdle() const noexcept {
    for (size_t i = 0; i < timers_.timer_count; ++i)
        if (timers_.timers[i].active && ! timers_.timers[i].quiescent)
            return false;
    return true;
}

uint16_t ChipTimers::getRegistersOf(size_t timer) const noexcept {
    if (timer >= timers_.timer_count)
        return 0;
    const auto& state = timers_.timers[timer];
    if (! state.active && ! state.quiescent)
        return 0;
    uint16_t registers = 0;
    for (size_t i = 0; i < state.target_count; ++i)
        if (state.targets[i].target_id < PsgRegsFrame::size())
            registers = static_cast<uint16_t>(registers | (1u << state.targets[i].target_id));
    return registers;
}

void ChipTimers::runCommands(const Commands& commands, uint64_t cycle) noexcept {
    for (size_t timer = 0; timer < commands.size(); ++timer) {
        const auto& command = commands[timer];
        if (command.command == TAYM_CMD_EMPTY)
            continue;
        const bool sets = command.command == TAYM_CMD_START || command.command == TAYM_CMD_MODULATE;
        if (timer >= timers_.timer_count) {
            // no bank, or a timer its chip does not have: nothing to stop, and nothing to start it with
            if (sets)
                faults_ |= bank_ == nullptr ? commandWithoutBank : commandRefused;
            continue;
        }
        // A command that names a lane or an action the bank does not have is refused and changes nothing
        if (taym_timers_command(&timers_, timer, &command, cycle) != TAYM_OK)
            faults_ |= commandRefused;
    }
}

bool ChipTimers::collect(uint64_t endCycle, bool ym, std::span<const TaymTimerWrite>& writes) noexcept {
    size_t count = 0;
    const auto result = taym_timers_collect(&timers_, endCycle, writes_.data(), writes_.size(), &count);
    const bool more = result == TAYM_ERROR_RANGE && count > 0;
    if (result != TAYM_OK && ! more) {
        reset();  // tables that fail under a running timer: stop, and leave the chip to the background
        faults_ |= tablesFailed;
    }

    count = taym_ay_resolve(writes_.data(), count, ym ? TAYM_AY_VARIANT_YM : TAYM_AY_VARIANT_AY);
    writes = {writes_.data(), count};
    return more;
}

}  // namespace MoTool::uZX
