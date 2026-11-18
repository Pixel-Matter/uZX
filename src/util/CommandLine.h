#pragma once

#include <JuceHeader.h>

namespace MoTool::CommandLine {

/** The files a command line names, in order, existing or not. Paths with spaces come quoted, as JUCE
    passes the files Finder opens and as it joins the arguments on Linux and Windows:
    `"/a b/x.psg" /c/y.psg`. A line that is one existing path, unquoted, is that file. A relative path is
    taken from the working directory. Options, such as macOS's `-NSDocumentRevisionsDebugMode`, are
    left out; what follows them is not, so a caller keeps only the files it can open. */
juce::Array<juce::File> getFiles(const juce::String& commandLine);

}  // namespace MoTool::CommandLine
