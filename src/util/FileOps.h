#pragma once

#include <JuceHeader.h>

namespace MoTool::EditFileOps {

String getDefaultEditFileSuffix();

/** Where the Open dialog starts: the folder last opened from or saved to, or the nearest of its parents
    that exists. */
File getRecentEditsDirectory(tracktion::PropertyStorage& storage);

void setRecentEditsDirectory(tracktion::PropertyStorage& storage, const File& fileOrDir);

/** Where edits live that the user hasn't saved anywhere: Studio's Unnamed edits, and the Player's copy
    of each tune it opens. It sits with the app's settings, so they last across restarts. */
File getWorkingEditsLocation(tracktion::PropertyStorage& storage);

/** True for an edit that the user has never saved: it lives in the working edits folder. */
bool isWorkingEdit(tracktion::Edit& edit);

/** The Player's working copy of a tune: one per set of tune files, named after the tune. */
File getTuneEditFile(tracktion::PropertyStorage& storage, const Array<File>& tuneFiles, const String& name);

/** Records in the edit which versions of the tune files it was imported from. */
void stampTuneFiles(tracktion::Edit& edit, const Array<File>& tuneFiles);

/** True when the working copy exists and was imported from the tune files as they are now. */
bool isTuneEditCurrent(const File& tuneEdit, const Array<File>& tuneFiles);

/** Deletes all but the `keep` most recently changed working copies of tunes. */
void pruneTuneEdits(tracktion::PropertyStorage& storage, int keep);

File suffestSaveAsFileName(tracktion::Edit& edit);

String getAppFileGlob();

/** Existing recent edit files, newest first. */
StringArray getRecentEdits(tracktion::PropertyStorage& storage);

void addToRecentEdits(tracktion::PropertyStorage& storage, const File& f);

void clearRecentEdits(tracktion::PropertyStorage& storage);

File findRecentEditInDir(const File& dir, const String& fileGlob);

/** A new Unnamed edit file in the working edits folder. */
File getUnnamedEditFile(tracktion::PropertyStorage& storage);

File getStartupEditFile(tracktion::PropertyStorage& storage);

File getRendersDirectory(tracktion::Edit& edit);

/** The edit format this build writes, separate from app versions. Bump it when an older app
    would misread a newer edit: an app refuses to open a file with a higher version.

    2: a PSG clip's frame restarts the envelope where it sets the retrigger, not wherever it sets the shape
       (PsgList converts older clips); the chip controllers in MIDI clips have MIDI's value ranges: a switch
       is on from 64, controller 70 is one of eight envelope shapes, controller 2 sweeps the noise periods.
    3: a PSG clip may start with empty frames before its first frame (psgLeadIn); an older app would play
       such a clip that much early. */
inline constexpr int editFormatVersion = 3;

/** The format version an edit file was saved with: 0 for files from before versioning or non-XML ones. */
int readEditFormatVersion(const File& editFile);

/** Marks the edit as being in this build's format; every save from then on writes it. */
void stampEditFormatVersion(tracktion::Edit& edit);

bool saveEdit(tracktion::Edit& edit, bool warnOfFailure, bool forceSaveEvenIfNotModified, bool offerToDiscardChanges);

/** The save the app makes on its own: when an edit opens, when another replaces it, and on quit. A working
    edit is written every time; a user's file only when it has changed, so a file that is only played stays
    as it is on disk. Either way the edit becomes the most recent one. An edit without a file, such as the
    Tuning window's, is left alone. */
void autoSaveEdit(tracktion::Edit& edit);

bool saveEditAs(tracktion::Edit& edit, const File& file);

bool saveEditAsWithDialog(tracktion::Edit& edit);

} // namespace MoTool::EditFileOps
