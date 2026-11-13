#include "FileOps.h"

#include "../controllers/App.h"

namespace MoTool::EditFileOps {

namespace te = tracktion;

namespace {
    static inline constexpr auto MOTOOL_EDIT_FILE_SUFFIX = ".motool";
    static inline constexpr auto UZX_EDIT_FILE_SUFFIX = ".uzx";
    static inline constexpr auto RECENT_EDITS_KEY = "recentEdits";
    static inline constexpr auto FORMAT_VERSION_ATTR = "uzxFormatVersion";
}

String getDefaultEditFileSuffix() {
    switch (MoToolApp::getApp().getTarget()) {
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
    return storage.getDefaultLoadSaveDirectory("edits");
}

static String getAppName() {
    return MoToolApp::getApp().getApplicationName();
}

static File getTempEditsLocation() {
    auto d = File::getSpecialLocation(File::tempDirectory).getChildFile(getAppName() + " Edits");
    d.createDirectory();
    return d;
}

void setRecentEditsDirectory(te::PropertyStorage& storage, const File& fileOrDir) {
    if (fileOrDir.isDirectory()) {
        if(fileOrDir != getTempEditsLocation())
            storage.setDefaultLoadSaveDirectory("edits", fileOrDir);
    } else {
        if (fileOrDir.getParentDirectory() != getTempEditsLocation())
            storage.setDefaultLoadSaveDirectory("edits", fileOrDir.getParentDirectory());
    }
}

bool isTempEdit(te::Edit& edit) {
    return te::EditFileOperations(edit).getEditFile().getParentDirectory() == getTempEditsLocation();
}

File suffestSaveAsFileName(te::Edit& edit) {
    auto efo = te::EditFileOperations(edit);
    auto currentFile = efo.getEditFile();

    File defaultLocation = !isTempEdit(edit) && currentFile.existsAsFile()
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
    auto f = findRecentEditInDir(getTempEditsLocation(), getAppFileGlob());

    if (f.existsAsFile()) {
        return f;
    } else {
        return {};
    }
}

File getTempEditFile() {
    return getTempEditsLocation().getNonexistentChildFile("Unnamed", getDefaultEditFileSuffix(), false);
}

File getStartupEditFile(te::PropertyStorage& storage) {
    auto f = getRecentEditFile(storage);
    if (!f.existsAsFile()) {
        f = getTempEditFile();
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

bool saveEdit(te::Edit& edit, bool warnOfFailure, bool forceSaveEvenIfNotModified, bool offerToDiscardChanges) {
    auto efo = te::EditFileOperations(edit);
    // ponytail: the callback runs before save() returns unless offerToDiscardChanges asks a
    // question; no caller passes true. Make saveEdit take a callback if one ever does.
    jassert(!offerToDiscardChanges);
    bool saved = false;
    efo.save(warnOfFailure, forceSaveEvenIfNotModified, offerToDiscardChanges, [&saved](bool ok) { saved = ok; });
    if (saved) {
        addToRecentEdits(edit.engine.getPropertyStorage(), efo.getEditFile());
        setRecentEditsDirectory(edit.engine.getPropertyStorage(), efo.getEditFile());
        return true;
    }
    return false;
}

bool saveEditAs(te::Edit& edit, const File& file) {
    auto efo = te::EditFileOperations(edit);
    // Overwriting is forced: the file chooser has already asked, and the engine's own prompt is async.
    bool saved = false;
    efo.saveAs(file, true, [&saved](bool ok) { saved = ok; });
    if (saved) {
        edit.editFileRetriever = [file] { return file; };

        addToRecentEdits(edit.engine.getPropertyStorage(), file);
        setRecentEditsDirectory(edit.engine.getPropertyStorage(), file);
        return true;
    }
    return false;
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
