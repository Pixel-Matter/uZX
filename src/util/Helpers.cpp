#include "Helpers.h"
#include "../formats/psg/PsgFile.h"
#include "../models/PsgClip.h"
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "../gui/common/Utilities.h"

#include <common/Utilities.h>  // from Tracktion

namespace te = tracktion;

namespace MoTool::Helpers {

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

void importPsgAsClip(te::Edit &edit, te::SelectionManager& selectionManager, bool insertAtCursor) {
    browseForPSGFile(edit.engine, [&](const File& f) {
        auto track = getSelectedOrInsertAudioTrack(edit, selectionManager, f.getFileNameWithoutExtension());
        jassert(track != nullptr);
        if (track->getClips().size() == 0) {
            track->setName(f.getFileNameWithoutExtension());
        }
        auto psgFile = uZX::PsgFile(f);
        psgFile.ensureRead();
        te::TimePosition insertTime;
        if (insertAtCursor) {
            insertTime = edit.getTransport().getPosition();
        }
        te::ClipPosition pos = {{insertTime, te::TimeDuration::fromSeconds(psgFile.getData().getLengthSeconds())}, {}};
        // TODO make it ThreadBackgroundJob
        if (auto inserted = PsgClip::insertTo(*track, psgFile, pos)) {
            track->changed();
            // make sure AYChipPlugin is added to the track
            for (auto& p : track->pluginList) {
                if (dynamic_cast<uZX::AYChipPlugin*>(p)) {
                    return;
                }
            }
            auto plugin = edit.getPluginCache().createNewPlugin(uZX::AYChipPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin(plugin, 0, nullptr);
        }
    });
}

File getRendersDirectory(te::Edit& edit) {
    auto editFile = edit.editFileRetriever();
    File rendersDir = editFile.existsAsFile()
        ? editFile.getParentDirectory().getChildFile(editFile.getFileNameWithoutExtension()).withFileExtension("Renders")
        : File::getSpecialLocation(File::userMusicDirectory)
            .getChildFile(CharPointer_UTF8(ProjectInfo::projectName)).getChildFile("Renders");

    rendersDir.createDirectory();
    return rendersDir;
}

void removeUnusedRenderFiles(te::Edit& edit) {
    auto rendersDir = getRendersDirectory(edit);
    DBG("Cleaning renders directory " << rendersDir.getFullPathName());

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
    auto location = getRendersDirectory(track.edit);
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

// juce::String formatTimecodeDisplay(const TempoSequence& tempo, const TimePosition time, bool isRelative) const
// {
//     if (type == TimecodeType::barsBeats)
//     {
//         tempo::BarsAndBeats barsBeats;
//         int bars, beats;
//         BeatDuration fraction;

//         if (! isRelative)
//         {
//             barsBeats = tempo.toBarsAndBeats (time + nudge);
//             bars = barsBeats.bars + 1;
//             beats = barsBeats.getWholeBeats() + 1;
//             fraction = barsBeats.getFractionalBeats();
//         }
//         else if (time < 0s)
//         {
//             barsBeats = tempo.toBarsAndBeats (time - nudge);
//             bars = -barsBeats.bars - 1;
//             beats = (tempo.getTimeSig(0)->numerator - 1) - barsBeats.getWholeBeats();
//             fraction = BeatDuration::fromBeats (1.0) - barsBeats.getFractionalBeats();
//         }
//         else
//         {
//             barsBeats = tempo.toBarsAndBeats (time + nudge);
//             bars = barsBeats.bars + 1;
//             beats = barsBeats.getWholeBeats() + 1;
//             fraction = barsBeats.getFractionalBeats();
//         }

//         auto s = juce::String::formatted ("%d|%d|%03d", bars, beats, (int) (fraction.inBeats() * Edit::ticksPerQuarterNote));
//         return time < 0s ? ("-" + s) : s;
//     }

//     return TimecodeDisplayFormat::toFullTimecode (time, getSubSecondDivisions());
// }

File getLastCsvExportDirectory() {
    PropertiesFile::Options options;
    options.applicationName = CharPointer_UTF8(ProjectInfo::projectName);
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
    options.applicationName = CharPointer_UTF8(ProjectInfo::projectName);
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";

    PropertiesFile settings(options);
    settings.setValue("lastCsvExportDirectory", directory.getFullPathName());
    settings.saveIfNeeded();
}

}  // namespace MoTool::Helpers