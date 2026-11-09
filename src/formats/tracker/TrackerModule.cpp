#include "TrackerModule.h"

#include "uzx/ay_module.h"

#include <map>
#include <string_view>

namespace MoTool::uZX {

namespace {

/** zxtune's names of the tables uZX has as presets, the same 96 periods each. zxtune's others
    (ASC's "ASM", ProSoundMaker, SQTracker, FastTracker, Fuxoft, PT 3.3's "Real") have none. */
std::optional<BuiltinTuningType> presetFor(std::string_view zxtuneTable) {
    using E = BuiltinTuningEnum;
    static const std::map<std::string_view, E::Enum> presets {
        {"SoundTracker", E::CustomST},
        {"SoundTrackerPro", E::CustomST},
        {"ProTracker2", E::CustomPT_1_ST},
        {"ProTracker3_ST", E::CustomPT_1_ST},
        {"ProTracker3.3", E::CustomPT_0_PT33},
        {"ProTracker3.4", E::CustomPT_0_PT},
        {"ProTracker3.3_ASM", E::CustomPT_2_ASM33},
        {"ProTracker3.4_ASM", E::CustomPT_2_ASM},
        {"ProTracker3.4_Real", E::CustomPT_3_REAL},
        {"NaturalScaled", E::CustomVT_4_NATURAL},
    };
    const auto found = presets.find(zxtuneTable);
    if (found == presets.end())
        return {};
    return BuiltinTuningType(found->second);
}

}  // namespace

std::optional<TrackerModule> readTrackerModule(const juce::MemoryBlock& bytes) {
    const auto tune = UzxAY::Play(bytes.getData(), bytes.getSize());
    if (! tune)
        return {};

    TrackerModule module;
    for (const auto& played : tune->Chips) {
        auto& chip = module.chips.emplace_back();
        chip.options.frameRate = tune->FrameRate;
        chip.frames.reserve(played.size());
        for (const auto& source : played) {
            PsgRegsFrame frame {};
            for (size_t reg = 0; reg < source.Registers.size(); ++reg) {
                if ((source.Written >> reg) & 1) {
                    frame.registers[reg] = source.Registers[reg];
                    frame.mask[reg] = true;
                }
            }
            chip.frames.push_back(frame);
        }
    }
    module.title = juce::String::fromUTF8(tune->Title.data(), (int) tune->Title.size()).trim();
    module.table = presetFor(tune->NoteTable);
    module.rowStarts = tune->RowStarts;
    if (tune->ChipClock > 0)
        module.chipClock = tune->ChipClock;
    return module;
}

const juce::StringArray& getTrackerModuleExtensions() {
    // zxtune finds the format from the content; these are the names the files usually have.
    // Vortex Tracker II saves its text modules as .vt2, its early versions as .txt. The Player
    // registers all but .txt with Finder (PLAYER_MODULE_EXTENSIONS in src/CMakeLists.txt).
    static const juce::StringArray extensions {"pt3", "pt2", "pt1", "stc", "st1", "st3", "stp", "asc", "as0",
                                               "sqt", "psc", "psm", "gtr", "ftc", "vt2", "txt"};
    return extensions;
}

}  // namespace MoTool::uZX
