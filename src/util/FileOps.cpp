#include "FileOps.h"
#include <JuceHeader.h>

namespace MoTool::EditFileOps {

String getAppFileGlob() {
    return "*" + String(EDIT_FILE_SUFFIX);
}

File findRecentEdit(const File& dir, const String& fileGlob) {
    auto files = dir.findChildFiles(File::findFiles, false, fileGlob);
    if (files.size() > 0) {
        files.sort();
        return files.getLast();
    }
    return {};
}

File getRecentEditFile() {
    auto d = File::getSpecialLocation(File::tempDirectory).getChildFile(ProjectInfo::projectName);
    d.createDirectory();

    auto f = findRecentEdit(d, getAppFileGlob());
    if (f.existsAsFile()) {
        return f;
    } else {
        return {};
    }
}

File getTempEditFile() {
    auto d = File::getSpecialLocation(File::tempDirectory).getChildFile(ProjectInfo::projectName);
    d.createDirectory();
    return d.getNonexistentChildFile("Unnamed", EDIT_FILE_SUFFIX, false);
}

File getStartupEditFile() {
    auto f = getRecentEditFile();
    if (!f.existsAsFile()) {
        f = getTempEditFile();
    }
    DBG("Startup file is located at " << f.getFullPathName());
    return f;
}

} // namespace MoTool::EditFileOps
