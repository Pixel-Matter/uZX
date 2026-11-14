#include <JuceHeader.h>
#include "TimerCC.h"

#include <vector>

namespace MoTool::Tests {

using namespace MoTool;

namespace {

TaymMods start(uint32_t base, uint32_t lane, uint32_t first, uint8_t count) {
    TaymMods m {};
    m.command = TAYM_CMD_START;
    m.base_timer_value = base;
    m.timer_lane_ref = lane;
    m.first_action = first;
    m.action_count = count;
    return m;
}

TaymMods modulate(uint32_t base, uint32_t lane, uint32_t first = 0, uint8_t count = 0) {
    auto m = start(base, lane, first, count);
    m.command = TAYM_CMD_MODULATE;
    return m;
}

TaymMods stop() {
    TaymMods m {};
    m.command = TAYM_CMD_STOP;
    return m;
}

bool same(const TaymMods& a, const TaymMods& b) {
    if (a.command != b.command)
        return false;
    if (a.command != TAYM_CMD_START && a.command != TAYM_CMD_MODULATE)
        return true;
    return a.base_timer_value == b.base_timer_value && a.timer_lane_ref == b.timer_lane_ref
        && a.action_count == b.action_count && (a.action_count == 0 || a.first_action == b.first_action);
}

juce::String describe(const TaymMods& m) {
    return "cmd " + juce::String(int(m.command)) + " base " + juce::String(m.base_timer_value) + " lane "
         + juce::String(juce::int64(m.timer_lane_ref)) + " actions " + juce::String(m.first_action) + "+"
         + juce::String(int(m.action_count));
}

}  // namespace

//==============================================================================
class TimerCCTests : public juce::UnitTest {
public:
    TimerCCTests() : UnitTest("TimerCC", "MoTool") {}

