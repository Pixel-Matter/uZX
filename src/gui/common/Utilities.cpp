#include <JuceHeader.h>

#include "Utilities.h"
#include "../../models/EditUtilities.h"
#include "../../util/Helpers.h"

#include <common/Utilities.h>  // from Tracktion

#include <string>

namespace te = tracktion;

namespace MoTool::Helpers {

static const std::string MIDI_WILDCARD {"*.mid;*.midi"};

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
    browseForFiles(engine, "PSG", "psg", getTuneFileWildcard(), true, std::move(filesChosenCallback));
}

juce::PopupMenu buildTimecodeFormatMenu(te::Edit& edit) {
    const auto current = getTimecodeDisplayMode(edit);

    // The menu picks a display *mode* only; the frame rate stays the project fps
    // (changed separately via the transport fps readout).
    int nextId = 1;
    auto item = [&edit, current, &nextId](const String& name, TimecodeDisplayMode mode) {
        PopupMenu::Item it;
        it.itemID = nextId++;
        it.text = name;
        it.isTicked = (current == mode);
        it.action = [&edit, mode] { setTimecodeDisplayMode(edit, mode); };
        return it;
    };

    PopupMenu menu;
    menu.addItem(item("Bars.Beats.Frames", TimecodeDisplayMode::barsBeatsFrames));
    menu.addItem(item("Minutes:Seconds:Frames", TimecodeDisplayMode::seconds));
    menu.addItem(item("Frames", TimecodeDisplayMode::framesOnly));
    return menu;
}

juce::DialogWindow* launchDialogAsync(juce::DialogWindow::LaunchOptions& options) {
    const auto size = options.content->getBounds();
    auto* window = options.create();
    window->setContentComponentSize(size.getWidth(), size.getHeight());
    window->centreAroundComponent(options.componentToCentreAround, window->getWidth(), window->getHeight());
    window->enterModalState(true, nullptr, true);
    return window;
}

}  // namespace MoTool::Helpers
