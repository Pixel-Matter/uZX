
#include "EditUtilities.h"

#include <common/Utilities.h>

namespace te = tracktion;

namespace MoTool::Helpers {

TimecodeDisplayFormatExt getEditTimecodeFormat(te::Edit& edit) {
    auto value = edit.state.getPropertyAsValue(te::IDs::timecodeFormat, nullptr);
    return VariantConverter<TimecodeDisplayFormatExt>::fromVar(value);
}

void setEditTimecodeFormat(te::Edit& edit, TimecodeDisplayFormatExt format) {
    edit.state.setProperty(te::IDs::timecodeFormat, VariantConverter<TimecodeDisplayFormatExt>::toVar(format), &edit.getUndoManager());
}

} // namespace MoTool::Helpers