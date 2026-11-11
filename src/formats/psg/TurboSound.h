#pragma once

#include <JuceHeader.h>

namespace MoTool::uZX {

/** The Turbo Sound set `file` belongs to: itself and the siblings named like it with another
    chip number 1-3 after `_`, `-`, `.` or a space ("song_1.psg", "song_2.psg"), with as many
    frames, give or take one, and the same PSG header. Ordered by chip number; empty when no sibling matches. */
Array<File> findTurboSoundSet(const File& file);

/** Name for PSG files opened together: their common name prefix without trailing separators
    ("song_1", "song_2" -> "song"), or the first file's name when they share none. */
String getSetName(const Array<File>& files);

}  // namespace MoTool::uZX
