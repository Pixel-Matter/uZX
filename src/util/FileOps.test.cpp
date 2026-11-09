#include "FileOps.h"

#include <JuceHeader.h>

namespace MoTool::Tests {

namespace te = tracktion;

class RecentEditsTests : public juce::UnitTest
{
public:
    RecentEditsTests() : UnitTest("RecentEdits", "MoTool") {}

    void runTest() override {
        auto& storage = te::Engine::getEngines()[0]->getPropertyStorage();
        auto dir = File::createTempFile("RecentEditsTests");
        dir.createDirectory();

        Array<File> files;
        for (int i = 0; i < 10; ++i) {
            files.add(dir.getChildFile(String(i) + ".uzx"));
            files.getLast().create();
        }

        EditFileOps::clearRecentEdits(storage);

        beginTest("Newest first, oldest dropped beyond the limit");
        {
            for (auto& f : files)
                EditFileOps::addToRecentEdits(storage, f);

            auto recent = EditFileOps::getRecentEdits(storage);
            expectEquals(recent.size(), 8);
            expectEquals(recent[0], files[9].getFullPathName());
            expectEquals(recent[7], files[2].getFullPathName());
            expectEquals(EditFileOps::getStartupEditFile(storage).getFullPathName(), files[9].getFullPathName());
        }

        beginTest("Re-adding moves a file to the front without duplicates");
        {
            EditFileOps::addToRecentEdits(storage, files[5]);

            auto recent = EditFileOps::getRecentEdits(storage);
            expectEquals(recent.size(), 8);
            expectEquals(recent[0], files[5].getFullPathName());
            expectEquals(recent[1], files[9].getFullPathName());
            expectEquals(recent.indexOf(files[5].getFullPathName(), false, 1), -1);
        }

        beginTest("Missing files are skipped");
        {
            files[5].deleteFile();

            auto recent = EditFileOps::getRecentEdits(storage);
            expect(!recent.contains(files[5].getFullPathName()));
            expectEquals(recent[0], files[9].getFullPathName());
        }

        beginTest("Clearing empties the list read from the same storage");
        {
            EditFileOps::clearRecentEdits(storage);
            expect(EditFileOps::getRecentEdits(storage).isEmpty());
        }

        dir.deleteRecursively();
    }
};

static RecentEditsTests recentEditsTests;

}  // namespace MoTool::Tests
