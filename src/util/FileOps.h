#pragma once

#include <JuceHeader.h>

namespace MoTool::EditFileOps {

String getDefaultEditFileSuffix();

File getRecentEditsDirectory(tracktion::PropertyStorage& storage);

void setRecentEditsDirectory(tracktion::PropertyStorage& storage, const File& fileOrDir);

/** True for an edit that has never been saved by the user: it lives in the app's temp folder. */
bool isTempEdit(tracktion::Edit& edit);

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

/** The edit format this build writes, separate from app versions. Bump it when an older app
    would misread a newer edit: an app refuses to open a file with a higher version. */
inline constexpr int editFormatVersion = 1;

/** The format version an edit file was saved with: 0 for files from before versioning or non-XML ones. */
int readEditFormatVersion(const File& editFile);

/** Marks the edit as being in this build's format; every save from then on writes it. */
void stampEditFormatVersion(tracktion::Edit& edit);

bool saveEdit(tracktion::Edit& edit, bool warnOfFailure, bool forceSaveEvenIfNotModified, bool offerToDiscardChanges);

bool saveEditAs(tracktion::Edit& edit, const File& file);

bool saveEditAsWithDialog(tracktion::Edit& edit);

} // namespace MoTool::EditFileOps
