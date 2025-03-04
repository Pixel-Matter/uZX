#pragma once

#include <JuceHeader.h>

#include <string>

namespace te = tracktion;

namespace MoTool::Helpers {

inline static constexpr std::string MIDI_WILDCARD {"*.mid;*.midi"};
inline static constexpr std::string PSG_WILDCARD {"*.psg;*.ay"};

inline void browseForFile(te::Engine& engine, const String& name, const String& wildcard, std::function<void (const File&)> fileChosenCallback) {
    DBG("Default save directory is " << engine.getPropertyStorage().getDefaultLoadSaveDirectory(ProjectInfo::projectName).getFullPathName());
    auto fc = std::make_shared<FileChooser>("Please select " + name + " file to load...",
                                            engine.getPropertyStorage().getDefaultLoadSaveDirectory(ProjectInfo::projectName),
                                            wildcard);

    fc->launchAsync(FileBrowserComponent::openMode + FileBrowserComponent::canSelectFiles,
                        [fc, &engine, callback = std::move(fileChosenCallback)] (const FileChooser&) {
                            const auto f = fc->getResult();

                            if (f.existsAsFile()) {
                                DBG("Set save directory is " << f.getParentDirectory().getFullPathName());
                                engine.getPropertyStorage().setDefaultLoadSaveDirectory(ProjectInfo::projectName, f.getParentDirectory());
                                DBG("Set save directory is " << engine.getPropertyStorage().getDefaultLoadSaveDirectory(ProjectInfo::projectName).getFullPathName());
                            }

                            callback(f);
                        });
}

inline void browseForAudioFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "an audio", engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats(), fileChosenCallback);
}

inline void browseForMidiFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "a MIDI", String(MIDI_WILDCARD), fileChosenCallback);
}

inline void browseForPSGFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "a PSG", String(PSG_WILDCARD), fileChosenCallback);
}

inline te::TimeRange getEffectiveClipsTimeRange(te::Edit& edit) {
    te::TimeRange result;

    // Visit all tracks and collect clip ranges
    edit.visitAllTracksRecursive([&](te::Track& track) {
        if (auto clipTrack = dynamic_cast<te::ClipTrack*>(&track)) {
            auto trackRange = clipTrack->getTotalRange();

            if (!trackRange.isEmpty()) {
                if (result.isEmpty()) {
                    result = trackRange;
                } else {
                    result = result.getUnionWith(trackRange);
                }
            }
        }
        return true; // Continue visiting tracks
    });

    return result;
}

}  // namespace MoToolHelpers
