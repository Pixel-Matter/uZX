#pragma once

#include "PsgRegisterFrame.h"
#include "PsgRenderer.h"

namespace MoTool::uZX {

/** Turns rendered frame parameters into the register writes the chip needs. */
class PsgRegisterWriter
{
public:
    PsgRegisterWriter() noexcept { reset(); }

    /** Writes the changes in `patch`; an envelope retrigger always writes its shape register. */
    PsgRegisterFrame write(const PsgPatch& patch) noexcept;

    /** Clears the remembered register values, as after a chip reset. */
    void reset() noexcept {
        held_ = {};
        held_.registers[RegTypeEnum::Mixer] = 0x3f;  // AyumiEmulator resets with tone and noise disabled.
        last_ = {};
        initialized_ = false;
    }

    /** The latest value remembered for a register, whether or not it was written this frame. */
    constexpr std::uint8_t held(std::size_t reg) const noexcept { return held_.registers[reg]; }

private:
    PsgRegisterFrame held_{};
    PsgPatch last_{};
    bool initialized_ = false;
};

}  // namespace MoTool::uZX
