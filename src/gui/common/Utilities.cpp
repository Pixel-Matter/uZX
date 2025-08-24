#include <JuceHeader.h>

#include "Utilities.h"
#include "../../formats/psg/PsgFile.h"
#include "../../models/PsgClip.h"
#include "../../plugins/uZX/aychip/AYPlugin.h"

#include <common/Utilities.h>  // from Tracktion

#include <string>

namespace te = tracktion;

namespace MoTool::Helpers {

static constexpr std::string MIDI_WILDCARD {"*.mid;*.midi"};
static constexpr std::string PSG_WILDCARD {"*.psg;*.ay"};

void browseForFile(te::Engine& engine, const String& name, const String& wildcard, std::function<void (const File&)> fileChosenCallback) {
    auto location = engine.getPropertyStorage().getDefaultLoadSaveDirectory(CharPointer_UTF8(ProjectInfo::projectName));
    // DBG("Default save location is " << dir.getFullPathName());
    auto fc = std::make_shared<FileChooser>("Please select " + name + " file to load...",
                                            location, wildcard);

    fc->launchAsync(FileBrowserComponent::openMode + FileBrowserComponent::canSelectFiles,
                        [fc, /*&engine,*/ callback = std::move(fileChosenCallback)] (const FileChooser&) {
                            const auto f = fc->getResult();
                            // if (f.existsAsFile()) {
                                // NOTE do not work, but we should define own PropertyStorage to it to work
                                // DBG("Set save directory is " << f.getParentDirectory().getFullPathName());
                                // engine.getPropertyStorage().setDefaultLoadSaveDirectory(CharPointer_UTF8(ProjectInfo::projectName), f.getParentDirectory());
                                // DBG("Set save directory is " << engine.getPropertyStorage().getDefaultLoadSaveDirectory(CharPointer_UTF8(ProjectInfo::projectName)).getFullPathName());
                            // }

                            callback(f);
                        });
}

void browseForAudioFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "an audio", engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats(), fileChosenCallback);
}

void browseForMidiFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "a MIDI", String(MIDI_WILDCARD), fileChosenCallback);
}

void browseForPSGFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "a PSG", String(PSG_WILDCARD), fileChosenCallback);
}

}  // namespace MoTool::Helpers
