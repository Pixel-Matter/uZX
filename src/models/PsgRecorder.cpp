#include "PsgRecorder.h"

#include <cmath>

namespace MoTool {

void PsgRecorder::add(double seconds, int channel, int controller, int value) {
    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wfloat-equal")
    const bool sameGroup = seconds == groupTime_;
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE
    if (!sameGroup) {
        closeGroup();
        // A hair early counts as on the frame: times come from sample positions and tempo maths
        moveToFrame(static_cast<size_t>(std::max(0.0, std::floor(seconds * frameRate_ + 1.0e-6))));
        groupTime_ = seconds;
    }
    decoder_.decode(channel, controller, value, [this] { return held_; });
}

uZX::PsgData PsgRecorder::finish(double seconds) {
    closeGroup();
    const auto numFrames = static_cast<size_t>(std::max(0.0, std::round(seconds * frameRate_)));
    moveToFrame(std::max(numFrames, current_.isEmpty() ? frames_.size() : frames_.size() + 1));

    uZX::PsgData data;
    data.options.frameRate = frameRate_;
    data.frames = std::move(frames_);
    *this = PsgRecorder(frameRate_);
    return data;
}

void PsgRecorder::closeGroup() {
    if (decoder_.isEmpty())
        return;
    const auto written = decoder_.takeRegisters();
    for (size_t reg = 0; reg < written.size(); ++reg) {
        if (!written.mask[reg])
            continue;
        if (reg == uZX::PsgRegType::EnvelopeShape || held_.registers[reg] != written.registers[reg]) {
            current_.registers[reg] = written.registers[reg];
            current_.mask[reg] = true;
        }
        held_.registers[reg] = written.registers[reg];
    }
}

void PsgRecorder::moveToFrame(size_t index) {
    while (frames_.size() < index) {
        frames_.push_back(current_);
        current_ = {};
    }
}

}  // namespace MoTool
