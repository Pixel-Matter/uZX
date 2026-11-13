#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "../../../models/tuning/TuningSystemBase.h"
#include "../../../util/enumchoice.h"

namespace MoTool::uZX {

//==============================================================================
/**
    The eight shapes the chip's envelope can have, for everything but the chip's own register: the note
    wire, parameters, the interface. They come in pairs, a shape that starts rising and the same one starting
    to fall: the two oscillator shapes first, triangle and sawtooth, then the two that play once.

    The chip's register has sixteen values for these eight; `toRegister` gives the one a tracker shows.
*/
struct EnvelopeShapeEnum
{
    enum Enum : uint8_t
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

using EnvelopeShape = MoTool::Util::EnumChoice<EnvelopeShapeEnum>;

/** The value of the chip's shape register that plays a shape: of the two that do, the one from 8 up. */
constexpr int toRegister(EnvelopeShape shape) noexcept {
    constexpr std::array<int, 8> registers{14, 10, 12, 8, 13, 9, 15, 11};
    return registers[static_cast<size_t>(shape.asUnderlying())];
}

/** How the tuning turns a pitch into the period of an envelope of this shape, so that the shape sounds at
    that pitch: a triangle takes two periods to repeat where a sawtooth takes one. */
constexpr TuningSystem::PeriodMode envelopePeriodMode(EnvelopeShape shape) noexcept {
    return shape == EnvelopeShape::Triangle || shape == EnvelopeShape::TriangleDown ? TuningSystem::EnvelopeTriangle
                                                                                    : TuningSystem::Envelope;
}

}  // namespace MoTool::uZX
