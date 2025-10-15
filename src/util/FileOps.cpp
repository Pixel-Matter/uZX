#include "FileOps.h"

#include "../controllers/App.h"

namespace MoTool::EditFileOps {

namespace te = tracktion;

namespace {
    static inline constexpr auto MOTOOL_EDIT_FILE_SUFFIX = ".motool";
    static inline constexpr auto UZX_EDIT_FILE_SUFFIX = ".uzx";
}

String getDefaultEditFileSuffix() {
    switch (MoToolApp::getApp().getTarget()) {
        case MoToolApp::Target::uZXStudio:
        case MoToolApp::Target::uZXTuning:
            return UZX_EDIT_FILE_SUFFIX;
        case MoToolApp::Target::MoTool:
            return MOTOOL_EDIT_FILE_SUFFIX;
    }
}

String getAppFileGlob() {
    return "*" + getDefaultEditFileSuffix();
}

static inline MoTool::PropertyStorage& getMoToolPropertyStorage() {
    auto e = te::Engine::getEngines()[0];
    jassert (e != nullptr);
    auto casted = dynamic_cast<MoTool::PropertyStorage*>(&e->getPropertyStorage());
    jassert(casted != nullptr);
    return *casted;
}

File getRecentEditsDirectory() {
    auto& storage = getMoToolPropertyStorage();
    return storage.getDefaultLoadSaveDirectory("edits");
}

static File getTempEditsLocation() {
    auto d = File::getSpecialLocation(File::tempDirectory).getChildFile(MoToolApp::getApp().getApplicationName() + " Edits");
    d.createDirectory();
    return d;
}

void setRecentEditsDirectory(const File& fileOrDir) {
    if (fileOrDir.isDirectory()) {
        if(fileOrDir != getTempEditsLocation())
            getMoToolPropertyStorage().setDefaultLoadSaveDirectory("edits", fileOrDir);
    } else {
        if (fileOrDir.getParentDirectory() != getTempEditsLocation())
            getMoToolPropertyStorage().setDefaultLoadSaveDirectory("edits", fileOrDir.getParentDirectory());
    }
}

File suffestSaveAsFileName(te::Edit& edit) {
    auto efo = te::EditFileOperations(edit);
    auto currentFile = efo.getEditFile();

    bool isTemp = currentFile.getParentDirectory() == getTempEditsLocation();
    File defaultLocation = !isTemp && currentFile.existsAsFile()
                               ? currentFile.getParentDirectory()
                               : File::getSpecialLocation(File::userDocumentsDirectory)
                                     .getChildFile(MoToolApp::getApp().getApplicationName());

    auto newEditName = defaultLocation.getChildFile(currentFile.getFileName());
    newEditName = te::getNonExistentSiblingWithIncrementedNumberSuffix(newEditName, false);
    newEditName.getParentDirectory().createDirectory();
    return newEditName;
}

StringArray getRecentEdits() {
    StringArray files;
    auto& storage = getMoToolPropertyStorage();
    files.addTokens(storage.getCustomProperty("recentEdits").toString(), ";", {});
    files.trim();
    files.removeEmptyStrings();

    while (files.size() > 8)
        files.remove(0);

    for (int i = files.size(); --i >= 0;) {
        const File f(files.getReference(i));

        if (!f.existsAsFile()) {
            files.remove(i);
        }
    }

    return files;
}

void addToRecentEdits(const File& f) {
    auto files = getRecentEdits();

    files.removeString(f.getFullPathName());
    files.insert(0, f.getFullPathName());

    getMoToolPropertyStorage().setCustomProperty("recentEdits", files.joinIntoString(";"));
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

File getRecentEditFile() {
    auto recent = getRecentEdits();
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

File getStartupEditFile() {
    auto f = getRecentEditFile();
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
            .getChildFile(CharPointer_UTF8(ProjectInfo::projectName)).getChildFile("Renders");

    return rendersDir;
}

bool saveEdit(te::Edit& edit, bool warnOfFailure, bool forceSaveEvenIfNotModified, bool offerToDiscardChanges) {
    auto efo = te::EditFileOperations(edit);
    if (efo.save(warnOfFailure, forceSaveEvenIfNotModified, offerToDiscardChanges)) {
        addToRecentEdits(efo.getEditFile());
        setRecentEditsDirectory(efo.getEditFile());
        return true;
    }
    return false;
}

bool saveEditAs(te::Edit& edit, const File& file) {
    auto efo = te::EditFileOperations(edit);
    if (efo.saveAs(file)) {
        edit.editFileRetriever = [file] { return file; };

        addToRecentEdits(file);
        setRecentEditsDirectory(file);
        return true;
    }
    return false;
}

bool saveEditAsWithDialog(te::Edit& edit) {
    auto newEditName = suffestSaveAsFileName(edit);

    juce::FileChooser fc("Save As...", newEditName, getAppFileGlob());
    if (fc.browseForFileToSave(false)) {
        auto savedFile = fc.getResult().withFileExtension(getDefaultEditFileSuffix());
        saveEditAs(edit, savedFile);
        return true;
    }
    return false;
}

} // namespace MoTool::EditFileOps
