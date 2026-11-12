#pragma once

#include <JuceHeader.h>

#include <vector>

#include "../formats/psg/PsgData.h"
#include "PsgCC.h"

namespace MoTool {

//==============================================================================
/**
    Records the PSG wire as a PSG file holds it: for each frame, the registers written in it. It reads
    the wire as a chip does, so what it records is what a chip on that wire is told.

    A register written with the value it holds is left out, except the envelope shape, whose write
    restarts the envelope.
*/
class PsgRecorder
{
public:
    explicit PsgRecorder(double frameRate) : frameRate_(frameRate) {}

    /** Takes a controller of the wire at `seconds` from the start, on a channel counted from the chip's
        base channel. The messages of one time are one frame of the wire. Times do not go back. */
    void add(double seconds, int channel, int controller, int value);

    /** What was recorded as frames up to `seconds`, and further if registers were written after that:
        the end of a note may come with the last message. */
    uZX::PsgData finish(double seconds);

private:
    void closeGroup();
    void moveToFrame(size_t index);

    double frameRate_;
    PsgDecoder decoder_;
    uZX::PsgRegsFrame held_ = allRegistersSet();  // what a chip on the wire holds
    std::vector<uZX::PsgRegsFrame> frames_;       // the frames that are over
    uZX::PsgRegsFrame current_;                   // the frame being recorded: the registers written so far
    double groupTime_ = -1.0;                     // the time of the messages the decoder holds

    static uZX::PsgRegsFrame allRegistersSet() noexcept {
        uZX::PsgRegsFrame regs;
        regs.mask.fill(true);
        return regs;
    }
};

}  // namespace MoTool
