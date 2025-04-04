#pragma once

#include <string>
#include <iostream>

namespace MoTool::Util {

// Base class for enumerated choices
template <class E>
class EnumChoice : public E {
public:
    using Enum = typename E::Enum;

    Enum value;

    constexpr EnumChoice(Enum value_) : value(value_) {}
    constexpr EnumChoice(int value_) : value(static_cast<Enum>(value_)) {}
    constexpr EnumChoice() : EnumChoice(0) {}
    constexpr EnumChoice(const EnumChoice&) = default;
    constexpr EnumChoice(EnumChoice&&) = default;

    inline static constexpr auto getLabels() noexcept {
        std::array<std::string_view, std::size(E::labels)> result;
        size_t i = 0;
        for (const auto& label: E::labels) {
            result[i++] = label;
        }
        return result;
    }

    inline static constexpr auto size() noexcept -> size_t  {
        return std::size(E::labels);
    }

    inline static constexpr auto getLabelFor(size_t index) noexcept {
        return getLabels()[index];
    }

    inline constexpr auto getLabel() const noexcept {
        return getLabelFor(value);
    }

    constexpr EnumChoice& operator=(const EnumChoice&) = default;
    constexpr EnumChoice& operator=(EnumChoice&&) = default;
    constexpr operator Enum() const { return value; }
    // NOTE confuses switch statements
    // constexpr operator int() const noexcept { return static_cast<int>(value); }
    constexpr operator std::string_view() const noexcept { return getLabel(); }

};

template <class E>
inline std::ostream& operator<<(std::ostream& out, EnumChoice<E> choice) {
    out << choice.getLabel();
    return out;
}

} // namespace MoTool::Util
