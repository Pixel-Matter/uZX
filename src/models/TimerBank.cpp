#include "TimerBank.h"
#include "TimerCC.h"

#include <cstring>
#include <vector>

namespace MoTool {

//==============================================================================
TimerBank::TimerBank() noexcept {
    taym_init(&taym_);
}

TimerBank::~TimerBank() {
    taym_free(&taym_);
}

TimerBank::Ptr TimerBank::fromTaym(const void* data, size_t size, size_t chipIndex) {
    std::shared_ptr<TimerBank> bank {new TimerBank()};
    if (taym_read_bytes(static_cast<const uint8_t*>(data), size, &bank->taym_) != TAYM_OK)
        return {};

    const auto& taym = bank->taym_;
    if (chipIndex >= taym.chip_count)
        return {};
    bank->chipIndex_ = chipIndex;

    for (size_t i = 0; i < taym.timer_count; ++i) {
        if (taym.timers[i].chip_index != chipIndex)
            continue;
        if (bank->numTimers_ >= static_cast<size_t>(TimerCC::maxTimers))
            return {};
        bank->clocks_[bank->numTimers_] = {taym.timers[i].clock_divider, taym.timers[i].clock_mode};
        bank->fileTimers_[bank->numTimers_] = i;
        ++bank->numTimers_;
    }

    auto& t = bank->tables_;
    t.actions = taym.actions;
    t.action_count = taym.action_count;
    t.lanes = taym.lanes;
    t.lane_count = taym.lane_count;
    t.tlanes = taym.tlanes;
    t.tlane_count = taym.tlane_count;
    t.vu08 = taym.vu08;
    t.vu08_count = taym.vu08_count;
    t.vu16 = taym.vu16;
    t.vu16_count = taym.vu16_count;
    t.vu32 = taym.vu32;
    t.vu32_count = taym.vu32_count;
    return bank;
}

uint32_t TimerBank::getPoolValue(uint8_t valueType, size_t index) const noexcept {
    const auto& t = tables_;
    switch (valueType) {
        case TAYM_VT_U8:  return index < t.vu08_count ? t.vu08[index] : 0;
        case TAYM_VT_U16: return index < t.vu16_count ? t.vu16[index] : 0;
        case TAYM_VT_U32: return index < t.vu32_count ? t.vu32[index] : 0;
        default:          return 0;
    }
}

juce::MemoryBlock TimerBank::toBankBytes(std::span<const TaymActn> moreActions) const {
    // The same tables, borrowed, under a track of one frame that commands nothing and has no frame data
    Taym bank = taym_;
    bank.trak.frame_count = 1;
    bank.trak.loop_frame = TAYM_NO_LOOP;

    std::vector<TaymActn> actions(taym_.actions, taym_.actions + taym_.action_count);
    actions.insert(actions.end(), moreActions.begin(), moreActions.end());
    bank.actions = actions.data();
    bank.action_count = actions.size();

    std::vector<TaymMods> noCommands(taym_.timer_count);  // zeroed: TAYM_CMD_EMPTY
    bank.mods = noCommands.data();
    bank.mod_count = noCommands.size();

    std::vector<TaymChip> chips(taym_.chips, taym_.chips + taym_.chip_count);
    for (auto& chip : chips)
        std::memset(chip.frame_data_tag, 0, sizeof(chip.frame_data_tag));
    bank.chips = chips.data();

    bank.chunks = nullptr;
    bank.chunk_count = 0;
    bank.info = nullptr;
    bank.info_size = 0;

    uint8_t* bytes = nullptr;
    size_t size = 0;
    if (taym_write_bytes(&bank, &bytes, &size) != TAYM_OK)
        return {};
    juce::MemoryBlock block {bytes, size};
    taym_free_bytes(bytes);
    return block;
}

//==============================================================================
TimerBankRegistry& TimerBankRegistry::getInstance() {
    static TimerBankRegistry registry;
    return registry;
}

TimerBankRegistry::Registration::~Registration() {
    if (number_ != 0)
        getInstance().remove(number_);
}

std::unique_ptr<TimerBankRegistry::Registration> TimerBankRegistry::add(TimerBank::Ptr bank) {
    // The node is made before the lock is taken, so nothing is allocated under it
    std::map<int, TimerBank::Ptr> node;
    int number = 0;
    {
        const juce::SpinLock::ScopedLockType sl(lock_);
        if (banks_.size() < static_cast<size_t>(TimerCC::maxBank)) {
            while (banks_.contains(next_))
                next_ = next_ % TimerCC::maxBank + 1;
            number = next_;
            next_ = next_ % TimerCC::maxBank + 1;
        }
    }
    if (number != 0) {
        node.emplace(number, std::move(bank));
        const juce::SpinLock::ScopedLockType sl(lock_);
        banks_.merge(node);
    }
    return std::unique_ptr<Registration>(new Registration(number));
}

TimerBank::Ptr TimerBankRegistry::find(int number) const {
    const juce::SpinLock::ScopedLockType sl(lock_);
    const auto found = banks_.find(number);
    return found != banks_.end() ? found->second : TimerBank::Ptr {};
}

void TimerBankRegistry::remove(int number) {
    std::map<int, TimerBank::Ptr> node;
    {
        const juce::SpinLock::ScopedLockType sl(lock_);
        if (auto found = banks_.extract(number); ! found.empty())
            node.insert(std::move(found));
    }
    // the bank, if this was its last owner, is freed here, outside the lock
}

}  // namespace MoTool
