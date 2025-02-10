#pragma once

#include <JuceHeader.h>

namespace MoTool::Util {

MidiMessageSequence readMidi(const std::string& data, int track);

}  // namespace MoTool::Util
