#include "Helpers.h"
#include "../formats/psg/PsgFile.h"
#include "../formats/psg/TurboSound.h"
#include "../models/PsgClip.h"
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "../gui/common/Utilities.h"
#include "../util/FileOps.h"

#include <common/Utilities.h>  // from Tracktion

namespace te = tracktion;

namespace MoTool::Helpers {

void visitAllMidiClips(te::Edit& edit, std::function<bool (te::MidiClip&)> func) {
    for (auto track : getClipTracks(edit)) {
        for (auto clip : track->getClips()) {
            if (dynamic_cast<te::MidiClip*>(clip)) {
                if (!func(*static_cast<te::MidiClip*>(clip))) {
                    return;
                }
            }
        }
    }
}

static void rescaleClipToFit(te::MidiClip& clip) {
    int maxNote = 0;
    int minNote = 256;

    for (auto n : clip.getSequence().getNotes()) {
        maxNote = std::max(maxNote, n->getNoteNumber() + 3);
        minNote = std::min(minNote, n->getNoteNumber() - 3);
    }
    if (minNote < maxNote) {
        const double newVisProp = (maxNote - minNote) / 128.0;
        clip.getAudioTrack()->setMidiVerticalPos(newVisProp, 1.0 - (maxNote / 128.0));
    }
}

void rescaleAllMidiClipsToFit(te::Edit& edit) {
    visitAllMidiClips(edit, [] (te::MidiClip& clip) {
        rescaleClipToFit(clip);
        return true; // continue
    });
}

// TODO see Edit::something for implementation
te::TimeRange getEffectiveClipsTimeRange(te::Edit& edit) {
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

te::AudioTrack* addAudioTrackAfter(te::Edit& edit, te::Track* track) {
    auto insertPoint = (track != nullptr) ? track : getAllTracks(edit).getLast();
    edit.getTransport().stopIfRecording();
    return edit.insertNewAudioTrack(te::TrackInsertPoint(nullptr, insertPoint), nullptr).get();
}

te::AudioTrack* getSelectedOrInsertAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager, String name) {
    auto sel = selectionManager.getSelectedObject(0);
    auto track = dynamic_cast<te::AudioTrack*>(sel);
    if (track == nullptr) {
        track = addAudioTrackAfter(edit, nullptr);
        if (name.isNotEmpty()) {
            track->setName(name);
        }
    }
    return track;
}

te::AudioTrack* addAndSelectAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager) {
    auto sel = selectionManager.getSelectedObject(0);
    auto added = addAudioTrackAfter(edit, dynamic_cast<te::AudioTrack*>(sel));
    selectionManager.select({added});
    return added;
}

// inline te::AutomationTrack* addAutomationTrackAfter(te::Edit& edit, te::Track* track) {
//     auto insertPoint = (track != nullptr) ? track : getAllTracks(edit).getLast();
//     edit.getTransport().stopIfRecording();
//     return edit.insertNewAutomationTrack(te::TrackInsertPoint(nullptr, insertPoint), nullptr).get();
// }

// inline te::AutomationTrack* addAndSelectAutomationTrack(te::Edit& edit, te::SelectionManager& selectionManager) {
//     auto sel = selectionManager.getSelectedObject(0);
//     auto added = addAutomationTrackAfter(edit, dynamic_cast<te::AudioTrack*>(sel));
//     selectionManager.select({added});
//     return added;
// }

bool readPsgOrShowError(uZX::PsgFile& psgFile) {
    if (psgFile.ensureRead())
        return true;

    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
        "Can't Open PSG File",
        "'" + psgFile.getFile().getFileName() + "' is not a PSG file or contains no music.",
        "OK");
    return false;
}

uZX::AYChipPlugin& ensureAYPluginOnTrack(te::AudioTrack& track) {
    if (auto* chip = track.pluginList.findFirstPluginOfType<uZX::AYChipPlugin>())
        return *chip;
    auto plugin = track.edit.getPluginCache().createNewPlugin(uZX::AYChipPlugin::xmlTypeName, {});
    track.pluginList.insertPlugin(plugin, 0, nullptr);
    return *dynamic_cast<uZX::AYChipPlugin*>(plugin.get());
}

