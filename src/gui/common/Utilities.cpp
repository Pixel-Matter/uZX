#include <JuceHeader.h>

#include "Utilities.h"

#include <common/Utilities.h>  // from Tracktion

#include <string>

namespace te = tracktion;

namespace MoTool::Helpers {

static const std::string MIDI_WILDCARD {"*.mid;*.midi"};
static const std::string PSG_WILDCARD {"*.psg;*.ay"};

//==============================================================================

void browseForFiles(te::Engine& engine,
                    const String& name, const String& category, const String& wildcard, bool multiple,
                    std::function<void(const Array<File>&)> filesChosenCallback) {
    auto location = engine.getPropertyStorage().getDefaultLoadSaveDirectory(category);
    auto fc = std::make_shared<FileChooser>("Please select " + name + " file to load...", location, wildcard);

    auto flags = FileBrowserComponent::openMode + FileBrowserComponent::canSelectFiles;
    if (multiple)
        flags += FileBrowserComponent::canSelectMultipleItems;

    fc->launchAsync(flags,
                    [fc, &engine, category, callback = std::move(filesChosenCallback)](const FileChooser&) {
                        Array<File> files;
                        for (const auto& f : fc->getResults())
                            if (f.existsAsFile())
                                files.add(f);
                        if (! files.isEmpty()) {
                            auto& storage = engine.getPropertyStorage();
                            storage.setDefaultLoadSaveDirectory(category, files[0].getParentDirectory());
                        }

                        callback(files);
                    });
}

void browseForFile(te::Engine& engine,
                   const String& name, const String& category, const String& wildcard,
                   std::function<void(const File&)> fileChosenCallback) {
    browseForFiles(engine, name, category, wildcard, false,
                   [callback = std::move(fileChosenCallback)](const Array<File>& files) { callback(files[0]); });
}

void browseForAudioFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "an audio", "audio", engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats(), fileChosenCallback);
}

void browseForMidiFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "a MIDI", "midi", String(MIDI_WILDCARD), fileChosenCallback);
}

void browseForPSGFiles(te::Engine& engine, std::function<void (const Array<File>&)> filesChosenCallback) {
    browseForFiles(engine, "PSG", "psg", String(PSG_WILDCARD), true, std::move(filesChosenCallback));
}

}  // namespace MoTool::Helpers
