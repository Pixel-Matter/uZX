#pragma once

#include <JuceHeader.h>

#include "TimerBank.h"

#include <cstring>
#include <vector>

namespace MoTool::Tests {

/** A small TAYM for tests: one AY chip at the ZX clock, with two timers that count the chip's clock in
    sixteens, so a base of 222 is an expiry every 3552 cycles.

        action 0   R8 from lane 0       lane 0   15, 9, looping
        action 1   R8, always 7         lane 1   3, 0, looping
        action 2   R8 from lane 1
        action 3   R9 from lane 0
        action 4   R13, always 0x0C
        action 5   R7, always 0x3F: the mixer, every channel off

    `frames` is the track's length; `commands`, if given, holds a command per timer per frame; `psg`, if
    given, is the chip's frame data, a whole PSG file. */
inline juce::MemoryBlock makeTestTaym(uint32_t frames = 1, const std::vector<TaymMods>& commands = {},
                                      const std::vector<uint8_t>& psg = {}) {
    constexpr size_t numTimers = 2;

    Taym taym;
    taym_init(&taym);
    taym.trak.frame_rate = taym_to_fix16(50.0);
    taym.trak.frame_count = frames;
    taym.trak.loop_frame = TAYM_NO_LOOP;

    TaymChip chip {};
    chip.clock_hz = 1773400;
    chip.chip_type_id = TAYM_CHIP_TYPE_AY;
    std::memcpy(chip.name, "AY", 2);
    taym.chips = &chip;
    taym.chip_count = 1;

    auto frameData = psg;
    TaymChunk chunk {};
    if (! frameData.empty()) {
        std::memcpy(chip.frame_data_tag, "PSG0", 4);
        std::memcpy(chunk.tag, "PSG0", 4);
        chunk.data = frameData.data();
        chunk.size = frameData.size();
        taym.chunks = &chunk;
        taym.chunk_count = 1;
    }

    TaymTimr timers[numTimers] {};
    for (auto& timer : timers) {
        timer.clock_divider = 16;
        timer.clock_mode = TAYM_CLOCK_CHIP_PERIOD;
    }
    taym.timers = timers;
    taym.timer_count = numTimers;

    std::vector<TaymMods> mods(frames * numTimers);
    jassert(commands.empty() || commands.size() == mods.size());
    if (commands.size() == mods.size())
        mods = commands;
    taym.mods = mods.data();
    taym.mod_count = mods.size();

    TaymActn actions[6] {};
    actions[0] = {0, 0x08, TAYM_SRC_BIND_LANE};
    actions[1] = {7, 0x08, TAYM_SRC_INLINE_VALUE};
    actions[2] = {1, 0x08, TAYM_SRC_BIND_LANE};
    actions[3] = {0, 0x09, TAYM_SRC_BIND_LANE};
    actions[4] = {0x0C, 0x0D, TAYM_SRC_INLINE_VALUE};
    actions[5] = {0x3F, 0x07, TAYM_SRC_INLINE_VALUE};
    taym.actions = actions;
    taym.action_count = 6;

    TaymLane lanes[2] {};
    lanes[0] = {0, 2, 0, TAYM_VT_U8};
    lanes[1] = {2, 2, 0, TAYM_VT_U8};
    taym.lanes = lanes;
    taym.lane_count = 2;

    uint8_t values[4] {15, 9, 3, 0};
    taym.vu08 = values;
    taym.vu08_count = 4;

    uint8_t* bytes = nullptr;
    size_t size = 0;
    juce::MemoryBlock block;
    if (taym_write_bytes(&taym, &bytes, &size) == TAYM_OK) {
        block.append(bytes, size);
        taym_free_bytes(bytes);
    }
    return block;
}

/** The test TAYM's tables as a bank for its one chip. */
inline TimerBank::Ptr makeTestBank() {
    const auto bytes = makeTestTaym();
    return TimerBank::fromTaym(bytes.getData(), bytes.getSize(), 0);
}

inline TaymMods timerStart(uint32_t base, uint32_t firstAction, uint8_t actionCount = 1, uint32_t lane = TAYM_TLAN_NONE) {
    TaymMods m {};
    m.command = TAYM_CMD_START;
    m.base_timer_value = base;
    m.timer_lane_ref = lane;
    m.first_action = firstAction;
    m.action_count = actionCount;
    return m;
}

inline TaymMods timerStop() {
    TaymMods m {};
    m.command = TAYM_CMD_STOP;
    return m;
}

}  // namespace MoTool::Tests
