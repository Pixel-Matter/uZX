#include "TaymFile.h"

#include "../psg/PsgFile.h"
#include "../../models/TimerBank.h"
#include "../../models/TimerCC.h"

#include <taym/taym.h>
#include <taym/taym_timers.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

namespace MoTool::uZX {

namespace {

/** A parsed TAYM, freed with its scope. */
struct ParsedTaym {
    ParsedTaym() { taym_init(&taym); }
    ~ParsedTaym() { taym_free(&taym); }
    Taym taym;
    JUCE_DECLARE_NON_COPYABLE(ParsedTaym)
};

/** A value of the file's metadata, key=value strings each ended by a zero; empty when the key is not there. */
juce::String readInfo(const Taym& taym, juce::StringRef key) {
    const auto* p = reinterpret_cast<const char*>(taym.info);
    const auto* end = p + taym.info_size;
    while (p != nullptr && p < end && *p != 0) {
        const auto length = static_cast<size_t>(std::find(p, end, '\0') - p);
        const auto pair = juce::String::fromUTF8(p, static_cast<int>(length));
        if (pair.upToFirstOccurrenceOf("=", false, false) == key)
            return pair.fromFirstOccurrenceOf("=", false, false);
        p += length + 1;
    }
    return {};
}

/** The chip's frames: its PSG chunk read as a PSG file, made the track's length.

    A pause in a PSG counts in fours, but TAYM's own tools write and read it in ones. The track says how many
    frames there are, so the reading that gives that many is the right one: fours first, as the format has it. */
PsgData readFrames(const Taym& taym, const TaymChip& chip) {
    PsgFile psg {juce::File()};
    if (! taym_tag_is_zero(chip.frame_data_tag)) {
        if (const auto* chunk = taym_find_chunk(&taym, chip.frame_data_tag)) {
            psg.readFrom(chunk->data, chunk->size);
            if (psg.getData().frames.size() != taym.trak.frame_count) {
                PsgFile inOnes {juce::File()};
                if (inOnes.readFrom(chunk->data, chunk->size, 1) && inOnes.getData().frames.size() == taym.trak.frame_count)
                    psg.getData() = std::move(inOnes.getData());
            }
        }
    }

    PsgData data = std::move(psg.getData());
    data.options = {taym_from_fix16(taym.trak.frame_rate), 1};
    data.frames.resize(std::min(static_cast<size_t>(taym.trak.frame_count), PsgFile::maxFrames));
    return data;
}

/** True if a modulate's actions name every target the timer has. */
bool namesEveryTarget(const TaymMods& modulate, const TaymTimerState& timer, const TaymTimerTables& tables) {
    for (size_t i = 0; i < timer.target_count; ++i) {
        const auto* first = tables.actions + modulate.first_action;
        const auto id = timer.targets[i].target_id;
        if (std::none_of(first, first + modulate.action_count, [id](const TaymActn& a) { return a.target_id == id; }))
            return false;
    }
    return true;
}

/** The chip's timers: its bank, and every command of its timers by frame.

    A modulate patches the targets it names, but the wire carries a timer's actions as a slice it holds, and
    a timer stated again where play resumes takes that slice for all its targets. So a modulate that names
    some of its timer's targets is given a slice that names all of them, with each one's source as the
    modulate leaves it: actions added after the file's, once for each such slice. The chip plays it alike,
    since a target named with the source it has keeps it. */
TrackerModule::Timers readTimers(const Taym& taym, const juce::MemoryBlock& bytes, size_t chipIndex, size_t frames) {
    TrackerModule::Timers timers;
    const auto bank = TimerBank::fromTaym(bytes.getData(), bytes.getSize(), chipIndex);
    if (bank == nullptr) {
        // more timers than a chip has channels for
        for (size_t i = 0; i < taym.timer_count; ++i)
            timers.unplayable = timers.unplayable || taym.timers[i].chip_index == chipIndex;
        return timers;
    }
    if (bank->getNumTimers() == 0)
        return timers;

    // The timers as the engine runs the commands, for the targets each has
    TaymTimerBank engine {};
    taym_timers_init(&engine, &bank->getTables(), bank->getTimerClocks(), bank->getNumTimers(), bank->getClockHz());
    std::vector<TaymActn> moreActions;
    std::map<std::vector<uint64_t>, uint32_t> slices;  // each added slice's first action, by its actions
    const auto sliceOf = [&](const TaymTimerState& timer) {
        std::vector<uint64_t> key;
        for (size_t i = 0; i < timer.target_count; ++i) {
            const auto& target = timer.targets[i];
            const uint64_t operand = target.has_lane ? target.lane_index : target.inline_value;
            key.push_back(operand << 16 | uint64_t {target.target_id} << 8 | target.has_lane);
        }
        const auto [found, added] = slices.try_emplace(key, static_cast<uint32_t>(taym.action_count + moreActions.size()));
        if (added)
            for (const auto action : key)
                moreActions.push_back({static_cast<uint32_t>(action >> 16), static_cast<uint8_t>(action >> 8),
                                       static_cast<uint8_t>(action & 1 ? TAYM_SRC_BIND_LANE : TAYM_SRC_INLINE_VALUE)});
        return found->second;
    };

    timers.chipIndex = static_cast<int>(chipIndex);
    for (size_t frame = 0; frame < frames; ++frame) {
        for (size_t timer = 0; timer < bank->getNumTimers(); ++timer) {
            const auto record = frame * taym.timer_count + bank->getFileTimerIndex(timer);
            if (record >= taym.mod_count || taym.mods[record].command == TAYM_CMD_EMPTY)
                continue;
            auto command = taym.mods[record];
            const auto& state = engine.timers[timer];
            if (taym_timers_command(&engine, timer, &command, 0) == TAYM_OK && command.command == TAYM_CMD_MODULATE
                && command.action_count != 0 && state.active && ! namesEveryTarget(command, state, bank->getTables())) {
                command.first_action = sliceOf(state);
                command.action_count = static_cast<uint8_t>(state.target_count);
            }
            if (! TimerCC::fits(static_cast<int>(timer), command)) {
                timers.unplayable = true;  // an index past what the wire carries
                continue;
            }
            timers.events.push_back({static_cast<int>(frame), static_cast<int>(timer), command});
        }
    }
    if (! timers.events.empty())
        timers.bank = bank->toBankBytes(moreActions);
    return timers;
}

}  // namespace

bool looksLikeTaym(const juce::MemoryBlock& bytes) {
    return bytes.getSize() >= 4 && std::memcmp(bytes.getData(), "TAYM", 4) == 0;
}

std::optional<TrackerModule> readTaymModule(const juce::MemoryBlock& bytes) {
    if (! looksLikeTaym(bytes))
        return {};
    ParsedTaym parsed;
    const auto& taym = parsed.taym;
    if (taym_read_bytes(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), &parsed.taym) != TAYM_OK)
        return {};
    if (taym.trak.frame_count == 0 || taym.trak.frame_rate == 0)
        return {};

    TrackerModule module;
    for (size_t i = 0; i < taym.chip_count; ++i) {
        const auto& chip = taym.chips[i];
        if (chip.chip_type_id != TAYM_CHIP_TYPE_AY)
            continue;
        auto& out = module.chips.emplace_back();
        out.psg = readFrames(taym, chip);
        if (chip.clock_hz != 0)
            out.clock = static_cast<double>(chip.clock_hz);
        out.type = chip.variant == TAYM_AY_VARIANT_YM ? ChipType {TypeEnum::YM} : ChipType {TypeEnum::AY};
        out.timers = readTimers(taym, bytes, i, out.psg.frames.size());
    }
    if (module.chips.empty())
        return {};

    module.title = readInfo(taym, "title");
    return module;
}

const juce::StringArray& getTaymExtensions() {
    static const juce::StringArray extensions {"taym"};
    return extensions;
}

}  // namespace MoTool::uZX
