#include <JuceHeader.h>

#include "ClipTimers.h"
#include "Ids.h"
#include "TimerTestBank.h"

#include <vector>

namespace MoTool::Tests {

namespace {

TaymMods timerModulate(uint32_t base, uint32_t firstAction = 0, uint8_t actionCount = 0) {
    TaymMods m {};
    m.command = TAYM_CMD_MODULATE;
    m.base_timer_value = base;
    m.timer_lane_ref = TAYM_TLAN_UNCHANGED;
    m.first_action = firstAction;
    m.action_count = actionCount;
    return m;
}

}  // namespace

//==============================================================================
/** What a clip's timers do, a span at a time, worked out from their commands. The test bank's timers count
    the chip's clock in sixteens, so a base of 222 is 3552 cycles an expiry. */
class ClipTimersTests : public juce::UnitTest {
public:
    ClipTimersTests() : UnitTest("ClipTimers", "MoTool") {}

    void runTest() override {
        const auto bank = makeTestTaym();
        juce::ValueTree clipState {"PSGCLIP"};
        ClipTimers timers {clipState};
        timers.set(bank, 0, {
            {10, 0, timerStart(222, 0)},        // R8 from lane 0: 15, 9
            {20, 0, timerModulate(100)},        // twice as fast, and a bit
            {30, 0, timerModulate(0, 1, 1)},    // R8, always 7
            {40, 0, timerStop()},
            {40, 1, timerStart(222, 4)},        // R13, always 0x0C: an envelope restarted every expiry
            {50, 0, timerModulate(180)},        // its timer is stopped: nothing
            {60, 0, timerStart(300, 3)},        // R9 from lane 0, to the end
        }, nullptr);

        beginTest("A span runs from a timer's command to its next, and a stop ends it");
        {
            expectEquals(timers.getNumTimers(), 2);
            const auto& first = timers.getSpans(0);
            expectEquals(first.size(), size_t {4});
            if (first.size() == 4) {
                expect(first[0].startFrame == 10 && first[0].endFrame == 20 && first[0].starts);
                expect(first[1].startFrame == 20 && first[1].endFrame == 30 && ! first[1].starts);
                expect(first[2].startFrame == 30 && first[2].endFrame == 40 && ! first[2].starts);
                expect(first[3].startFrame == 60 && first[3].endFrame == TimerSpan::toTheEnd, "nothing ends the last");
            }
            expect(timers.findSpan(0, 9) == nullptr, "before its start");
            expect(timers.findSpan(0, 10) != nullptr && timers.findSpan(0, 19) == timers.findSpan(0, 10));
            expect(timers.findSpan(0, 45) == nullptr && timers.findSpan(0, 55) == nullptr, "stopped: a modulate changes nothing");
            expect(timers.findSpan(0, 100000) != nullptr, "running to the end");
            expect(timers.findSpan(1, 39) == nullptr && timers.findSpan(1, 40) != nullptr);
            expect(timers.findSpan(5, 40) == nullptr, "a timer the chip does not have");
        }

        beginTest("A span says what its timer holds");
        {
            const auto* volume = timers.findSpan(0, 10);
            const auto* shape = timers.findSpan(1, 40);
            const auto* other = timers.findSpan(0, 60);
            expect(volume != nullptr && shape != nullptr && other != nullptr);
            if (volume != nullptr && shape != nullptr && other != nullptr) {
                expect(volume->channels == 1 && volume->registers == (1u << 8) && ! volume->envelope, "channel A's volume");
                expect(volume->numTargets == 1 && volume->targets[0].id == 8 && volume->targets[0].lane == 0);
                expect(shape->envelope && shape->channels == 0 && shape->registers == (1u << 13), "the envelope's shape");
                expect(shape->targets[0].lane == -1 && shape->targets[0].value == 0x0C);
                expect(other->channels == 2, "channel B's volume");
            }
        }

        beginTest("A span's repeat is the time its values and intervals take to come round, and its duty the time on the highest");
        {
            const auto* pulse = timers.findSpan(0, 10);
            const auto* faster = timers.findSpan(0, 20);
            const auto* steady = timers.findSpan(0, 30);
            const auto* shape = timers.findSpan(1, 40);
            expect(pulse != nullptr && faster != nullptr && steady != nullptr && shape != nullptr);
            if (pulse != nullptr && faster != nullptr && steady != nullptr && shape != nullptr) {
                expectWithinAbsoluteError(pulse->loopCycles, 2.0 * 3552.0, 1.0e-9);     // 15 for one expiry, 9 for the next
                expectWithinAbsoluteError(double(pulse->duty), 0.5, 1.0e-6);
                expectWithinAbsoluteError(faster->loopCycles, 2.0 * 1600.0, 1.0e-9);    // the new base, the same lane
                expect(steady->loopCycles == 0.0 && ! steady->getPitch(), "one value at every expiry is no pitch");
                expect(steady->duty < 0.0f, "no duty where every step is alike");
                expectWithinAbsoluteError(shape->loopCycles, 3552.0, 1.0e-9);           // a restart every expiry
                expectEquals(shape->envelopeShape, 0x0C, "the shape it restarts the envelope with");
                expect(shape->duty < 0.0f, "a restart has no duty: only a volume has");
                expectEquals(pulse->envelopeShape, -1, "a volume is no shape");
            }
        }

        beginTest("A command its bank's tables cannot play has no span, as the chip refuses it");
        {
            // The test bank with lane 1 emptied: action 2 reads it
            juce::MemoryBlock broken;
            Taym taym;
            taym_init(&taym);
            if (taym_read_bytes(static_cast<const uint8_t*>(bank.getData()), bank.getSize(), &taym) == TAYM_OK) {
                taym.lanes[1].length = 0;
                uint8_t* bytes = nullptr;
                size_t size = 0;
                if (taym_write_bytes(&taym, &bytes, &size) == TAYM_OK) {
                    broken.append(bytes, size);
                    taym_free_bytes(bytes);
                }
            }
            taym_free(&taym);

            juce::ValueTree state {"PSGCLIP"};
            ClipTimers withEmptyLane {state};
            withEmptyLane.set(broken, 0, {{10, 0, timerStart(222, 2, 2)}, {20, 1, timerStart(222, 0)}}, nullptr);
            expect(withEmptyLane.getBank() != nullptr, "the bank reads");
            expect(withEmptyLane.getSpans(0).empty(), "a start whose action reads an empty lane is refused");
            expectEquals(withEmptyLane.getSpans(1).size(), size_t {1}, "the rest are as ever");
        }

        beginTest("A clip that loses its timers has no spans");
        {
            timers.set({}, 0, {}, nullptr);
            expectEquals(timers.getNumTimers(), 0);
            expect(timers.findSpan(0, 10) == nullptr);
        }
    }
};

static ClipTimersTests clipTimersTests;

}  // namespace MoTool::Tests
