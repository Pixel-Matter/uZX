#pragma once

#include <JuceHeader.h>
#include "../../chip/PsgRegisterFrame.h"
#include "../../util/enumchoice.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>

namespace MoTool::uZX {

using PsgRegType = MoTool::Util::EnumChoice<RegTypeEnum>;


struct PsgRegsFrame : public PsgRegisterFrame {

    using PsgRegisterFrame::PsgRegisterFrame;

    constexpr PsgRegsFrame(std::initializer_list<std::pair<PsgRegType, uint8_t>> regs) noexcept {
        for (const auto& [reg, value] : regs) {
            registers[static_cast<size_t>(reg)] = value;
            mask[static_cast<size_t>(reg)] = true;
        }
    }

    void debugPrint() const noexcept {
        for (size_t i = 0; i < size(); ++i) {
            DBG(std::string(PsgRegType::getLabel(i)) << ": " << static_cast<int>(registers[i]) << " " << (mask[i] ? "true" : "false"));
        }
    }

    void debugPrintSet() const noexcept {
        for (size_t i = 0; i < size(); ++i) {
            if (!mask[i]) continue;
            DBG(std::string(PsgRegType::getLabel(i)) << ": " << static_cast<int>(registers[i]));
        }
    }
};

struct PsgData {
    struct Options {
        double frameRate = 50;
        size_t playRate = 1;
        // bool usesRetriggers = false;
        // TODO frequency table or id or shared_ptr to it
    };

    std::vector<PsgRegsFrame> frames;
    Options options;

    void clear() {
        frames.clear();
    }

    size_t getLengthMachineFrames() const noexcept {
        return frames.size() * options.playRate;
    }

    double getLengthSeconds() const noexcept {
        return static_cast<double>(getLengthMachineFrames()) / options.frameRate;
    }

    inline bool isEmpty() const noexcept {
        return frames.empty();
    }

    inline double getFrameRate() const {
        return options.frameRate;
    }

    inline double frameNumToSeconds(size_t frameNum) const {
        return static_cast<double>(frameNum) / getFrameRate();
    }
};


} // namespace MoTool::uZX