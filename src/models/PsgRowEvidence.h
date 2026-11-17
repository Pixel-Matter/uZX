#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

#include "../formats/psg/PsgData.h"

namespace MoTool {

//==============================================================================
/** Evidence that a tracker row starts on a frame of an AY register dump, learnt from
    tracker modules, which know the frame every row starts at.

    Every frame gives each channel one event, a cell: how its volume moved, how its
    pitch moved and for how long it had held, whether its tone or noise switched, and
    whether its envelope bit did. The chip gets one too: an envelope restart (R13
    written), a move of the envelope period, a move of the noise period. A table gives
    every cell a weight, the log of how much likelier it is on a row start than on any
    frame; scripts/train_row_evidence.py counts it from a corpus of modules. A frame's
    evidence is the sum of its four weights.
*/
namespace PsgRowEvidence {

constexpr size_t numChannelCells = 8 * 9 * 9 * 3;  // volume x pitch x mixer x envelope bit
constexpr size_t numChipCells = 4 * 3 * 2;         // envelope restart x envelope period x noise period

/** The events of one frame: a cell per channel, and the chip's. */
struct FrameCells
{
    std::array<uint16_t, 3> channels{};
    uint16_t chip = 0;
};

/** The cells of every frame. The first frame has nothing before it, so its cells are 0. */
std::vector<FrameCells> cellsOf(const uZX::PsgData&);

/** Per frame, how much the registers speak for a row starting there; never negative. */
std::vector<double> of(const uZX::PsgData&);

}  // namespace PsgRowEvidence

}  // namespace MoTool
