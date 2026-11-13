#pragma once

#include <JuceHeader.h>

#include "../../../formats/psg/PsgData.h"
#include "../../../models/TimerBank.h"
#include "../../../models/TimerCC.h"

#include <taym/taym_ay.h>
#include <taym/taym_timers.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace MoTool::uZX {

//==============================================================================
/**
    Chip time: the chip's clock cycles against the samples a plugin renders, both counted from the last
    reset, in whole numbers so that cutting time into blocks differently never moves a write.

    A write at a cycle sounds from the first sample at or after it. The clock is faster than the sample
    rate, so every sample has cycles of its own.
*/
struct ChipTime {
    uint64_t clockHz = 1773400;
    uint64_t sampleRate = 44100;

    /** A cycle within `sample`: where something that happens at that sample is on the chip's clock. */
    constexpr uint64_t cycleAt(uint64_t sample) const noexcept { return sample * clockHz / sampleRate; }

    /** The sample a write at `cycle` sounds from. */
    constexpr uint64_t sampleAt(uint64_t cycle) const noexcept { return (cycle * sampleRate + clockHz - 1) / clockHz; }

    /** The first cycle whose write sounds from `sample` or later: writes before it belong to earlier samples. */
    constexpr uint64_t firstCycleOf(uint64_t sample) const noexcept {
        return sample == 0 ? 0 : (sample - 1) * clockHz / sampleRate + 1;
    }
};

//==============================================================================
/**
    The timers of one AY chip: TAYM's timed register writes between frames (PWM, sync-buzzer, samples),
    run by the format's own engine.

    A frame brings each timer at most one command and the frame's ordinary register writes, the background.
    A running timer owns the registers its actions name: the background's writes to those are held back,
    and given to the chip when the timer lets go.

    Nothing here allocates or locks, but a bank's last owner frees it: setBank() on the audio thread can.
    // ponytail: free a replaced bank on the message thread if that ever shows in a profile.
*/
class ChipTimers {
public:
    using Commands = std::array<TaymMods, TimerCC::maxTimers>;

    /** What the timers had to ignore; each is a bit. */
    enum Fault : uint32_t {
        commandWithoutBank = 1,  ///< a start or a modulate came with no bank to read
        commandRefused = 2,      ///< a command named a lane, an action or values its bank does not have
        tablesFailed = 4,        ///< a running timer read past its bank's tables, and all were stopped
    };

    /** The faults since the last call. */
    uint32_t takeFaults() noexcept { return std::exchange(faults_, uint32_t {0}); }

    /** The tables the timers read; null for none. Stops every timer. */
    void setBank(TimerBank::Ptr bank) noexcept;

    /** The chip's clock in Hz. A changed clock stops every timer: their time was counted in the old one. */
    void setClock(uint32_t clockHz) noexcept;

    /** Stops every timer and forgets what they owned, without writing anything. */
    void reset() noexcept;

    /** True if no timer will write: there is nothing to collect. */
    bool isIdle() const noexcept;

    /** True if a timer owns register `reg`. */
    bool owns(size_t reg) const noexcept { return (owned_ & (1u << reg)) != 0; }

    /** Applies one frame at `cycle`: the timers' commands, then the background.

        `chip` needs getRegister(index) and setRegister(index, value). A register a timer owns keeps the
        timer's value and the background's write is held; a register a timer took in this frame has what the
        chip held saved; a register a timer let go of gets the background's write, or what was held for it. */
    template <typename Chip>
    void applyFrame(const Commands& commands, const PsgRegsFrame& background, uint64_t cycle, Chip& chip) noexcept {
        const auto before = owned_;
        runCommands(commands, cycle);
        const auto acquired = static_cast<uint16_t>(owned_ & ~before);
        const auto released = static_cast<uint16_t>(before & ~owned_);

        for (size_t reg = 0; reg < PsgRegsFrame::size(); ++reg) {
            const auto bit = static_cast<uint16_t>(1u << reg);
            if ((acquired & bit) != 0)
                held_[reg] = chip.getRegister(reg);
            if (background.isSet(reg)) {
                if ((owned_ & bit) != 0)
                    held_[reg] = background.getRaw(reg);
                else
                    chip.setRegister(reg, background.getRaw(reg));
            } else if ((released & bit) != 0) {
                chip.setRegister(reg, held_[reg]);
            }
        }
    }

    /** The timers' register writes before `endCycle`, in order, a part at a time: `target` is the register
        and `value` its byte. `ym` picks the chip's volume curve for sample amplitudes.
        Returns true if there are more before `endCycle`; call again. */
    bool collect(uint64_t endCycle, bool ym, std::span<const TaymTimerWrite>& writes) noexcept;

private:
    TimerBank::Ptr bank_;
    TaymTimerBank timers_ {};
    uint32_t clockHz_ = 1773400;
    uint16_t owned_ = 0;                                     // a bit per register a timer owns
    std::array<uint8_t, PsgRegsFrame::size()> held_ {};      // the background's value of each owned register
    std::array<TaymTimerWrite, 128> writes_ {};
    uint32_t faults_ = 0;

    void restart() noexcept;
    void runCommands(const Commands& commands, uint64_t cycle) noexcept;
    uint16_t findOwned() const noexcept;
};

}  // namespace MoTool::uZX
