#include "Helpers.h"
#include "../formats/psg/PsgFile.h"
#include "../formats/psg/TurboSound.h"
#include "../formats/tracker/TrackerModule.h"
#include "../models/EditUtilities.h"
#include "../models/PsgClip.h"
#include "../models/PsgTempoDetector.h"

#include <algorithm>
#include <atomic>
#include <list>
#include <optional>
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "../plugins/uZX/notes_to_psg/NotesToPsgPlugin.h"
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
    // Tracktion makes the scene list on first use, undoably, and a new audio track is that use: inside the
    // track's own insertion, so the two undo in the wrong order. Made here, it is no part of the undo.
    edit.state.getOrCreateChildWithName(te::IDs::SCENES, nullptr);
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

namespace {

const StringArray psgExtensions {"psg", "ay"};

/** Reads a PSG file, else a tracker module; shows an error and returns nothing when the file is
    neither or holds no music. A PSG file reads as a module of one chip without a title, table
    or rows. */
std::optional<uZX::TrackerModule> readTuneOrShowError(const File& file) {
    uZX::PsgFile psg(file);
    if (psg.ensureRead())
        return uZX::TrackerModule{{std::move(psg.getData())}, {}, {}, {}};

    // The largest Vortex Tracker text modules are a few hundred KB.
    constexpr int64 maxModuleSize = 4 * 1024 * 1024;
    MemoryBlock bytes;
    if (file.getSize() <= maxModuleSize && file.loadFileAsData(bytes))
        if (auto module = uZX::readTrackerModule(bytes))
            return module;

    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
        "Can't Open File",
        "'" + file.getFileName() + "' is not a PSG file or a tracker module, or contains no music.",
        "OK");
    return {};
}

}  // namespace

bool isTuneFile(const File& file) {
    return file.hasFileExtension(psgExtensions.joinIntoString(";") + ";"
                                 + uZX::getTrackerModuleExtensions().joinIntoString(";"));
}

