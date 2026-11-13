#include <JuceHeader.h>

#include "ChipTimers.h"
#include "../../../models/TimerTestBank.h"

#include <array>
#include <span>
#include <vector>

namespace MoTool::Tests {

using namespace MoTool::uZX;

namespace {

/** A chip that is only its registers. */
struct RegisterChip {
    std::array<uint8_t, 14> regs {};
    uint8_t getRegister(size_t index) const noexcept { return regs[index]; }
    void setRegister(size_t index, uint8_t value) noexcept { regs[index] = value; }
};

constexpr uint64_t interval = 16 * 222;

/** Every write before `endCycle`, also written to the chip. */
std::vector<TaymTimerWrite> collectAll(ChipTimers& timers, uint64_t endCycle, RegisterChip& chip) {
    std::vector<TaymTimerWrite> all;
    for (bool more = true; more;) {
        std::span<const TaymTimerWrite> writes;
        more = timers.collect(endCycle, false, writes);
        for (const auto& write : writes) {
            chip.setRegister(write.target, static_cast<uint8_t>(write.value));
            all.push_back(write);
        }
    }
    return all;
}

ChipTimers::Commands only(size_t timer, const TaymMods& command) {
    ChipTimers::Commands commands {};
    commands[timer] = command;
    return commands;
}

}  // namespace

//==============================================================================
class ChipTimersTests : public juce::UnitTest {
public:
    ChipTimersTests() : UnitTest("ChipTimers", "MoTool") {}

