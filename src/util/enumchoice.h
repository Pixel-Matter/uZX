#pragma once

#include <limits>
#include <string>
#include <iostream>
#include <string_view>
#include "../../../modules/3rd_party/magic_enum/tracktion_magic_enum.hpp"

namespace MoTool::Util {

// Helper function to convert C-style array to std::array with automatic size deduction
template<typename T, size_t N>
constexpr auto to_array(const T (&arr)[N]) -> std::array<T, N> {
    std::array<T, N> result{};
    for (size_t i = 0; i < N; ++i) {
        result[i] = arr[i];
    }
    return result;
}

// C++20 concept to enforce requirements on enum types used with EnumChoice
template <typename E>
concept EnumChoiceConcept = requires {
    // Must have a nested enum type called Enum
    typename E::Enum;
    typename E::UnderlyingType;

    // Enum must be an actual enum type
    requires std::is_enum_v<typename E::Enum>;

    // Must have a static getLabels() method that returns an array of string_view
    { E::getLabels() };

    // Labels must be indexable with enum values
    requires requires(typename E::Enum value) {
        E::getLabels()[static_cast<std::size_t>(value)];
    };

    requires requires(E value) {
        value.getLabel();
    };
};

template<typename T>
struct is_enum_choice : std::false_type {};

template<typename E>
    requires EnumChoiceConcept<E>
struct is_enum_choice<E> : std::true_type {};

//==============================================================================
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

    // constexpr operator juce::String() const noexcept { return juce::String(std::string(getLabel())); }

    // constexpr operator juce::StringRef() const noexcept { return juce::StringRef(getLabel().data()); }

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

    constexpr juce::StringRef getLabelStringRef() const noexcept {
        return juce::StringRef(getLabel().data());
    }

    constexpr static std::string_view getLabel(size_t i) noexcept {
        return magic_enum::enum_name<Enum>(static_cast<Enum>(i));
    }

    constexpr static auto getLabels() noexcept {
        return magic_enum::enum_names<Enum>();
    }

    constexpr auto getLongLabel() const noexcept {
        if constexpr (requires { E::longLabels[value]; }) {
            return E::longLabels[value];
        } else {
            return magic_enum::enum_name<Enum>(value);
        }
    }

    constexpr static auto getLongLabels() noexcept {
        if constexpr (requires { E::longLabels; }) {
            return to_array(E::longLabels);
        } else {
            return magic_enum::enum_names<Enum>();
        }
    }

    constexpr auto getShortLabel() const noexcept {
        if constexpr (requires { E::shortLabels[value]; }) {
            return E::shortLabels[value];
        } else {
            return magic_enum::enum_name<Enum>(value);
        }
    }

    constexpr static auto getShortLabels() noexcept {
        if constexpr (requires { E::shortLabels; }) {
            return E::shortLabels;
        } else {
            return magic_enum::enum_names<Enum>();
        }
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
        if constexpr (requires { E::labels; }) {
            if (value < 0 || value >= std::size(E::labels))
                return magic_enum::customize::default_tag;
            return E::labels[value];
        } else {
            return magic_enum::customize::default_tag;
        }
    }
};

template <class E>
inline std::ostream& operator<<(std::ostream& out, EnumChoice<E> choice) {
    out << choice.getLabel();
    return out;
}

/**
    A helper type that can be used to implement specialisations of VariantConverter that use
    EnumChoice

    @code
    template <>
    struct juce::VariantConverter<YourEnumChoice> : public EnumVariantConverter<YourEnumChoice> {};
    @endcode

    @tags{Core}
*/
template<class E>
struct EnumVariantConverter {
    static E fromVar(const juce::var& v) {
        if (v.isString()) {
            return E(v.toString().toStdString());
        } else if (v.isInt() || v.isInt64() || v.isDouble()) {
            return E(static_cast<int>(v));
        }
        return E();
    }

    static juce::var toVar(E choice) {
        // NOTE Do not use short or lonng labels to be able to read it back
        const std::string_view label = choice.getLabel();
        return juce::String {label.data()};
    }
};

} // namespace MoTool::Util