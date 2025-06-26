#pragma once

#include <JuceHeader.h>
#include <array>
#include <string_view>

using namespace juce;

namespace MoTool {

template <size_t N>
static StringArray toStringArray(const std::array<std::string_view, N>& ch) {
    StringArray result;
    for (const auto& s : ch) {
        result.add(String(s.data(), s.size()));
    }
    return result;
}

}  // namespace MoTool