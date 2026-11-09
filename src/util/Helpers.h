#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool { class PsgClip; }
namespace MoTool::uZX { class AYChipPlugin; }

namespace MoTool::Helpers {

void visitAllMidiClips(te::Edit& edit, std::function<bool (te::MidiClip&)> func);
void rescaleAllMidiClipsToFit(te::Edit& edit);

te::TimeRange getEffectiveClipsTimeRange(te::Edit& edit);

te::AudioTrack* addAudioTrackAfter(te::Edit& edit, te::Track* track);
te::AudioTrack* getSelectedOrInsertAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager, String name = {});
te::AudioTrack* addAndSelectAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager);

// te::AutomationTrack* addAutomationTrackAfter(te::Edit& edit, te::Track* track);
// te::AutomationTrack* addAndSelectAutomationTrack(te::Edit& edit, te::SelectionManager& selectionManager);

/** Whether the file's extension is one of a tune uZX imports: a PSG register dump or a tracker
    module (see uZX::getTrackerModuleExtensions). */
bool isTuneFile(const File& file);
/** The same extensions as a file chooser wildcard: "*.psg;*.ay;*.pt3;...". */
String getTuneFileWildcard();

/** The track's AY chip, added first if the track has none. */
uZX::AYChipPlugin& ensureAYPluginOnTrack(te::AudioTrack& track);

/** Adds each PSG file or tracker module at `start` on a new track named after it, the first one
    on `firstTrack` instead when given; skips unreadable files with an error. A module adds a track
    per AY chip (two for Turbo Sound) named after its title, else its file. The files play
    together as one tune (the chips of a Turbo Sound tune): their tempo is detected together (see
    PsgTempo::applyToEdit), and so are their tuning and key (see PsgClip::linkAsOneTune). A
    module's note table, when uZX has it, is chosen for the tune, and its rows set the tempo
    grid. Returns the clips added. */
Array<PsgClip*> importPsgFiles(te::Edit& edit, const Array<File>& files, te::TimePosition start = {},
                               te::AudioTrack* firstTrack = nullptr);

/** Calls back with the files to open: when `files` is a single PSG of a Turbo Sound set (see
    uZX::findTurboSoundSet), asks first whether to take the whole set. Not called on Cancel. */
void withTurboSoundSet(const Array<File>& files, std::function<void (const Array<File>&)> callback);

/** Browses for PSG files; one goes on the selected track, several on new tracks. */
void importPsgAsClip(te::Edit &edit, te::SelectionManager& selectionManager, bool insertAtCursor = false);
void importAudioAsClip(te::Edit& edit, te::SelectionManager& selectionManager, bool insertAtCursor = false);

void removeUnusedRenderFiles(te::Edit& edit);
File getFreezeFileForTrack(const te::AudioTrack& track);
te::AudioTrack* renderSelectedTracksToAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager);

// CSV export directory persistence
File getLastCsvExportDirectory();
void setLastCsvExportDirectory(const File& directory);

}  // namespace MoTool::Helpers