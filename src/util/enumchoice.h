#pragma once

#include <limits>
#include <string>
#include <iostream>
#include <string_view>
#include "../../../modules/3rd_party/magic_enum/tracktion_magic_enum.hpp"

namespace MoTool::Util {

// Base class for enumerated choices

template <class E>
class EnumChoice : public E {
public:
    using Enum = typename E::Enum;
    using UnderlyingType = typename magic_enum::underlying_type<Enum>::type;

    static_assert(std::is_enum_v<Enum>, "EnumChoice can only be used with enum types");

    Enum value;

    constexpr EnumChoice(Enum v) noexcept : value(v) {}

    // constexpr EnumChoice(UnderlyingType v) noexcept
    //     : value(magic_enum::enum_cast<Enum>(v).value_or(magic_enum::enum_value<Enum>(0u)))
    // {}

    constexpr EnumChoice(int v) noexcept
        : value(magic_enum::enum_cast<Enum>(static_cast<UnderlyingType>(v)).value_or(magic_enum::enum_value<Enum>(0u)))
    {}

    // constexpr EnumChoice(size_t v) noexcept
    //     : value(magic_enum::enum_cast<Enum>(static_cast<UnderlyingType>(v)).value_or(magic_enum::enum_value<Enum>(0u)))
    // {}

    constexpr EnumChoice(std::string_view v) noexcept
        : value(magic_enum::enum_cast<Enum>(v).value_or(magic_enum::enum_value<Enum>(0u)))
    {}

    constexpr EnumChoice() noexcept : value(magic_enum::enum_value<Enum>(0u)) {}
    constexpr EnumChoice(const EnumChoice&) noexcept = default;
    constexpr EnumChoice(EnumChoice&&) noexcept = default;
    constexpr EnumChoice& operator=(const EnumChoice&) noexcept = default;
    constexpr EnumChoice& operator=(EnumChoice&&) noexcept = default;

    constexpr operator Enum() const noexcept { return value; }
    // constexpr operator UnderlyingType() const noexcept { return magic_enum::enum_underlying(value); }

    template <typename T>
    constexpr T as() const noexcept { return magic_enum::enum_cast<T>(value); }

    // constexpr operator size_t() const noexcept { return magic_enum::enum_underlying(value); }
    // constexpr operator int() const noexcept { return magic_enum::enum_underlying(value); }

    // constexpr operator std::string_view() const noexcept { return getLabel(); }
    // constexpr operator std::string() const noexcept { return std::string(getLabel()); }

    // operator juce::String() const noexcept { return juce::String(std::string(getLabel())); }
    // operator juce::StringRef() const noexcept { return juce::StringRef(getLabel().data()); }

    constexpr bool operator ==(Enum other) const noexcept {
        return value == other;
    }
    constexpr bool operator !=(Enum other) const noexcept {
        return value != other;
    }
    constexpr bool operator ==(const EnumChoice& other) const noexcept {
        return value == other.value;
    }
    constexpr bool operator !=(const EnumChoice& other) const noexcept {
        return value != other.value;
    }

    constexpr bool isValid() const noexcept {
        return magic_enum::enum_contains<Enum>(value);
    }

    constexpr std::string_view getLabel() const noexcept {
        return magic_enum::enum_name<Enum>(value);
    }

    constexpr static std::string_view getLabel(size_t i) noexcept {
        return magic_enum::enum_name<Enum>(static_cast<Enum>(i));
    }

    constexpr static auto getLabels() noexcept {
        return magic_enum::enum_names<Enum>();
    }

    constexpr static auto getLongLabels() noexcept {
        return E::longLabels;
    }

    constexpr auto getLongLabel() noexcept {
        return E::longLabels[value];
    }

    constexpr static size_t size() noexcept {
        return magic_enum::enum_count<Enum>();
    }

    constexpr static Enum undefined() noexcept {
        return static_cast<Enum>(std::numeric_limits<UnderlyingType>::max());
    }

    template <typename Lambda>
    static constexpr auto forEach(Lambda&& lambda) {
        magic_enum::enum_for_each<Enum>(std::forward<Lambda>(lambda));
    }

    inline constexpr magic_enum::customize::customize_t enumNameCustom() noexcept {
        if (value < 0 || value >= std::size(E::labels))
            return magic_enum::customize::default_tag;
        return E::labels[value];
    }
};

template <class E>
inline std::ostream& operator<<(std::ostream& out, EnumChoice<E> choice) {
    out << choice.getLabel();
    return out;
}

} // namespace MoTool::Util
