#pragma once

#include <JuceHeader.h>

#include "../../formats/psg/PsgFile.h"
#include "../../model/PsgClip.h"
#include "juce_core/juce_core.h"

#include <common/Utilities.h>

#include <string>

namespace te = tracktion;

namespace MoTool::Helpers {

inline static constexpr std::string MIDI_WILDCARD {"*.mid;*.midi"};
inline static constexpr std::string PSG_WILDCARD {"*.psg;*.ay"};

inline void browseForFile(te::Engine& engine, const String& name, const String& wildcard, std::function<void (const File&)> fileChosenCallback) {
    auto location = engine.getPropertyStorage().getDefaultLoadSaveDirectory(CharPointer_UTF8(ProjectInfo::projectName));
    // DBG("Default save location is " << dir.getFullPathName());
    auto fc = std::make_shared<FileChooser>("Please select " + name + " file to load...",
                                            location, wildcard);

    fc->launchAsync(FileBrowserComponent::openMode + FileBrowserComponent::canSelectFiles,
                        [fc, &engine, callback = std::move(fileChosenCallback)] (const FileChooser&) {
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

inline void browseForAudioFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "an audio", engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats(), fileChosenCallback);
}

inline void browseForMidiFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "a MIDI", String(MIDI_WILDCARD), fileChosenCallback);
}

inline void browseForPSGFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback) {
    browseForFile(engine, "a PSG", String(PSG_WILDCARD), fileChosenCallback);
}

// TODO see Edit::something for implenetation
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


inline te::AudioTrack* getSelectedOrInsertAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager, String name = {}) {
    auto sel = selectionManager.getSelectedObject(0);
    auto track = dynamic_cast<te::AudioTrack*>(sel);
    if (track == nullptr) {
        track = EngineHelpers::getOrInsertAudioTrackAt(edit, 0);
        if (name.isNotEmpty()) {
            track->setName(name);
        }
    }
    return track;
}

inline te::AudioTrack* addAudioTrackAfter(te::Edit& edit, te::Track* track) {
    auto insertPoint = (track != nullptr) ? track : getAllTracks(edit).getLast();
    edit.getTransport().stopIfRecording();
    return edit.insertNewAudioTrack(te::TrackInsertPoint(nullptr, insertPoint), nullptr).get();
}

inline te::AudioTrack* addAndSelectAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager) {
    auto sel = selectionManager.getSelectedObject(0);
    auto added = addAudioTrackAfter(edit, dynamic_cast<te::AudioTrack*>(sel));
    selectionManager.select({added});
    return added;
}

inline void importPsgAsClip(te::Edit &edit, te::SelectionManager& selectionManager, bool insertAtCursor = false) {
    Helpers::browseForPSGFile(edit.engine, [&](const File& f) {
        auto track = getSelectedOrInsertAudioTrack(edit, selectionManager, f.getFileNameWithoutExtension());
        auto psgFile = uZX::PsgFile(f);
        psgFile.ensureRead();
        te::TimePosition insertTime;
        if (insertAtCursor) {
            insertTime = edit.getTransport().getPosition();
        }
        te::ClipPosition pos = {{insertTime, te::TimeDuration::fromSeconds(psgFile.getLengthSeconds())}, {}};
        // TODO make it ThreadBackgroundJob
        if (auto inserted = PsgClip::insertTo(*track, psgFile, pos)) {
            // make sure AYChipPlugin is added to the track
            for (auto& p : track->pluginList) {
                if (auto ay = dynamic_cast<uZX::AYChipPlugin*>(p)) {
                    return;
                }
            }
            auto plugin = edit.getPluginCache().createNewPlugin(uZX::AYChipPlugin::xmlTypeName, {});
            track->pluginList.insertPlugin(plugin, 0, nullptr);
        }
    });
}

inline File getRendersDirectory(te::Edit& edit) {
    auto editFile = edit.editFileRetriever();
    File rendersDir = editFile.existsAsFile()
        ? editFile.getParentDirectory().getChildFile(editFile.getFileNameWithoutExtension()).withFileExtension("Renders")
        : File::getSpecialLocation(File::userMusicDirectory)
            .getChildFile(CharPointer_UTF8(ProjectInfo::projectName)).getChildFile("Renders");

    rendersDir.createDirectory();
    return rendersDir;
}

inline File getFreezeFileForTrack(const te::AudioTrack& track) {
    auto location = getRendersDirectory(track.edit);
    auto file = location.getChildFile("0_" + track.itemID.toString() + "_0.wav");

    // TODO on exit, remove all unused files in Renders directory
    file = te::getNonExistentSiblingWithIncrementedNumberSuffix(file, false);
    return file;
}

inline te::AudioTrack* renderSelectedTracksToAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager) {
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
    DBG("Freeze file: " << freezeFile.getFullPathName());

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

// inline juce::String formatTimecodeDisplay(const TempoSequence& tempo, const TimePosition time, bool isRelative) const
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

}  // namespace MoToolHelpers
