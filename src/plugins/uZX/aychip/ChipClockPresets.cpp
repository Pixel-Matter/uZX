#include "ChipClockPresets.h"

namespace MoTool::uZX {

std::vector<std::pair<double, juce::String>> makeChipClockPresets() {
    std::vector<std::pair<double, juce::String>> choices;
    choices.reserve(static_cast<size_t>(ChipClockChoice::size()) - 1);

    auto entries = ChipClockChoice::getClockEntries();
    for (int idx = 0; idx < static_cast<int>(ChipClockChoice::size()); ++idx) {
        auto choice = ChipClockChoice(idx);
        if (entries[idx] != 0)
            choices.emplace_back(entries[idx] / MHz, juce::String(choice.getLongLabel().data()));
    }
    return choices;
}

} // namespace MoTool::uZX

