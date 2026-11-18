#include "CommandLine.h"

namespace MoTool::CommandLine {

static juce::File toFile(const juce::String& path) {
    return juce::File::isAbsolutePath(path) ? juce::File(path)
                                            : juce::File::getCurrentWorkingDirectory().getChildFile(path);
}

juce::Array<juce::File> getFiles(const juce::String& commandLine) {
    const auto line = commandLine.trim();
    if (line.isEmpty())
        return {};
    // a file whose name has spaces, passed unquoted
    if (const auto whole = toFile(line); whole.existsAsFile())
        return {whole};

    juce::Array<juce::File> files;
    for (auto token : juce::StringArray::fromTokens(line, true)) {
        token = token.unquoted().replace("\\\"", "\"");  // macOS escapes quotes inside a path
        if (token.isNotEmpty() && ! token.startsWithChar('-'))
            files.addIfNotAlreadyThere(toFile(token));
    }
    return files;
}

}  // namespace MoTool::CommandLine
