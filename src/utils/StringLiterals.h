#pragma once

#include <JuceHeader.h>

namespace MoTool {

/** UTF-8 string literal suffix for JUCE String.
 *
 *  Usage:
 *    auto playIcon = "▶"_u;
 *    auto pauseIcon = "⏸"_u;
 *
 *  This ensures proper UTF-8 handling for Unicode characters.
 */
inline juce::String operator""_u(const char* str, std::size_t len) {
    return juce::String::fromUTF8(str, static_cast<int>(len));
}

} // namespace MoTool