    void runTest() override {
        beginTest("A timer's commands come back from the wire as they went, a frame at a time");
        {
            TimerEncoder encoder;
            TimerDecoder decoder;
            // Timer 2 through a tune: started, its base and lane changed, its actions changed, stopped, started again
            const std::vector<TaymMods> commands {
                start(222, TAYM_TLAN_NONE, 0, 1),
                modulate(180, TAYM_TLAN_UNCHANGED),
                modulate(0, 7),
                modulate(0, TAYM_TLAN_NONE),
                modulate(0, TAYM_TLAN_UNCHANGED, 5, 2),
                stop(),
                start(0x12345678u, 1144, 76, 3),
                start(0x12345678u, 1144, 76, 3),  // a retrigger with every value as it was
            };
            for (const auto& command : commands) {
                for (const auto& m : encoder.encode(2, command))
                    expect(decoder.decode(m.channel, static_cast<int>(m.controller), m.value), "a timer message is read");
                const auto got = decoder.takeCommands();
                expect(same(got[2], command), "sent " + describe(command) + ", got " + describe(got[2]));
                for (size_t other = 0; other < got.size(); ++other)
                    if (other != 2)
                        expectEquals(int(got[other].command), int(TAYM_CMD_EMPTY), "no other timer gets a command");
            }
        }

        beginTest("A timer has the channel after the chip's four, counted from its number");
        {
            TimerEncoder encoder;
            for (const auto& m : encoder.encode(0, start(100, TAYM_TLAN_NONE, 0, 1)))
                expectEquals(m.channel, 4);
            for (const auto& m : encoder.encode(11, start(100, TAYM_TLAN_NONE, 0, 1)))
                expectEquals(m.channel, 15);
            expectEquals(encoder.encode(12, start(100, TAYM_TLAN_NONE, 0, 1)).count, size_t {0}, "a thirteenth timer has no channel");
        }

        beginTest("A start with nothing changed sends only the start, pressed and released");
        {
            TimerEncoder encoder;
            const auto first = encoder.encode(0, start(222, 3, 10, 2));
            expectEquals(first.count, size_t {13}, "the first start states every value");
            const auto again = encoder.encode(0, start(222, 3, 10, 2));
            expectEquals(again.count, size_t {2});
            expectEquals(static_cast<int>(again.items[0].controller), 83);
            expectEquals(again.items[0].value, 1);
            expectEquals(again.items[1].value, 0);
        }

        beginTest("A changed base goes whole, its least significant part last, where a tone period's is");
        {
            TimerEncoder encoder;
            encoder.encode(0, start(222, TAYM_TLAN_NONE, 0, 1));
            const auto changed = encoder.encode(0, modulate(180, TAYM_TLAN_UNCHANGED));
            expectEquals(changed.count, size_t {5}, "the base goes whole, its last part last");
            expectEquals(static_cast<int>(changed.items[4].controller), 52);
            expectEquals(changed.items[3].value, 180 >> 7);
            expectEquals(changed.items[4].value, 180 & 0x7F);
        }

        beginTest("Values stated again with no start, as a host does when play starts inside a clip, start a timer that was off and leave a running one alone");
        {
            TimerEncoder encoder;
            TimerDecoder decoder;
            const auto began = encoder.encode(1, start(222, 3, 10, 2));

            // A chip that never saw the start, or was reset since, gets what the controllers were left at: the
            // start released
            for (const auto& m : began)
                if (static_cast<int>(m.controller) != 83)
                    decoder.decode(m.channel, static_cast<int>(m.controller), m.value);
            decoder.decode(5, 83, 0);
            const auto resumed = decoder.takeCommands()[1];
            expectEquals(int(resumed.command), int(TAYM_CMD_START), "on with no start, from off: it starts");
            expectEquals(int(resumed.base_timer_value), 222);
            expectEquals(int(resumed.timer_lane_ref), 3);
            expectEquals(int(resumed.first_action), 10);
            expectEquals(int(resumed.action_count), 2);

            // A chip that did see it gets the same values again
            decoder.reset();
            for (const auto& m : began)
                decoder.decode(m.channel, static_cast<int>(m.controller), m.value);
            expectEquals(int(decoder.takeCommands()[1].command), int(TAYM_CMD_START));
            for (const auto& m : began)
                if (static_cast<int>(m.controller) != 83)
                    decoder.decode(m.channel, static_cast<int>(m.controller), m.value);
            expectEquals(int(decoder.takeCommands()[1].command), int(TAYM_CMD_EMPTY), "the same values again change nothing");
        }

        beginTest("A modulate before the encoder's first start is dropped, and a stop is still sent");
        {
            TimerEncoder encoder;
            expectEquals(encoder.encode(0, modulate(180, 2)).count, size_t {0});
            const auto stopped = encoder.encode(0, stop());
            expectEquals(stopped.count, size_t {1});
            expectEquals(static_cast<int>(stopped.items[0].controller), 80);
            expectEquals(stopped.items[0].value, 0);
        }

        beginTest("Commands skipped before where play begins leave each running timer to be started there with all it holds");
        {
            TimerEncoder encoder;
            encoder.skip(0, start(222, 3, 10, 2));
            encoder.skip(0, modulate(180, TAYM_TLAN_UNCHANGED));
            encoder.skip(1, start(100, TAYM_TLAN_NONE, 0, 1));
            encoder.skip(1, stop());
            expectEquals(encoder.restart(1).count, size_t {0}, "a stopped timer is not started");
            expectEquals(encoder.restart(2).count, size_t {0}, "nor one never started");

            TimerDecoder decoder;
            for (const auto& m : encoder.restart(0))
                decoder.decode(m.channel, static_cast<int>(m.controller), m.value);
            const auto started = decoder.takeCommands()[0];
            expect(same(started, start(180, 3, 10, 2)), describe(started));

            // A command of the clip's first frame comes in the same frame: a stop there leaves the timer off
            TimerDecoder another;
            for (const auto& m : encoder.restart(0))
                another.decode(m.channel, static_cast<int>(m.controller), m.value);
            for (const auto& m : encoder.encode(0, stop()))
                another.decode(m.channel, static_cast<int>(m.controller), m.value);
            expectEquals(int(another.takeCommands()[0].command), int(TAYM_CMD_EMPTY), "a start the frame turns off starts nothing");
        }

        beginTest("A reset turns every timer off and forgets its start");
        {
            TimerEncoder encoder;
            TimerDecoder decoder;
            for (const auto& m : encoder.encode(0, start(222, TAYM_TLAN_NONE, 0, 1)))
                decoder.decode(m.channel, static_cast<int>(m.controller), m.value);
            decoder.takeCommands();
            decoder.reset();
            for (const auto& m : encoder.encode(0, modulate(180, TAYM_TLAN_UNCHANGED)))
                decoder.decode(m.channel, static_cast<int>(m.controller), m.value);
            expectEquals(int(decoder.takeCommands()[0].command), int(TAYM_CMD_EMPTY), "a timer that is off takes no modulate");
        }

        beginTest("The bank travels on the chip's fourth channel and is told once");
        {
            TimerDecoder decoder;
            for (const auto& m : TimerCC::encodeBank(300)) {
                expectEquals(m.channel, 3);
                expect(decoder.decode(m.channel, static_cast<int>(m.controller), m.value));
            }
            expect(! decoder.isEmpty());
            expect(decoder.takeBank() == std::optional<int> {300});
            for (const auto& m : TimerCC::encodeBank(300))
                decoder.decode(m.channel, static_cast<int>(m.controller), m.value);
            expect(! decoder.takeBank().has_value(), "the same bank again is no change");
            expect(! decoder.decode(3, 70, 5), "the envelope shape on that channel is not a timer's");
            expect(! decoder.decode(0, 80, 1), "nor is a chip channel's tone switch");
        }

        beginTest("A command with an index the wire cannot carry is not sent");
        {
            TimerEncoder encoder;
            expect(! TimerCC::fits(0, start(100, 20000, 0, 1)), "a timer lane past 14 bits");
            expect(! TimerCC::fits(0, start(100, TAYM_TLAN_NONE, 20000, 1)), "an action past 14 bits");
            expect(TimerCC::fits(0, start(0xFFFFFFFFu, 16382, 16383, 127)), "the largest that fits");
            expectEquals(encoder.encode(0, start(100, 20000, 0, 1)).count, size_t {0});
        }
    }
};

static TimerCCTests timerCCTests;

}  // namespace MoTool::Tests
