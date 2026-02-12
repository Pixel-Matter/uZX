#pragma once

#include <JuceHeader.h>

#include "Timecode.h"

namespace te = tracktion;

namespace MoTool::Helpers {

TimecodeDisplayFormatExt getEditTimecodeFormat(te::Edit& edit);

void setEditTimecodeFormat(te::Edit& edit, TimecodeDisplayFormatExt format);

juce::PopupMenu buildTimecodeFormatMenu(te::Edit& edit);

} // namespace MoTool::Helpers