Array<PsgClip*> importPsgFiles(te::Edit& edit, const Array<File>& files, te::TimePosition start,
                               te::AudioTrack* firstTrack) {
    Array<PsgClip*> clips;
    for (const auto& f : files) {
        auto psgFile = uZX::PsgFile(f);
        if (! readPsgOrShowError(psgFile))
            continue;

        auto* track = clips.isEmpty() && firstTrack != nullptr ? firstTrack : addAudioTrackAfter(edit, nullptr);
        if (track->getClips().isEmpty())
            track->setName(f.getFileNameWithoutExtension());

        te::ClipPosition pos = {{start, te::TimeDuration::fromSeconds(psgFile.getData().getLengthSeconds())}, {}};
        // TODO make it ThreadBackgroundJob
        if (auto clip = PsgClip::insertTo(*track, psgFile, pos)) {
            track->changed();
            ensureAYPluginOnTrack(*track);
            clips.add(clip.get());
        }
    }
    return clips;
}

void withTurboSoundSet(const Array<File>& files, std::function<void (const Array<File>&)> callback) {
    const auto set = files.size() == 1 ? uZX::findTurboSoundSet(files[0]) : Array<File>();
    if (set.isEmpty()) {
        callback(files);
        return;
    }

    StringArray names;
    for (const auto& f : set)
        names.add(f.getFileName());

    auto options = MessageBoxOptions()
        .withIconType(MessageBoxIconType::QuestionIcon)
        .withTitle("Turbo Sound")
        .withMessage("\"" + files[0].getFileName() + "\" looks like one chip of a Turbo Sound tune: "
                     + names.joinIntoString(", ") + ". Open all of them, each on its own track?")
        .withButton("All " + String(set.size()) + " Files")
        .withButton("This File Only")
        .withButton("Cancel");

    NativeMessageBox::showAsync(options, [files, set, callback = std::move(callback)](int result) {
        if (result == 0)
            callback(set);
        else if (result == 1)
            callback(files);
    });
}

void importPsgAsClip(te::Edit& targetEdit, te::SelectionManager& selectionManager, bool insertAtCursor) {
    // The macOS file panel is not modal: the edit may be closed before a file is picked.
    browseForPSGFiles(targetEdit.engine, [safeEdit = te::SafeSelectable<te::Edit>(targetEdit), &selectionManager,
                                         insertAtCursor](const Array<File>& picked) {
        withTurboSoundSet(picked, [safeEdit, &selectionManager, insertAtCursor](const Array<File>& files) {
            if (safeEdit == nullptr)
                return;
            auto& edit = *safeEdit.get();
            auto* selected = files.size() == 1 ? dynamic_cast<te::AudioTrack*>(selectionManager.getSelectedObject(0))
                                               : nullptr;
            importPsgFiles(edit, files, insertAtCursor ? edit.getTransport().getPosition() : te::TimePosition(),
                           selected);
        });
    });
}

void importAudioAsClip(te::Edit& targetEdit, te::SelectionManager& selectionManager, bool insertAtCursor) {
    browseForAudioFile(targetEdit.engine, [safeEdit = te::SafeSelectable<te::Edit>(targetEdit), &selectionManager,
                                          insertAtCursor](const File& f) {
        if (safeEdit == nullptr || !f.existsAsFile()) {
            return;
        }
        auto& edit = *safeEdit.get();

        // Validate audio file
        te::AudioFile audioFile(edit.engine, f);
        if (!audioFile.isValid()) {
            edit.engine.getUIBehaviour().showWarningMessage("Invalid audio file: " + f.getFileName());
            return;
        }

        // Get or create track
        auto track = getSelectedOrInsertAudioTrack(edit, selectionManager, f.getFileNameWithoutExtension());
        jassert(track != nullptr);

        // Rename track if empty
        if (track->getClips().size() == 0) {
            track->setName(f.getFileNameWithoutExtension());
        }

        // Determine insert position
        te::TimePosition insertTime;
        if (insertAtCursor) {
            insertTime = edit.getTransport().getPosition();
        }

        // Create clip position with audio file duration
        te::ClipPosition pos = {{insertTime, te::TimeDuration::fromSeconds(audioFile.getLength())}, {}};

        // Insert audio clip
        if (auto clip = track->insertWaveClip(
            f.getFileNameWithoutExtension(),
            f,
            pos,
            false  // deleteExistingClips
        )) {
            // Disable time-stretching features
            clip->setAutoTempo(false);
            clip->setAutoPitch(false);

            // Notify track of changes
            track->changed();
        } else {
            edit.engine.getUIBehaviour().showWarningMessage("Failed to insert audio clip");
        }
    });
}

