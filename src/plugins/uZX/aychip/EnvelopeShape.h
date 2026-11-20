#pragma once

#include "../../../models/tuning/TuningSystemBase.h"
#include "../../../util/enumchoice.h"
#include "../../../chip/EnvelopeShape.h"

namespace MoTool::uZX {

//==============================================================================
/**
    The eight shapes the chip's envelope can have, for everything but the chip's own register: the note
    wire, parameters, the interface. They come in pairs, a shape that starts rising and the same one starting
    to fall: the two oscillator shapes first, triangle and sawtooth, then the two that play once.

    The chip's register has sixteen values for these eight; `toRegister` gives the one a tracker shows.
*/
using EnvelopeShape = MoTool::Util::EnumChoice<EnvelopeShapeEnum>;

/** How the tuning turns a pitch into the period of an envelope of this shape, so that the shape sounds at
    that pitch: a triangle takes two periods to repeat where a sawtooth takes one. */
constexpr TuningSystem::PeriodMode envelopePeriodMode(EnvelopeShape shape) noexcept {
    return shape == EnvelopeShape::Triangle || shape == EnvelopeShape::TriangleDown ? TuningSystem::EnvelopeTriangle
                                                                                    : TuningSystem::Envelope;
}

}  // namespace MoTool::uZX
