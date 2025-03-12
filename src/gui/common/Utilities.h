#pragma once

#include <JuceHeader.h>

#include "../../formats/psg/PsgFile.h"
#include "../../model/PsgClip.h"

#include <common/Utilities.h>

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


inline te::AudioTrack* getSelectedOrInsertAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager) {
    auto sel = selectionManager.getSelectedObject(0);
    auto track = dynamic_cast<te::AudioTrack*>(sel);
    if (track == nullptr) {
        track = EngineHelpers::getOrInsertAudioTrackAt(edit, 0);
    }
    return track;
}

inline void importPsgAsClip(te::Edit &edit, te::SelectionManager& selectionManager, bool insertAtCursor = false) {
    Helpers::browseForPSGFile(edit.engine, [&](const File& f) {
        auto track = getSelectedOrInsertAudioTrack(edit, selectionManager);
        auto psgFile = uZX::PsgFile(f);
        psgFile.ensureRead();
        te::TimePosition insertTime;
        if (insertAtCursor) {
            insertTime = edit.getTransport().getPosition();
        }
        te::ClipPosition pos = {{insertTime, te::TimeDuration::fromSeconds(psgFile.getLengthSeconds())}, {}};
        if (auto inserted = PsgClip::insertTo(*track, psgFile, pos)) {
            DBG("Inserted clip: " << inserted->getName());
        }
    });
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
