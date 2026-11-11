#pragma once

#include <JuceHeader.h>

#include <vector>

namespace MoTool::TuningCsv {

/** One note of a tuning table. */
struct Row
{
    int midiNote;
    String noteName;  // with its octave, "C♯4"
    int period;
};

/** A `MIDI,Note,Period` header and a line per row, the note names spelled in ASCII and padded to three
    characters as trackers show them: "C♯4" -> "C#4", "C4" -> "C-4". */
String format(const std::vector<Row>& rows);

/** A `.csv` file name for a table described as `description`, without characters file systems reject. */
String fileName(const String& description);

}  // namespace MoTool::TuningCsv
