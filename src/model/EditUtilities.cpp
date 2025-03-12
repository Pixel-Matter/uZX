
#include "EditUtilities.h"

#include <common/Utilities.h>

namespace te = tracktion;

namespace MoTool::Helpers {

TimecodeDisplayFormatExt getEditTimecodeFormat(te::Edit& edit) {
    // TODO use UndoManager
    auto value = edit.state.getPropertyAsValue(te::IDs::timecodeFormat, nullptr);
    return VariantConverter<TimecodeDisplayFormatExt>::fromVar(value);
}

void setEditTimecodeFormat(te::Edit& edit, TimecodeDisplayFormatExt format) {
    // TODO use UndoManager
    edit.state.setProperty(te::IDs::timecodeFormat, VariantConverter<TimecodeDisplayFormatExt>::toVar(format), nullptr);
}

} // namespace MoTool::Helpers