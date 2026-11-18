#include "CommandLine.h"

#include <JuceHeader.h>

namespace MoTool::Tests {

class CommandLineTests : public juce::UnitTest
{
public:
    CommandLineTests() : UnitTest("CommandLine", "MoTool") {}

    void runTest() override {
        using CommandLine::getFiles;
        const auto dir = juce::File::createTempFile("cmdline");
        dir.createDirectory();
        const auto one = dir.getChildFile("song_1.psg"), two = dir.getChildFile("song_2.psg");
        const auto spaced = dir.getChildFile("my song.psg");
        for (auto f : {one, two, spaced})
            f.create();

        beginTest("Nothing on the command line names no files");
        expect(getFiles({}).isEmpty());
        expect(getFiles("  ").isEmpty());

        beginTest("One path, quoted or not, even with spaces");
        expect(getFiles(one.getFullPathName()) == juce::Array<juce::File>{one});
        expect(getFiles(spaced.getFullPathName()) == juce::Array<juce::File>{spaced});
        expect(getFiles(spaced.getFullPathName().quoted()) == juce::Array<juce::File>{spaced});

        beginTest("Several paths, as Finder hands over a selection: each one, in order");
        {
            const auto line = one.getFullPathName() + " " + spaced.getFullPathName().quoted() + " "
                              + two.getFullPathName();
            expect(getFiles(line) == juce::Array<juce::File>{one, spaced, two});
        }

        beginTest("A path given twice is one file");
        expect(getFiles(one.getFullPathName() + " " + one.getFullPathName()) == juce::Array<juce::File>{one});

        beginTest("Options are left out; missing files are named, for the caller to report");
        {
            const auto missing = dir.getChildFile("gone.psg");
            const auto files = getFiles("-NSDocumentRevisionsDebugMode " + one.getFullPathName() + " "
                                        + missing.getFullPathName());
            expect(files.contains(one));
            expect(files.contains(missing));
            expect(! missing.existsAsFile());
        }

        beginTest("A relative path is taken from the working directory");
        {
            const auto cwd = juce::File::getCurrentWorkingDirectory();
            dir.setAsCurrentWorkingDirectory();
            // the working directory is the real path: on macOS the temp folder is a link to /private/var
            const auto files = getFiles("song_2.psg");
            expect(files.size() == 1 && files[0].getFileName() == "song_2.psg" && files[0].existsAsFile());
            cwd.setAsCurrentWorkingDirectory();
        }

        dir.deleteRecursively();
    }
};

static CommandLineTests commandLineTests;

}  // namespace MoTool::Tests
