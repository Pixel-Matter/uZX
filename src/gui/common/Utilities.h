#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool::Helpers {

void browseForFile(te::Engine& engine, const String& name, const String& wildcard, std::function<void (const File&)> fileChosenCallback);
void browseForAudioFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback);
void browseForMidiFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback);
void browseForPSGFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback);

te::TimeRange getEffectiveClipsTimeRange(te::Edit& edit);

te::AudioTrack* addAudioTrackAfter(te::Edit& edit, te::Track* track);
te::AudioTrack* getSelectedOrInsertAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager, String name = {});
te::AudioTrack* addAndSelectAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager);

// te::AutomationTrack* addAutomationTrackAfter(te::Edit& edit, te::Track* track);
// te::AutomationTrack* addAndSelectAutomationTrack(te::Edit& edit, te::SelectionManager& selectionManager);

void importPsgAsClip(te::Edit &edit, te::SelectionManager& selectionManager, bool insertAtCursor = false);

File getRendersDirectory(te::Edit& edit);
void removeUnusedRenderFiles(te::Edit& edit);
File getFreezeFileForTrack(const te::AudioTrack& track);
te::AudioTrack* renderSelectedTracksToAudioTrack(te::Edit& edit, te::SelectionManager& selectionManager);

// inline juce::String formatTimecodeDisplay(const TempoSequence& tempo, const TimePosition time, bool isRelative) const;

}  // namespace MoTool::Helpers
