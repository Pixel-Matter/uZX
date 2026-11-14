#pragma once

#include <JuceHeader.h>

#include "../tracker/TrackerModule.h"

#include <optional>

namespace MoTool::uZX {

/** Reads a TAYM, the interchange format for chip music with timer tricks (github.com/ruguevara/taym): each
    AY chip's frames, as its embedded PSG has them, and its timers.

    A chip's frame data is a standard PSG and is read as any PSG file is: every write kept, a repeated one
    too. Chips of another type are left out.

    Empty when the bytes are not a TAYM or it has no AY chip. */
std::optional<TrackerModule> readTaymModule(const juce::MemoryBlock& bytes);

/** True if the bytes begin as a TAYM does. */
bool looksLikeTaym(const juce::MemoryBlock& bytes);

/** The file extensions of TAYM files, lower case, without the dot. */
const juce::StringArray& getTaymExtensions();

}  // namespace MoTool::uZX
