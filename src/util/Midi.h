#pragma once

#include <JuceHeader.h>

namespace MoTool::Helpers {

MidiMessageSequence readMidi(const std::string& data, int track);

}  // namespace MoTool::Util