String getTuneFileWildcard() {
    StringArray patterns;
    for (const auto* extensions : {&psgExtensions, &uZX::getTrackerModuleExtensions()})
        for (const auto& extension : *extensions)
            patterns.add("*." + extension);
    return patterns.joinIntoString(";");
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
    std::list<uZX::TrackerModule> tunes;  // their data, for the tempo detection below
    std::vector<const uZX::PsgData*> chips;
    for (const auto& f : files) {
        auto read = readTuneOrShowError(f);
        if (! read)
            continue;

        auto& tune = tunes.emplace_back(std::move(*read));
        const auto name = tune.title.isNotEmpty() ? tune.title : f.getFileNameWithoutExtension();
        for (size_t i = 0; i < tune.chips.size(); ++i) {
            auto& data = tune.chips[i];
            const auto chipName = tune.chips.size() > 1 ? name + " " + String((int) i + 1) : name;
            auto* track = clips.isEmpty() && firstTrack != nullptr ? firstTrack : addAudioTrackAfter(edit, nullptr);
            const bool emptyTrack = track->getClips().isEmpty();
            if (emptyTrack)
                track->setName(chipName);

            te::ClipPosition pos = {{start, te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}};
            // TODO make it ThreadBackgroundJob
            if (auto clip = PsgClip::insertTo(*track, data, pos, chipName)) {
                track->changed();
                setProjectFps(edit, data.getFrameRate());
                auto& chip = ensureAYPluginOnTrack(*track);
                // The clock a module was written for, on a track no other clip plays at its own clock
                if (emptyTrack && tune.chipClock)
                    chip.staticParams.chipClock.setStoredValue(*tune.chipClock / uZX::MHz);
                clips.add(clip.get());
                chips.push_back(&data);
            }
        }
    }
    if (clips.isEmpty())
        return clips;
    if (clips.size() > 1)
        PsgClip::linkAsOneTune(clips);

    // Files imported together are one tune: the first module's table and rows stand for it.
    const auto module = std::find_if(tunes.begin(), tunes.end(), [](const auto& t) { return ! t.rowStarts.empty(); });
    if (module != tunes.end() && module->table)
        clips.getFirst()->chooseTuningTable(*module->table);
    PsgTempo::applyToEdit(clips, chips, module != tunes.end() ? module->rowStarts : std::vector<size_t>{});

    // A module that loops back past its start loops there; the frames keep their times through the tempo above
    if (module != tunes.end() && module->loopFrame > 0) {
        const auto position = clips.getFirst()->getPosition();
        const auto loopStart = position.getStartOfSource()
                               + te::TimeDuration::fromSeconds((double) module->loopFrame
                                                               / module->chips.front().getFrameRate());
        edit.getTransport().setLoopRange({loopStart, position.getEnd()});
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

namespace {

class TrackRenderTask : public te::Renderer::RenderTask
{
public:
    enum class Outcome { Incomplete, Succeeded, Failed };

    TrackRenderTask(const String& description, const te::Renderer::Parameters& parameters)
        : RenderTask(description, parameters, nullptr, nullptr) {}

    bool canCancel() const override { return true; }
    Outcome getOutcome() const { return outcome_.load(); }

    float getCurrentTaskProgress() override {
        // Tracktion's progress can reach 100% while the render tail is still running.
        return getOutcome() == Outcome::Incomplete ? std::min(0.99f, RenderTask::getCurrentTaskProgress()) : 1.0f;
    }

    JobStatus runJob() override {
        const auto status = RenderTask::runJob();
        // The progress dialog also signals exit when closing after a successful run.
        if (status == jobHasFinished && !shouldExit())
            outcome_.store(errorMessage.isEmpty() ? Outcome::Succeeded : Outcome::Failed);
        return status;
    }

private:
    std::atomic<Outcome> outcome_{Outcome::Incomplete};
};

struct ScopedTrackUnmute
{
    explicit ScopedTrackUnmute(te::AudioTrack& source) : track(source), wasMuted(source.isMuted(false)) {
        track.setMute(false);
    }
    ~ScopedTrackUnmute() { track.setMute(wasMuted); }

    te::AudioTrack& track;
    const bool wasMuted;
};

te::Renderer::Parameters makeTrackRenderParameters(te::AudioTrack& track, const File& destination) {
    auto& edit = track.edit;
    auto& dm = edit.engine.getDeviceManager();
    juce::Array<te::EditItemID> trackIDs { track.itemID };

    te::Renderer::Parameters r(edit);
    r.tracksToDo.setBit(track.getIndexInEditTrackList());
    r.destFile = destination;
    r.audioFormat = edit.engine.getAudioFileFormatManager().getDefaultFormat();
    r.blockSizeForAudio = dm.getBlockSize();
    r.sampleRateForAudio = dm.getSampleRate();
    r.time = { {}, track.getLengthIncludingInputTracks() };
    r.endAllowance = te::RenderOptions::findEndAllowance(edit, &trackIDs, nullptr);
    r.checkNodesForAudio = false;
    r.canRenderInMono = true;
    r.mustRenderInMono = false;
    r.usePlugins = true;
    r.useMasterPlugins = false;
    return r;
}

bool renderTrackToFile(te::AudioTrack& track, const te::Renderer::Parameters& parameters, const String& what = "audio") {
    auto& edit = track.edit;
    auto& ui = edit.engine.getUIBehaviour();
    const auto samples = te::toSamples(parameters.time.getLength(), parameters.sampleRateForAudio);
    if (samples <= 0) {
        ui.showWarningMessage("This track has no audio to render.");
        return false;
    }

    const te::Edit::ScopedRenderStatus renderStatus(edit, true);
    const ScopedTrackUnmute unmute(track);
    te::TransportControl::stopAllTransports(edit.engine, false, true);
    TrackRenderTask task(String("Rendering track \"") + track.getName() + "\" to " + what, parameters);
    ui.runTaskWithProgressBar(task);
    if (task.getOutcome() == TrackRenderTask::Outcome::Incomplete)
        return false;

    if (task.getOutcome() == TrackRenderTask::Outcome::Failed) {
        ui.showWarningMessage(task.errorMessage);
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(
        te::AudioFileUtils::createReaderFor(edit.engine, parameters.destFile));
    if (reader == nullptr || reader->lengthInSamples < samples) {
        ui.showWarningMessage("The render did not produce a complete audio file.");
        return false;
    }

    return true;
}

/** Keeps the edit's undo transaction open until the messages queued by now have run. Tracktion sorts a track's
    children after a clip is added, later and as an undoable change, and starts a new transaction 350 ms after
    the last change: on a slow machine the sort would be an undo step of its own, and the first undo after a
    render would undo only that. */
void keepUndoTransactionForQueuedMessages(te::Edit& edit) {
    juce::MessageManager::callAsync([inhibitor = te::Edit::UndoTransactionInhibitor(edit)] {});
}

}  // namespace

te::AudioTrack* renderSelectedTracksToAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager) {
    auto& ui = edit.engine.getUIBehaviour();
    const te::AudioTrack::Ptr track(dynamic_cast<te::AudioTrack*>(selectionManager.getSelectedObject(0)));
    if (track == nullptr || !te::containsTrack(edit, *track)) {
        ui.showWarningMessage("Select an audio track to render.");
        return nullptr;
    }

    const auto freezeFile = getFreezeFileForTrack(*track);
    if (!freezeFile.getParentDirectory().isDirectory()) {
        ui.showWarningMessage("Could not create the folder for rendered audio.");
        return nullptr;
    }

    const juce::TemporaryFile staging(freezeFile, juce::TemporaryFile::useHiddenFile);
    const auto parameters = makeTrackRenderParameters(*track, staging.getFile());
    if (!renderTrackToFile(*track, parameters))
        return nullptr;

    if (!te::containsTrack(edit, *track)) {
        ui.showWarningMessage("The source track was removed during rendering.");
        return nullptr;
    }

    if (freezeFile.exists() || !staging.getFile().moveFileTo(freezeFile)) {
        ui.showWarningMessage("Could not save the rendered audio file.");
        return nullptr;
    }

    auto& undo = edit.getUndoManager();
    undo.beginNewTransaction("Render to audio");
    auto* added = addAudioTrackAfter(edit, track.get());
    auto clip = added != nullptr
        ? added->insertWaveClip(String("Frozen ") + track->getName(), freezeFile, {parameters.time, {}}, false)
        : te::WaveAudioClip::Ptr{};
    if (clip == nullptr) {
        undo.undoCurrentTransactionOnly();
        if (freezeFile.deleteFile())
            ui.showWarningMessage("Could not add the rendered audio to a new track.");
        else
            ui.showWarningMessage("Could not add the rendered audio to a new track. "
                                  "The audio file is still in the Renders folder.");
        return nullptr;
    }

    clip->setAutoTempo(false);
    clip->setAutoPitch(false);
    track->state.setProperty(te::IDs::mute, true, &undo);
    selectionManager.select({added});
    keepUndoTransactionForQueuedMessages(edit);
    return added;
}

static uZX::NotesToPsgPlugin* findMixerOnSelectedTrack(te::SelectionManager& selectionManager) {
    auto* track = dynamic_cast<te::AudioTrack*>(selectionManager.getSelectedObject(0));
    return track != nullptr ? track->pluginList.findFirstPluginOfType<uZX::NotesToPsgPlugin>() : nullptr;
}

bool canRenderSelectedTrackToPsg(te::SelectionManager& selectionManager) {
    return findMixerOnSelectedTrack(selectionManager) != nullptr;
}

te::AudioTrack* renderSelectedTrackToPsgTrack(te::Edit& edit, te::SelectionManager& selectionManager) {
    auto& ui = edit.engine.getUIBehaviour();
    const te::AudioTrack::Ptr track(dynamic_cast<te::AudioTrack*>(selectionManager.getSelectedObject(0)));
    const uZX::NotesToPsgPlugin::Ptr mixer(findMixerOnSelectedTrack(selectionManager));
    if (mixer == nullptr || !te::containsTrack(edit, *track)) {
        ui.showWarningMessage("Select a track with MIDI to PSG to render.");
        return nullptr;
    }

    auto rendersDir = EditFileOps::getRendersDirectory(edit);
    rendersDir.createDirectory();
    if (!rendersDir.isDirectory()) {
        ui.showWarningMessage("Could not create the folder for rendered files.");
        return nullptr;
    }

    // The track plays as for a render to audio, which runs its plugins offline; the audio is thrown away and
    // what MIDI to PSG writes on the way is kept
    const juce::TemporaryFile audio(rendersDir.getChildFile("psg.wav"), juce::TemporaryFile::useHiddenFile);
    const auto parameters = makeTrackRenderParameters(*track, audio.getFile());
    PsgRecorder recorder(uZX::noteFrameRate);
    mixer->setRecorder(&recorder);
    const bool rendered = renderTrackToFile(*track, parameters, "PSG");
    mixer->setRecorder(nullptr);
    if (!rendered)
        return nullptr;

    if (!te::containsTrack(edit, *track)) {
        ui.showWarningMessage("The source track was removed during rendering.");
        return nullptr;
    }

    auto data = recorder.finish(parameters.time.getLength().inSeconds());
    if (std::all_of(data.frames.begin(), data.frames.end(), [](const auto& frame) { return frame.isEmpty(); })) {
        ui.showWarningMessage("The track sent nothing to the chip, so there is no PSG to render.");
        return nullptr;
    }

    const auto psgFile = te::getNonExistentSiblingWithIncrementedNumberSuffix(
        rendersDir.getChildFile(File::createLegalFileName(track->getName()) + ".psg"), false);
    {
        uZX::PsgFile file(psgFile);
        file.getData() = data;
        FileOutputStream stream(psgFile);
        if (!stream.openedOk() || !file.writeTo(stream)) {
            stream.flush();
            psgFile.deleteFile();
            ui.showWarningMessage("Could not save the PSG file.");
            return nullptr;
        }
    }

    auto& undo = edit.getUndoManager();
    undo.beginNewTransaction("Render to PSG");
    auto* added = addAudioTrackAfter(edit, track.get());
    const te::ClipPosition position {{te::TimePosition(), te::TimeDuration::fromSeconds(data.getLengthSeconds())}, {}};
    auto clip = added != nullptr ? PsgClip::insertTo(*added, data, position, track->getName()) : PsgClip::Ptr {};
    if (clip == nullptr) {
        undo.undoCurrentTransactionOnly();
        ui.showWarningMessage("Could not add the rendered PSG to a new track. The file is in the Renders folder: "
                              + psgFile.getFileName());
        return nullptr;
    }

    added->setName(track->getName() + " PSG");
    // The periods were made for the clock MIDI to PSG played at
    ensureAYPluginOnTrack(*added).staticParams.chipClock.setStoredValue(mixer->getChipClock() / uZX::MHz);
    track->state.setProperty(te::IDs::mute, true, &undo);
    selectionManager.select({added});
    keepUndoTransactionForQueuedMessages(edit);
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
