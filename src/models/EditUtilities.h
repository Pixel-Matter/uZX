#pragma once

#include <JuceHeader.h>

#include "Timecode.h"

namespace te = tracktion;

namespace MoTool::Helpers {

/** The frame rates the project fps and the frame-bearing timecode formats may take. */
inline constexpr double kAllowedProjectFps[] = { 24, 25, 30, 48, 50, 60, 100, 200 };

TimecodeDisplayFormatExt getEditTimecodeFormat(te::Edit& edit);

void setEditTimecodeFormat(te::Edit& edit, TimecodeDisplayFormatExt format);

/** The project's global frame rate: the cadence PSG frames fire at, and the basis
    for frames-per-beat / BPM snapping. Independent from the timecode display format,
    though the two are kept equal by setProjectFps. Falls back to the timecode
    format's fps for edits saved before this property existed. */
double getProjectFps(te::Edit& edit);

/** Sets the project fps and rewrites the timecode display format to the same fps,
    preserving its current display mode so the grid/ruler follow. */
void setProjectFps(te::Edit& edit, double fps);

juce::PopupMenu buildTimecodeFormatMenu(te::Edit& edit);

} // namespace MoTool::Helpers