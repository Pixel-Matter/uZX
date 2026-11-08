#pragma once

#include <JuceHeader.h>

namespace MoTool::EditFileOps {

String getDefaultEditFileSuffix();

File getRecentEditsDirectory();

void setRecentEditsDirectory(const File& fileOrDir);

/** True for an edit that has never been saved by the user: it lives in the app's temp folder. */
bool isTempEdit(tracktion::Edit& edit);

File suffestSaveAsFileName(tracktion::Edit& edit);

String getAppFileGlob();

StringArray getRecentEdits();

void addToRecentEdits(const File& f);

File findRecentEditInDir(const File& dir, const String& fileGlob);

File getRecentEditFile();

File getTempEditFile();

File getStartupEditFile();

File getRendersDirectory(tracktion::Edit& edit);

bool saveEdit(tracktion::Edit& edit, bool warnOfFailure, bool forceSaveEvenIfNotModified, bool offerToDiscardChanges);

bool saveEditAs(tracktion::Edit& edit, const File& file);

bool saveEditAsWithDialog(tracktion::Edit& edit);

} // namespace MoTool::EditFileOps
