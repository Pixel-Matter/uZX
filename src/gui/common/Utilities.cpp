#include <JuceHeader.h>

#include "Utilities.h"

#include <common/Utilities.h>  // from Tracktion

#include <string>

namespace te = tracktion;

namespace MoTool::Helpers {

static constexpr std::string MIDI_WILDCARD {"*.mid;*.midi"};
static constexpr std::string PSG_WILDCARD {"*.psg;*.ay"};

//==============================================================================

void browseForFile(te::Engine& engine,
                   const String& name, const String& category, const String& wildcard,
                   std::function<void(const File&)> fileChosenCallback) {
    auto location = engine.getPropertyStorage().getDefaultLoadSaveDirectory(category);
    DBG("Default save location is " << location.getFullPathName());
    auto fc = std::make_shared<FileChooser>("Please select " + name + " file to load...", location, wildcard);

    fc->launchAsync(FileBrowserComponent::openMode + FileBrowserComponent::canSelectFiles,
                    [fc, &engine, category, callback = std::move(fileChosenCallback)](const FileChooser&) {
                        const auto f = fc->getResult();
                        if (f.existsAsFile()) {
                            auto& storage = engine.getPropertyStorage();
                            storage.setDefaultLoadSaveDirectory(category, f.getParentDirectory());
                        }

                        callback(f);
                    });
}

void browseForAudioFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "an audio", "audio", engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats(), fileChosenCallback);
}

void browseForMidiFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "a MIDI", "midi", String(MIDI_WILDCARD), fileChosenCallback);
}

void browseForPSGFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "a PSG", "psg", String(PSG_WILDCARD), fileChosenCallback);
}

}  // namespace MoTool::Helpers
