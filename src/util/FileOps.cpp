#include "FileOps.h"

#include "../controllers/App.h"
#include "../controllers/PropertyStorage.h"

namespace MoTool::EditFileOps {

namespace te = tracktion;

namespace {
    static inline constexpr auto MOTOOL_EDIT_FILE_SUFFIX = ".motool";
    static inline constexpr auto UZX_EDIT_FILE_SUFFIX = ".uzx";
    static inline constexpr auto RECENT_EDITS_KEY = "recentEdits";
    static inline constexpr auto FORMAT_VERSION_ATTR = "uzxFormatVersion";
    static inline constexpr auto TUNE_FILES_TIME_ATTR = "tuneFilesTime";
}

String getDefaultEditFileSuffix() {
    switch (MoToolApp::getTarget()) {
        case MoToolApp::Target::uZXStudio:
        case MoToolApp::Target::uZXTuning:
        case MoToolApp::Target::uZXPlayer:
            return UZX_EDIT_FILE_SUFFIX;
        case MoToolApp::Target::MoTool:
            return MOTOOL_EDIT_FILE_SUFFIX;
    }
    jassertfalse;
    return {};
}

String getAppFileGlob() {
    return "*" + getDefaultEditFileSuffix();
}

File getRecentEditsDirectory(te::PropertyStorage& storage) {
    // The nearest folder that exists: the default one is made only by a first Save As, and JUCE's own file
    // dialog, which Linux uses without zenity or kdialog, puts a missing folder's name in its file name box
    auto dir = storage.getDefaultLoadSaveDirectory("edits");
    while (! dir.isDirectory() && dir.getParentDirectory() != dir)
        dir = dir.getParentDirectory();
    return dir;
}

static String getAppName() {
    return MoToolApp::getApp().getApplicationName();
}

File getWorkingEditsLocation(te::PropertyStorage& storage) {
    auto d = MoTool::PropertyStorage::getAppDataFolder(storage.getApplicationName()).getChildFile("Edits");
    d.createDirectory();
    return d;
}

static bool isInWorkingEdits(te::PropertyStorage& storage, const File& f) {
    const auto location = getWorkingEditsLocation(storage);
    return f == location || f.isAChildOf(location);
}

void setRecentEditsDirectory(te::PropertyStorage& storage, const File& fileOrDir) {
    const auto dir = fileOrDir.isDirectory() ? fileOrDir : fileOrDir.getParentDirectory();
    if (! isInWorkingEdits(storage, dir))
        storage.setDefaultLoadSaveDirectory("edits", dir);
}

bool isWorkingEdit(te::Edit& edit) {
    // Not through te::EditFileOperations: a temporary one re-reads the whole edit file, and the File menu asks this
    return isInWorkingEdits(edit.engine.getPropertyStorage(), edit.editFileRetriever());
}

static File getTuneEditDir(te::PropertyStorage& storage, const Array<File>& tuneFiles) {
    StringArray paths;
    for (const auto& f : tuneFiles)
        paths.add(f.getFullPathName());
    paths.sort(false);
    return getWorkingEditsLocation(storage).getChildFile(String::toHexString(paths.joinIntoString("\n").hashCode64()));
}

File getTuneEditFile(te::PropertyStorage& storage, const Array<File>& tuneFiles, const String& name) {
    return getTuneEditDir(storage, tuneFiles).getChildFile(File::createLegalFileName(name) + getDefaultEditFileSuffix());
}

static int64 getNewestTime(const Array<File>& files) {
    int64 newest = 0;
    for (const auto& f : files)
        newest = std::max(newest, f.getLastModificationTime().toMilliseconds());
    return newest;
}

void stampTuneFiles(te::Edit& edit, const Array<File>& tuneFiles) {
    // Not the edit file's own time: every open saves it
    edit.state.setProperty(TUNE_FILES_TIME_ATTR, getNewestTime(tuneFiles), nullptr);
}

bool isTuneEditCurrent(const File& tuneEdit, const Array<File>& tuneFiles) {
    if (! tuneEdit.existsAsFile())
        return false;
    auto root = XmlDocument(tuneEdit).getDocumentElement(true);
    return root != nullptr
           && root->getStringAttribute(TUNE_FILES_TIME_ATTR).getLargeIntValue() == getNewestTime(tuneFiles);
}

void pruneTuneEdits(te::PropertyStorage& storage, int keep) {
    // By the edit inside: saving it in place may leave its folder's own time as it was
    std::vector<std::pair<Time, File>> dirs;
    for (const auto& dir : getWorkingEditsLocation(storage).findChildFiles(File::findDirectories, false)) {
        Time newest;
        for (const auto& f : dir.findChildFiles(File::findFiles, false))
            newest = std::max(newest, f.getLastModificationTime());
        dirs.emplace_back(newest, dir);
    }
    if (std::ssize(dirs) <= keep)
        return;
    std::ranges::sort(dirs, std::greater<>{}, &std::pair<Time, File>::first);
    for (size_t i = static_cast<size_t>(keep); i < dirs.size(); ++i)
        dirs[i].second.deleteRecursively();
}

File suffestSaveAsFileName(te::Edit& edit) {
    auto currentFile = edit.editFileRetriever();

    File defaultLocation = !isWorkingEdit(edit) && currentFile.existsAsFile()
                               ? currentFile.getParentDirectory()
                               : File::getSpecialLocation(File::userDocumentsDirectory)
                                     .getChildFile(getAppName());

    auto newEditName = defaultLocation.getChildFile(currentFile.getFileName());
    newEditName = te::getNonExistentSiblingWithIncrementedNumberSuffix(newEditName, false);
    newEditName.getParentDirectory().createDirectory();
    return newEditName;
}

StringArray getRecentEdits(te::PropertyStorage& storage) {
    StringArray files;
    files.addTokens(storage.getPropertiesFile().getValue(RECENT_EDITS_KEY), ";", {});
    files.trim();
    files.removeEmptyStrings();

    for (int i = files.size(); --i >= 0;) {
        const File f(files.getReference(i));

        if (!f.existsAsFile()) {
            files.remove(i);
        }
    }

    files.removeRange(8, files.size());  // newest first: drop the oldest
    return files;
}

void addToRecentEdits(te::PropertyStorage& storage, const File& f) {
    auto files = getRecentEdits(storage);

    files.removeString(f.getFullPathName());
    files.insert(0, f.getFullPathName());

    storage.getPropertiesFile().setValue(RECENT_EDITS_KEY, files.joinIntoString(";"));
}

void clearRecentEdits(te::PropertyStorage& storage) {
    storage.getPropertiesFile().removeValue(RECENT_EDITS_KEY);
}

File findRecentEditInDir(const File& dir, const String& fileGlob) {
    // sort by name, intendet to work in temp directory where files are named with incrementing numbers
    auto files = dir.findChildFiles(File::findFiles, false, fileGlob);
    if (files.size() > 0) {
        files.sort();
        return files.getLast();
    }
    return {};
}

static File getRecentEditFile(te::PropertyStorage& storage) {
    auto recent = getRecentEdits(storage);
    // fins first existing file in recent edits
    for (auto& f : recent) {
        File file(f);
        if (file.existsAsFile()) {
            return file;
        }
    }
    // otherwise look in recent edits directory
    auto f = findRecentEditInDir(getWorkingEditsLocation(storage), getAppFileGlob());

    if (f.existsAsFile()) {
        return f;
    } else {
        return {};
    }
}

File getUnnamedEditFile(te::PropertyStorage& storage) {
    return getWorkingEditsLocation(storage).getNonexistentChildFile("Unnamed", getDefaultEditFileSuffix(), false);
}

File getStartupEditFile(te::PropertyStorage& storage) {
    auto f = getRecentEditFile(storage);
    if (!f.existsAsFile()) {
        f = getUnnamedEditFile(storage);
    }
    return f;
}

File getRendersDirectory(te::Edit& edit) {
    auto editFile = edit.editFileRetriever();
    File rendersDir = editFile.existsAsFile()
        ? editFile.getParentDirectory().getChildFile(editFile.getFileNameWithoutExtension()).withFileExtension("Renders")
        : File::getSpecialLocation(File::userMusicDirectory)
            .getChildFile(getAppName()).getChildFile("Renders");

    return rendersDir;
}

int readEditFormatVersion(const File& editFile) {
    // The root element alone: the version sits on EDIT, no need to parse the whole edit
    auto root = XmlDocument(editFile).getDocumentElement(true);
    return root != nullptr ? root->getIntAttribute(FORMAT_VERSION_ATTR) : 0;
}

void stampEditFormatVersion(te::Edit& edit) {
    edit.state.setProperty(FORMAT_VERSION_ATTR, editFormatVersion, nullptr);
}

static void makeMostRecent(te::Edit& edit, const File& file) {
    addToRecentEdits(edit.engine.getPropertyStorage(), file);
    setRecentEditsDirectory(edit.engine.getPropertyStorage(), file);
}

bool saveEdit(te::Edit& edit, bool warnOfFailure, bool forceSaveEvenIfNotModified, bool offerToDiscardChanges) {
    auto efo = te::EditFileOperations(edit);
    // ponytail: the callback runs before save() returns unless offerToDiscardChanges asks a
    // question; no caller passes true. Make saveEdit take a callback if one ever does.
    jassert(!offerToDiscardChanges);
    bool saved = false;
    efo.save(warnOfFailure, forceSaveEvenIfNotModified, offerToDiscardChanges, [&saved](bool ok) { saved = ok; });
    if (saved)
        makeMostRecent(edit, efo.getEditFile());
    return saved;
}

void autoSaveEdit(te::Edit& edit) {
    if (edit.editFileRetriever() == File())
        return;
    // Not save() with force off: it writes a temporary copy next to the file even when nothing changed
    if (isWorkingEdit(edit) || edit.hasChangedSinceSaved())
        saveEdit(edit, true, true, false);
    else
        makeMostRecent(edit, edit.editFileRetriever());
}

bool saveEditAs(te::Edit& edit, const File& file) {
    auto efo = te::EditFileOperations(edit);
    // Overwriting is forced: the file chooser has already asked, and the engine's own prompt is async.
    bool saved = false;
    efo.saveAs(file, true, [&saved](bool ok) { saved = ok; });
    if (saved) {
        edit.editFileRetriever = [file] { return file; };
        makeMostRecent(edit, file);
    }
    return saved;
}

bool saveEditAsWithDialog(te::Edit& edit) {
    auto newEditName = suffestSaveAsFileName(edit);

    juce::FileChooser fc("Save As...", newEditName, getAppFileGlob());
    if (fc.browseForFileToSave(false)) {
        auto savedFile = fc.getResult().withFileExtension(getDefaultEditFileSuffix());
        return saveEditAs(edit, savedFile);
    }
    return false;
}

} // namespace MoTool::EditFileOps