    void runTest() override {
        beginTest("Chip time puts a frame's cycle in its own sample, and no write before its sample");
        {
            for (const auto& time : {ChipTime {1773400, 44100}, ChipTime {1750000, 48000}, ChipTime {2000000, 96000},
                                     ChipTime {894887, 192000}}) {
                for (uint64_t sample : {uint64_t {0}, uint64_t {1}, uint64_t {2}, uint64_t {881}, uint64_t {882},
                                        uint64_t {44100}, uint64_t {123456789}, uint64_t {1} << 36}) {
                    expect(time.sampleAt(time.cycleAt(sample)) == sample, "a frame's cycle sounds from its sample");
                    expect(time.sampleAt(time.firstCycleOf(sample)) == sample, "the sample's first cycle is its own");
                    expect(time.firstCycleOf(sample) <= time.cycleAt(sample));
                    if (sample > 0)
                        expect(time.sampleAt(time.firstCycleOf(sample) - 1) < sample, "the cycle before belongs to an earlier sample");
                }
            }
        }

        beginTest("With no bank a frame's registers go straight to the chip");
        {
            ChipTimers timers;
            RegisterChip chip;
            timers.applyFrame({}, PsgRegsFrame {{PsgRegType::VolumeA, 12}, {PsgRegType::Mixer, 0x38}}, 0, chip);
            expectEquals(int(chip.regs[8]), 12);
            expectEquals(int(chip.regs[7]), 0x38);
            expect(timers.isIdle());
            timers.applyFrame(only(0, timerStart(222, 0)), {}, 0, chip);
            expect(timers.isIdle(), "a start with no bank starts nothing");
        }

        beginTest("A started timer writes its lane at whole intervals from the frame's cycle");
        {
            ChipTimers timers;
            RegisterChip chip;
            timers.setClock(1773400);
            timers.setBank(makeTestBank());
            expect(timers.isIdle());

            timers.applyFrame(only(0, timerStart(222, 0)), {}, 1000, chip);
            expect(! timers.isIdle());
            expect(timers.owns(8) && ! timers.owns(9));

            const auto writes = collectAll(timers, 1000 + 2 * interval + 1, chip);
            expectEquals(writes.size(), size_t {3});
            if (writes.size() == 3) {
                expect(writes[0].cycle == 1000 && writes[0].target == 8 && writes[0].value == 15);
                expect(writes[1].cycle == 1000 + interval && writes[1].value == 9);
                expect(writes[2].cycle == 1000 + 2 * interval && writes[2].value == 15);
            }
            expectEquals(collectAll(timers, 1000 + 2 * interval + 1, chip).size(), size_t {0}, "nothing twice");
        }

        beginTest("The background's write to an owned register is held, and reaches the chip when the timer stops");
        {
            ChipTimers timers;
            RegisterChip chip;
            timers.setBank(makeTestBank());
            chip.regs[8] = 10;
            chip.regs[9] = 11;

            timers.applyFrame(only(0, timerStart(222, 0)), {}, 0, chip);
            collectAll(timers, 1, chip);
            expectEquals(int(chip.regs[8]), 15, "the timer's value");

            timers.applyFrame({}, PsgRegsFrame {{PsgRegType::VolumeA, 12}, {PsgRegType::VolumeB, 5}}, 100, chip);
            expectEquals(int(chip.regs[8]), 15, "the background's write to R8 is held back");
            expectEquals(int(chip.regs[9]), 5, "its write to a free register goes through");

            timers.applyFrame(only(0, timerStop()), {}, 200, chip);
            expectEquals(int(chip.regs[8]), 12, "the held write reaches the chip");
            expect(timers.isIdle() && ! timers.owns(8));
        }

        beginTest("A register taken with no background write since gets back what the chip held");
        {
            ChipTimers timers;
            RegisterChip chip;
            timers.setBank(makeTestBank());
            chip.regs[8] = 10;
            timers.applyFrame(only(0, timerStart(222, 0)), {}, 0, chip);
            collectAll(timers, interval + 1, chip);
            expectEquals(int(chip.regs[8]), 9);
            timers.applyFrame(only(0, timerStop()), {}, 2 * interval, chip);
            expectEquals(int(chip.regs[8]), 10);
        }

        beginTest("A register that changes hands in one frame is written by the background once the new timer stops");
        {
            ChipTimers timers;
            RegisterChip chip;
            timers.setBank(makeTestBank());
            chip.regs[8] = 10;
            timers.applyFrame(only(0, timerStart(222, 0)), {}, 0, chip);

            ChipTimers::Commands handoff {};
            handoff[0] = timerStop();
            handoff[1] = timerStart(222, 1);  // R8, always 7
            timers.applyFrame(handoff, PsgRegsFrame {{PsgRegType::VolumeA, 3}}, 100, chip);
            expect(timers.owns(8));
            const auto writes = collectAll(timers, 101, chip);
            expectEquals(writes.size(), size_t {1});
            expectEquals(int(chip.regs[8]), 7, "the new owner's value");

            timers.applyFrame(only(1, timerStop()), {}, 200, chip);
            expectEquals(int(chip.regs[8]), 3, "the write that came during the handoff");
        }

        beginTest("A new bank, another clock or a reset stops the timers");
        {
            ChipTimers timers;
            RegisterChip chip;
            timers.setBank(makeTestBank());
            timers.applyFrame(only(0, timerStart(222, 0)), {}, 0, chip);
            expect(! timers.isIdle());
            timers.reset();
            expect(timers.isIdle() && ! timers.owns(8), "reset");

            timers.applyFrame(only(0, timerStart(222, 0)), {}, 0, chip);
            timers.setClock(1750000);
            expect(timers.isIdle(), "another clock");

            timers.applyFrame(only(0, timerStart(222, 0)), {}, 0, chip);
            timers.setBank(makeTestBank());
            expect(timers.isIdle(), "a new bank");
            timers.setBank({});
            timers.applyFrame(only(0, timerStart(222, 0)), {}, 0, chip);
            expect(timers.isIdle(), "no bank");
        }

        beginTest("A command for an action the bank does not have changes nothing");
        {
            ChipTimers timers;
            RegisterChip chip;
            timers.setBank(makeTestBank());
            timers.applyFrame(only(0, timerStart(222, 99)), {}, 0, chip);
            expect(timers.isIdle() && ! timers.owns(8));
        }

        beginTest("A fast timer's writes come in parts, none lost");
        {
            ChipTimers timers;
            RegisterChip chip;
            timers.setBank(makeTestBank());
            timers.applyFrame(only(0, timerStart(1, 0)), {}, 0, chip);  // every 16 cycles
            const auto writes = collectAll(timers, 16 * 1000, chip);
            expectEquals(writes.size(), size_t {1000});
            bool inOrder = true;
            for (size_t i = 0; i < writes.size(); ++i)
                inOrder = inOrder && writes[i].cycle == 16 * i && writes[i].value == (i % 2 == 0 ? 15u : 9u);
            expect(inOrder, "every expiry, once, in order");
        }

        beginTest("A bank written without commands and frame data reads back with the same tables");
        {
            const auto original = makeTestBank();
            const auto bytes = original->toBankBytes();
            expect(bytes.getSize() > 0);
            const auto again = TimerBank::fromTaym(bytes.getData(), bytes.getSize(), 0);
            expect(again != nullptr);
            if (again != nullptr) {
                expectEquals(again->getNumTimers(), size_t {2});
                expectEquals(again->getTables().action_count, original->getTables().action_count);
                expectEquals(again->getTables().lane_count, original->getTables().lane_count);
                expectEquals(int(again->getTaym().trak.frame_count), 1);
            }
            expect(TimerBank::fromTaym(bytes.getData(), bytes.getSize(), 1) == nullptr, "a chip the file does not have");
            expect(TimerBank::fromTaym("not a taym", 10, 0) == nullptr);
        }

        beginTest("A registered bank is found by its number until its registration goes");
        {
            auto& registry = TimerBankRegistry::getInstance();
            const auto bank = makeTestBank();
            int number = 0;
            {
                const auto registration = registry.add(bank);
                number = registration->getNumber();
                expect(number > 0);
                expect(registry.find(number) == bank);
                const auto other = registry.add(makeTestBank());
                expect(other->getNumber() != number, "each registration has its own number");
            }
            expect(registry.find(number) == nullptr);
            expect(registry.find(0) == nullptr);
        }
    }
};

static ChipTimersTests chipTimersTests;

}  // namespace MoTool::Tests
