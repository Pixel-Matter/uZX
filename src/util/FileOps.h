#pragma once

#include <JuceHeader.h>

namespace MoTool::EditFileOps {

static inline constexpr auto EDIT_FILE_SUFFIX = ".motool";

String getAppFileGlob();

File findRecentEdit(const File& dir, const String& fileGlob);

File getRecentEditFile();

File getTempEditFile();

File getStartupEditFile();

} // namespace MoTool::EditFileOps
