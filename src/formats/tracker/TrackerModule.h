#pragma once

#include <JuceHeader.h>

#include "../../models/ClipTimers.h"
#include "../../models/tuning/TuningRegistry.h"
#include "../../plugins/uZX/aychip/aychip.h"
#include "../psg/PsgData.h"

#include <optional>
#include <vector>

namespace MoTool::uZX {

//==============================================================================
/** A ZX Spectrum tracker module (PT3, PT2, STC, ASC, SQT, Vortex Tracker text, a Turbo Sound
    pair, ...) as zxtune's players play it, up to its loop.
*/
struct TrackerModule {
    std::vector<PsgData> chips;  // the AY registers of every frame, one per chip (two or three for
                                 // Turbo Sound), all the same length, at the module's frame rate
    juce::String title;          // empty when the module stores none
    /** The tracker's note table, when it is one of uZX's presets; else left to detection. */
    std::optional<BuiltinTuningType> table;
    std::vector<size_t> rowStarts;  // the frame each pattern row starts at, in play order
    std::optional<double> chipClock;  // Hz, when the module stores it (Vortex Tracker's text modules do)
    size_t loopFrame = 0;             // the frame the tune loops back to; 0 when it loops to its start
    std::optional<ChipType> chipType; // AY or YM, when the tune says which (a TAYM does)

    /** A chip's timers: timed register writes between its frames, which a TAYM has. */
    struct Timers {
        juce::MemoryBlock bank;                 // the lanes and actions the commands index, as ClipTimers keeps them
        int chipIndex = 0;                      // the chip in that bank
        std::vector<ClipTimers::Event> events;  // the commands in frame order; empty for a chip without timers
        bool unplayable = false;                // true if some of the chip's timers cannot be played here
    };
    std::vector<Timers> timers;  // one for each of `chips` when the tune has timers; else empty
};

/** Empty when the bytes are not a module of a supported format. */
std::optional<TrackerModule> readTrackerModule(const juce::MemoryBlock& bytes);

/** The file extensions of the formats readTrackerModule reads, lower case, without the dot. */
const juce::StringArray& getTrackerModuleExtensions();

}  // namespace MoTool::uZX
