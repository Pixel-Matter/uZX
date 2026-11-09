#pragma once

#include <JuceHeader.h>

namespace MoTool::EditFileOps {

String getDefaultEditFileSuffix();

File getRecentEditsDirectory(tracktion::PropertyStorage& storage);

void setRecentEditsDirectory(tracktion::PropertyStorage& storage, const File& fileOrDir);

File suffestSaveAsFileName(tracktion::Edit& edit);

String getAppFileGlob();

/** Existing recent edit files, newest first. */
StringArray getRecentEdits(tracktion::PropertyStorage& storage);

void addToRecentEdits(tracktion::PropertyStorage& storage, const File& f);

void clearRecentEdits(tracktion::PropertyStorage& storage);

File findRecentEditInDir(const File& dir, const String& fileGlob);

File getTempEditFile();

File getStartupEditFile(tracktion::PropertyStorage& storage);

File getRendersDirectory(tracktion::Edit& edit);

bool saveEdit(tracktion::Edit& edit, bool warnOfFailure, bool forceSaveEvenIfNotModified, bool offerToDiscardChanges);

bool saveEditAs(tracktion::Edit& edit, const File& file);

bool saveEditAsWithDialog(tracktion::Edit& edit);

} // namespace MoTool::EditFileOps
