#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace MoTool::uZX {

/** The eight envelope shapes used by the chip-note stream. */
struct EnvelopeShapeEnum
{
    enum Enum : std::uint8_t
    {
        Triangle,         ///< /\/\  register 14
        TriangleDown,     ///< \/\/  register 10
        Saw,              ///< ////  register 12
        SawDown,          ///< \\\\  register 8
        Attack,           ///< /~~~  register 13
        Decay,            ///< \___  register 9, and 0 to 3
        AttackToSilence,  ///< /___  register 15, and 4 to 7
        DecayToFull,      ///< \~~~  register 11
    };

    static inline constexpr std::string_view longLabels[]{
        "Triangle",
        "Triangle, falling first",
        "Sawtooth",
        "Sawtooth, falling",
        "Attack",
        "Decay",
        "Attack, then silent",
        "Decay, then full",
    };

    static inline constexpr std::string_view shortLabels[]{
        "/\\/\\",
        "\\/\\/",
        "/|/|",
        "\\|\\|",
        "/~~~",
        "\\___",
        "/|__",
        "\\|~~",
    };
};

constexpr bool isTriangle(EnvelopeShapeEnum::Enum shape) noexcept {
    return shape == EnvelopeShapeEnum::Triangle || shape == EnvelopeShapeEnum::TriangleDown;
}

/** The register value for the sounding version of a shape. */
constexpr int toRegister(EnvelopeShapeEnum::Enum shape) noexcept {
    constexpr std::array<int, 8> registers{14, 10, 12, 8, 13, 9, 15, 11};
    return registers[static_cast<std::size_t>(shape)];
}

}  // namespace MoTool::uZX
