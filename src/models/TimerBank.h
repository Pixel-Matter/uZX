#pragma once

#include <JuceHeader.h>

#include <taym/taym.h>
#include <taym/taym_timers.h>

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <span>

namespace MoTool {

//==============================================================================
/**
    The lanes and actions the timers of one chip read: a TAYM's tables, parsed once and never changed, so
    the audio thread reads them freely. Timer commands name its lanes and actions by index.
*/
class TimerBank {
public:
    using Ptr = std::shared_ptr<const TimerBank>;

    /** Parses a TAYM for the timers of its chip `chipIndex`. Null if the bytes are not a TAYM, the chip
        is not in it, or it has more timers than a chip here can run. */
    static Ptr fromTaym(const void* data, size_t size, size_t chipIndex);

    ~TimerBank();

    /** The clock of the chip the timers belong to, as the file has it, in Hz. */
    uint32_t getClockHz() const noexcept { return taym_.chips[chipIndex_].clock_hz; }

    /** The tables in the engine's terms; they live as long as the bank. */
    const TaymTimerTables& getTables() const noexcept { return tables_; }

    /** Element `index` of the pool of `valueType` (a TAYM_VT_*), widened; 0 past the pool's end. A value
        lane's element `step` is at its value_offset + step, a timer lane's in the 32-bit pool. */
    uint32_t getPoolValue(uint8_t valueType, size_t index) const noexcept;

    /** The chip's timers in the order their commands are numbered on the wire. */
    size_t getNumTimers() const noexcept { return numTimers_; }
    const TaymTimerClock* getTimerClocks() const noexcept { return clocks_.data(); }

    /** The file's index of the chip's timer `timer`: its column among the file's commands. */
    size_t getFileTimerIndex(size_t timer) const noexcept { return fileTimers_[timer]; }

    /** A TAYM with the same chips, timers and tables, `moreActions` after its own actions, and nothing
        else: one frame, no commands, no frame data. A clip keeps this; its commands and frames are the
        clip's own. Empty if it cannot be written. */
    juce::MemoryBlock toBankBytes(std::span<const TaymActn> moreActions = {}) const;

private:
    TimerBank() noexcept;

    Taym taym_;
    size_t chipIndex_ = 0;
    TaymTimerTables tables_ {};
    std::array<TaymTimerClock, TAYM_MAX_TIMERS> clocks_ {};
    std::array<size_t, TAYM_MAX_TIMERS> fileTimers_ {};
    size_t numTimers_ = 0;

    JUCE_DECLARE_NON_COPYABLE(TimerBank)
};

//==============================================================================
/**
    Gives timer banks the numbers the PSG wire chooses them by. A bank does not travel as MIDI: whatever
    plays timer commands registers the bank they index, sends its number, and the chip looks it up here.
    A device on a real wire would be sent the bank ahead of time under the same number.

    One for the process; a number is good for as long as its registration lives.
*/
class TimerBankRegistry {
public:
    static TimerBankRegistry& getInstance();

    /** Keeps a bank findable under a number until it is destroyed. */
    class Registration {
    public:
        ~Registration();
        /** The bank's number, 1 or more; 0 if every number was taken. */
        int getNumber() const noexcept { return number_; }

    private:
        friend class TimerBankRegistry;
        explicit Registration(int number) noexcept : number_(number) {}
        int number_;
        JUCE_DECLARE_NON_COPYABLE(Registration)
    };

    std::unique_ptr<Registration> add(TimerBank::Ptr bank);

    /** The bank registered under a number, or null. Takes a spin lock and allocates nothing. */
    TimerBank::Ptr find(int number) const;

private:
    mutable juce::SpinLock lock_;
    std::map<int, TimerBank::Ptr> banks_;
    int next_ = 1;

    void remove(int number);
};

}  // namespace MoTool
