#pragma once

#include <JuceHeader.h>

#include "Timecode.h"

namespace te = tracktion;

namespace MoTool::Helpers {

/** The frame rates the project fps may take. */
inline constexpr double kAllowedProjectFps[] = { 24, 25, 30, 48, 50, 60, 100, 200 };

/** The timecode display mode of the timeline ruler/grid. Falls back to parsing
    the legacy combined te::IDs::timecodeFormat property for edits saved before
    IDs::timecodeDisplayMode existed. */
TimecodeDisplayMode getTimecodeDisplayMode(te::Edit& edit);

void setTimecodeDisplayMode(te::Edit& edit, TimecodeDisplayMode mode);

/** Initializes missing timecode display properties from the legacy combined
    te::IDs::timecodeFormat property without losing its encoded fps. */
void migrateTimecodeDisplaySettings(te::Edit& edit);

/** The project's global frame rate: the cadence PSG frames fire at, and the basis
    for frames-per-beat / BPM snapping. Falls back to the fps encoded in the legacy
    timecode format for edits saved before this property existed. */
double getProjectFps(te::Edit& edit);

void setProjectFps(te::Edit& edit, double fps);

/** Default frame subdivision pattern for a frames-per-beat value:
    quarters when divisible by 4, halves when divisible by 2, otherwise empty. */
std::vector<int> getDefaultGridSubdivisionPattern(int framesPerBeat);

/** Parses positive integers from any non-digit-delimited text and adjusts the
    tail of the pattern so the sum matches framesPerBeat. */
std::vector<int> parseGridSubdivisionPattern(const juce::String& text, int framesPerBeat);

juce::String formatGridSubdivisionPattern(const std::vector<int>& pattern);

/** Frame counts per subdivision group within one beat (e.g. {6, 7, 6, 7} for a
    swing grid at 26 frames per beat), or the default pattern when unset. */
std::vector<int> getGridSubdivisionPattern(te::Edit& edit, int framesPerBeat);

/** Stores the subdivision pattern as a space-separated string; fewer than two
    groups clears it. */
void setGridSubdivisionPattern(te::Edit& edit, const std::vector<int>& pattern);

juce::PopupMenu buildTimecodeFormatMenu(te::Edit& edit);

} // namespace MoTool::Helpers
