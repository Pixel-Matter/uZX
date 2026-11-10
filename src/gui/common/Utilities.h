#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool::Helpers {

void browseForFiles(te::Engine& engine, const String& name, const String& category, const String& wildcard, bool multiple, std::function<void (const Array<File>&)> filesChosenCallback);
void browseForFile(te::Engine& engine, const String& name, const String& category, const String& wildcard, std::function<void (const File&)> fileChosenCallback);
void browseForAudioFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback);
void browseForMidiFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback);
/** Lets several files be picked; calls back with the existing ones, empty if cancelled. */
void browseForPSGFiles(te::Engine& engine, std::function<void (const Array<File>&)> filesChosenCallback);

/** Display-mode picker for the timeline ruler/grid; the frame rate stays the project fps. */
juce::PopupMenu buildTimecodeFormatMenu(te::Edit& edit);

}  // namespace MoTool::Helpers
