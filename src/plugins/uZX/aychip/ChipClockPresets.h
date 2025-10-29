#pragma once

#include <JuceHeader.h>

#include "aychip.h"

#include <utility>
#include <vector>

namespace MoTool::uZX {

std::vector<std::pair<double, juce::String>> makeChipClockPresets();

} // namespace MoTool::uZX

