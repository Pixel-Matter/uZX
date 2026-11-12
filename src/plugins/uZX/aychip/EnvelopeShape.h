#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "../../../util/enumchoice.h"

namespace MoTool::uZX {

//==============================================================================
/**
    The eight shapes the chip's envelope can have, for everything but the chip's own register: the note
    wire, parameters, the interface. The oscillator shapes come first, the usual two and then each of them
    turned over; the shapes that play once come after.

    The chip's register has sixteen values for these eight; `toRegister` gives the one a tracker shows.
*/
struct EnvelopeShapeEnum
{
    enum Enum : uint8_t
    {
        Triangle,         ///< /\/\  register 14
        Saw,              ///< ////  register 12
        TriangleDown,     ///< \/\/  register 10
        SawDown,          ///< \\\\  register 8
        Decay,            ///< \___  register 9, and 0 to 3
        Attack,           ///< /~~~  register 13
        DecayToFull,      ///< \~~~  register 11
        AttackToSilence,  ///< /___  register 15, and 4 to 7
    };

    static inline constexpr std::string_view longLabels[]{
        "Triangle",
        "Sawtooth",
        "Triangle, falling first",
        "Sawtooth, falling",
        "Decay",
        "Attack",
        "Decay, then full",
        "Attack, then silent",
    };

    static inline constexpr std::string_view shortLabels[]{
        "/\\/\\",
        "/|/|",
        "\\/\\/",
        "\\|\\|",
        "\\___",
        "/~~~",
        "\\|~~",
        "/|__",
    };
};

using EnvelopeShape = MoTool::Util::EnumChoice<EnvelopeShapeEnum>;

/** The value of the chip's shape register that plays a shape: of the two that do, the one from 8 up. */
constexpr int toRegister(EnvelopeShape shape) noexcept {
    constexpr std::array<int, 8> registers{14, 12, 10, 8, 9, 13, 11, 15};
    return registers[static_cast<size_t>(shape.asUnderlying())];
}

}  // namespace MoTool::uZX
