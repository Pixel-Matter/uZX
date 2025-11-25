#pragma once

#include <limits>
#include <string>
#include <iostream>
#include <string_view>
#include "../../../modules/3rd_party/magic_enum/tracktion_magic_enum.hpp"

namespace MoTool::Util {

// C++20 concept to enforce requirements on enum types used with EnumChoice
template <typename E>
concept EnumStructConcept = requires {
    typename E::Enum;
    requires std::is_enum_v<typename E::Enum>;
};

template <typename E>
concept EnumChoiceConcept = requires {
    // Must have nested types
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
template <EnumStructConcept E>
class EnumChoice : public E {
public:
    using Enum = typename E::Enum;
    using UnderlyingType = typename magic_enum::underlying_type<Enum>::type;

    static_assert(std::is_enum_v<Enum>, "EnumChoice can only be used with enum types");

    constexpr EnumChoice(Enum v) noexcept : value(v) {}
    constexpr EnumChoice() noexcept : value(magic_enum::enum_value<Enum>(0u)) {}
    constexpr EnumChoice(const EnumChoice&) noexcept = default;
    constexpr EnumChoice(EnumChoice&&) noexcept = default;
    constexpr EnumChoice& operator=(const EnumChoice&) noexcept = default;
    constexpr EnumChoice& operator=(EnumChoice&&) noexcept = default;

    // No construction from unsigned integral types to avoid accidental misuse
    constexpr EnumChoice(int v) noexcept
        : value(magic_enum::enum_cast<Enum>(static_cast<UnderlyingType>(v)).value_or(magic_enum::enum_value<Enum>(0u)))
    {}

    constexpr explicit EnumChoice(std::string_view v) noexcept
        : value(magic_enum::enum_cast<Enum>(v).value_or(undefined()))
    {}

    // Conversion operators
    constexpr operator Enum() const noexcept { return value; }

    explicit constexpr operator juce::String() const noexcept { return juce::String(getLabel().data()); }

    constexpr Enum asEnum() const noexcept {
        return value;
    }

    // Get underlying integer value
    constexpr UnderlyingType asUnderlying() const noexcept {
        return static_cast<UnderlyingType>(value);
    }

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

    // Display label - uses custom labels[] if defined, otherwise enum name
    constexpr std::string_view getLabel() const noexcept {
        if constexpr (requires { E::labels; }) {
            auto idx = static_cast<size_t>(value);
            if (idx < std::size(E::labels))
                return E::labels[idx];
        }
        return magic_enum::enum_name<Enum>(value);
    }

    // Serialization name - always pristine magic_enum name (for round-trip parsing)
    constexpr std::string_view getEnumName() const noexcept {
        return magic_enum::enum_name<Enum>(value);
    }

    constexpr static std::string_view getLabel(size_t i) noexcept {
        if constexpr (requires { E::labels; }) {
            if (i < std::size(E::labels))
                return E::labels[i];
        }
        return magic_enum::enum_name<Enum>(static_cast<Enum>(i));
    }

    constexpr static std::string_view getEnumName(size_t i) noexcept {
        return magic_enum::enum_name<Enum>(static_cast<Enum>(i));
    }

    constexpr static size_t size() noexcept {
        return magic_enum::enum_count<Enum>();
    }

    constexpr static auto getLabels() noexcept -> std::array<std::string_view, size()> {
        if constexpr (requires { E::labels; }) {
            return std::to_array(E::labels);
        } else {
            return magic_enum::enum_names<Enum>();
        }
    }

    constexpr auto getLongLabel() const noexcept {
        if constexpr (requires { E::longLabels[value]; }) {
            return E::longLabels[value];
        } else {
            return magic_enum::enum_name<Enum>(value);
        }
    }

    constexpr static auto getLongLabels() noexcept -> std::array<std::string_view, size()> {
        if constexpr (requires { E::longLabels; }) {
            return std::to_array(E::longLabels);
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

    constexpr static auto getShortLabels() noexcept  -> std::array<std::string_view, size()> {
        if constexpr (requires { E::shortLabels; }) {
            return std::to_array(E::shortLabels);
        } else {
            return magic_enum::enum_names<Enum>();
        }
    }

    constexpr static Enum undefined() noexcept {
        return static_cast<Enum>(std::numeric_limits<UnderlyingType>::max());
    }

    template <typename Lambda>
    static constexpr auto forEach(Lambda&& lambda) {
        magic_enum::enum_for_each<Enum>(std::forward<Lambda>(lambda));
    }

private:
    Enum value;
};

template <class E>
inline std::ostream& operator<<(std::ostream& out, EnumChoice<E> choice) {
    out << choice.getLabel();
    return out;
}

template <class E>
juce::String& operator<<(juce::String& string1, EnumChoice<E> choice) {
    return string1 << choice.getLabel().data();
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
        const std::string_view label = choice.getEnumName();
        return juce::String {label.data()};
    }
};

} // namespace MoTool::Util