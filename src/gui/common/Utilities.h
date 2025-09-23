#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

namespace MoTool::Helpers {

void browseForFile(te::Engine& engine, const String& name, const String& category, const String& wildcard, std::function<void (const File&)> fileChosenCallback);
void browseForAudioFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback);
void browseForMidiFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback);
void browseForPSGFile(te::Engine& engine, std::function<void (const File&)> fileChosenCallback);

}  // namespace MoTool::Helpers
