#pragma once

#include <JuceHeader.h>

#include "Timecode.h"

namespace te = tracktion;

namespace MoTool::Helpers {

TimecodeDisplayFormatExt getEditTimecodeFormat(te::Edit& edit);

void setEditTimecodeFormat(te::Edit& edit, TimecodeDisplayFormatExt format);

} // namespace MoTool::Helpers