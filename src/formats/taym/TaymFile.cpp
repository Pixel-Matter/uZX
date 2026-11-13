#include "TaymFile.h"

#include "../psg/PsgFile.h"
#include "../../models/TimerBank.h"
#include "../../models/TimerCC.h"

#include <taym/taym.h>

#include <algorithm>
#include <cstring>

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

/** The chip's frames: its PSG chunk read as a PSG file, made the track's length. */
PsgData readFrames(const Taym& taym, const TaymChip& chip) {
    PsgFile psg {juce::File()};
    if (! taym_tag_is_zero(chip.frame_data_tag))
        if (const auto* chunk = taym_find_chunk(&taym, chip.frame_data_tag))
            psg.readFrom(chunk->data, chunk->size);

    PsgData data = std::move(psg.getData());
    data.options = {taym_from_fix16(taym.trak.frame_rate), 1};
    data.frames.resize(std::min(static_cast<size_t>(taym.trak.frame_count), PsgFile::maxFrames));
    return data;
}

/** The chip's timers: its bank, and every command of its timers by frame. */
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

    timers.chipIndex = static_cast<int>(chipIndex);
    for (size_t frame = 0; frame < frames; ++frame) {
        for (size_t timer = 0; timer < bank->getNumTimers(); ++timer) {
            const auto record = frame * taym.timer_count + bank->getFileTimerIndex(timer);
            if (record >= taym.mod_count || taym.mods[record].command == TAYM_CMD_EMPTY)
                continue;
            if (! TimerCC::fits(static_cast<int>(timer), taym.mods[record])) {
                timers.unplayable = true;  // an index past what the wire carries
                continue;
            }
            timers.events.push_back({static_cast<int>(frame), static_cast<int>(timer), taym.mods[record]});
        }
    }
    if (! timers.events.empty())
        timers.bank = bank->toBankBytes();
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
        if (module.chips.empty()) {  // the first chip's stand for the tune, as a module's do
            if (chip.clock_hz != 0)
                module.chipClock = static_cast<double>(chip.clock_hz);
            module.chipType = chip.variant == TAYM_AY_VARIANT_YM ? ChipType {TypeEnum::YM} : ChipType {TypeEnum::AY};
        }
        module.chips.push_back(readFrames(taym, chip));
        module.timers.push_back(readTimers(taym, bytes, i, module.chips.back().frames.size()));
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