void removeUnusedRenderFiles(te::Edit& edit) {
    auto rendersDir = EditFileOps::getRendersDirectory(edit);
    TRACKTION_LOG("Cleaning renders directory " + rendersDir.getFullPathName());

    Array<File> usedFiles;
    for (auto audioTrack : getAudioTracks(edit))
        for (auto clip : audioTrack->getClips())
            if (auto waveClip = dynamic_cast<te::AudioClipBase*>(clip)) {
                usedFiles.add(waveClip->getOriginalFile());
            }

    for (auto& file : rendersDir.findChildFiles(File::findFiles, false, "0_*.wav")) {
        if (!usedFiles.contains(file)) {
            file.deleteFile();
        }
    }
}

File getFreezeFileForTrack(const te::AudioTrack& track) {
    auto location = EditFileOps::getRendersDirectory(track.edit);
    location.createDirectory();
    auto file = location.getChildFile("0_" + track.itemID.toString() + ".wav");

    // TODO on exit, remove all unused files in Renders directory
    file = te::getNonExistentSiblingWithIncrementedNumberSuffix(file, true);
    return file;
}

te::AudioTrack* renderSelectedTracksToAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager) {
    auto sel = selectionManager.getSelectedObject(0);
    auto track = dynamic_cast<te::AudioTrack*>(sel);
    if (track == nullptr) {
        edit.engine.getUIBehaviour().showWarningMessage("No track selected");
        return nullptr;
    }

    // render selected tracks to added track

    const bool shouldBeMuted = track->isMuted(true);
    track->setMute(false);

    auto freezeFile = getFreezeFileForTrack(*track);
    // DBG("Freeze file: " << freezeFile.getFullPathName());

    auto& dm = edit.engine.getDeviceManager();
    juce::Array<te::EditItemID> trackIDs { track->itemID };

    juce::BigInteger trackNum;
    trackNum.setBit(track->getIndexInEditTrackList());

    te::Renderer::Parameters r (edit);
    r.tracksToDo = trackNum;
    r.destFile = freezeFile;
    r.audioFormat = edit.engine.getAudioFileFormatManager().getDefaultFormat();
    r.blockSizeForAudio = dm.getBlockSize();
    r.sampleRateForAudio = dm.getSampleRate();
    r.time = { {}, track->getLengthIncludingInputTracks() };
    r.endAllowance = te::RenderOptions::findEndAllowance(edit, &trackIDs, nullptr);
    r.checkNodesForAudio = false;
    r.canRenderInMono = true;
    r.mustRenderInMono = false;
    r.usePlugins = true;
    r.useMasterPlugins = false;

    const te::Edit::ScopedRenderStatus srs (edit, true);
    const auto desc = String("Rendering track \"") + track->getName() + "\" to audio";

    if (getProjectForEdit(edit) != nullptr) {
        te::Renderer::renderToProjectItem(desc, r, te::ProjectItem::Category::frozen);
    } else {
        te::Renderer::renderToFile(desc, r);
    }

    if (!r.destFile.existsAsFile()) {
        edit.engine.getUIBehaviour().showWarningMessage("Render failed");
        track->setMute(shouldBeMuted);
        return nullptr;
    }

    // add audio file to added track (name, file, position, deleteExistingClips)
    auto added = addAudioTrackAfter(edit, track);
    auto clip = added->insertWaveClip(String("Frozen ") + track->getName(), freezeFile, {r.time, {}}, true);
    if (clip != nullptr) {
        clip->setAutoTempo(false);
        clip->setAutoPitch(false);
    }

    track->setMute(true);  // mute original track
    selectionManager.select({added});
    return added;
}

File getLastCsvExportDirectory() {
    PropertiesFile::Options options;
    options.applicationName = JUCEApplication::getInstance()->getApplicationName();
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";

    PropertiesFile settings(options);
    String lastExportPath = settings.getValue("lastCsvExportDirectory",
                                             File::getSpecialLocation(File::userDesktopDirectory).getFullPathName());
    File lastExportDir(lastExportPath);

    // If the saved directory doesn't exist anymore, fallback to desktop
    if (!lastExportDir.exists() || !lastExportDir.isDirectory()) {
        return File::getSpecialLocation(File::userDesktopDirectory);
    }

    return lastExportDir;
}

void setLastCsvExportDirectory(const File& directory) {
    PropertiesFile::Options options;
    options.applicationName = JUCEApplication::getInstance()->getApplicationName();
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";

    PropertiesFile settings(options);
    settings.setValue("lastCsvExportDirectory", directory.getFullPathName());
    settings.saveIfNeeded();
}

}  // namespace MoTool::Helpers