#pragma once

#include <JuceHeader.h>
#include <array>
#include <string_view>
#include <utility>

using namespace juce;

namespace MoTool {

template <size_t N>
inline StringArray toStringArray(const std::array<std::string_view, N>& ch) {
    StringArray result;
    for (const auto& s : ch) {
        result.add(String::fromUTF8(s.data(), static_cast<int>(s.size())));
    }
    return result;
}

inline StringArray toStringArray(StringArray&& sa) {
    return std::move(sa);
}

template <std::size_t M, typename T, std::size_t N>
inline constexpr std::array<T, M> truncateArray(const std::array<T, N>& labels) {
    std::array<T, M> limitedLabels{};
    std::ranges::copy_n(labels.begin(), std::min(M, N), limitedLabels.begin());
    return limitedLabels;
}


}  // namespace MoTool
