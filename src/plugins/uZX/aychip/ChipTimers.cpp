#include "ChipTimers.h"

namespace MoTool::uZX {

namespace {
// A timer whose rate is zero fires once a frame, and the chip is not told the frame rate.
// ponytail: 50 Hz; take it from the wire if a tune at another rate ever relies on a rateless timer.
constexpr uint32_t noRateFramesPerSecond = 50;
}  // namespace

void ChipTimers::restart() noexcept {
    owned_ = 0;
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

uint16_t ChipTimers::findOwned() const noexcept {
    uint16_t owned = 0;
    for (size_t reg = 0; reg < PsgRegsFrame::size(); ++reg)
        if (taym_timers_owns_target(&timers_, static_cast<uint8_t>(reg)))
            owned = static_cast<uint16_t>(owned | (1u << reg));
    return owned;
}

void ChipTimers::runCommands(const Commands& commands, uint64_t cycle) noexcept {
    if (timers_.timer_count == 0)
        return;
    bool any = false;
    for (size_t timer = 0; timer < timers_.timer_count; ++timer) {
        if (commands[timer].command == TAYM_CMD_EMPTY)
            continue;
        // A command that names a lane or an action the bank does not have is refused and changes nothing
        taym_timers_command(&timers_, timer, &commands[timer], cycle);
        any = true;
    }
    if (any)
        owned_ = findOwned();
}

bool ChipTimers::collect(uint64_t endCycle, bool ym, std::span<const TaymTimerWrite>& writes) noexcept {
    size_t count = 0;
    const auto result = taym_timers_collect(&timers_, endCycle, writes_.data(), writes_.size(), &count);
    const bool more = result == TAYM_ERROR_RANGE && count > 0;
    if (result != TAYM_OK && ! more)
        reset();  // tables that fail under a running timer: stop, and leave the chip to the background

    count = taym_ay_resolve(writes_.data(), count, ym ? TAYM_AY_VARIANT_YM : TAYM_AY_VARIANT_AY);
    writes = {writes_.data(), count};
    return more;
}

}  // namespace MoTool::uZX
