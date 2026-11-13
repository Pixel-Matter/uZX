#include <JuceHeader.h>

#include "FileOps.h"

namespace MoTool::Tests {

class RecentEditsTest : public juce::UnitTest {
public:
    RecentEditsTest() : UnitTest("RecentEdits", "MoTool") {}

    void runTest() override {
        auto& storage = tracktion::Engine::getEngines()[0]->getPropertyStorage();
        EditFileOps::clearRecentEdits(storage);

        auto root = File::getSpecialLocation(File::tempDirectory).getNonexistentChildFile("uzx-recent", "", false);
        root.createDirectory();
        Array<File> files;
        for (int i = 0; i < 10; ++i) {
            files.add(root.getChildFile("edit" + String(i) + ".uzx"));
            files.getReference(i).create();
        }

        beginTest("A full list keeps the newest edits, newest first");
        {
            for (auto& f : files)
                EditFileOps::addToRecentEdits(storage, f);

            const auto recent = EditFileOps::getRecentEdits(storage);
            expectEquals(recent.size(), 8);
            for (int i = 0; i < recent.size(); ++i)
                expectEquals(recent[i], files[9 - i].getFullPathName());
            expectEquals(EditFileOps::getStartupEditFile(storage).getFullPathName(), files[9].getFullPathName());
        }

        beginTest("Missing files don't take slots");
        {
            files[9].deleteFile();
            const auto recent = EditFileOps::getRecentEdits(storage);
            expectEquals(recent.size(), 8);
            expectEquals(recent[0], files[8].getFullPathName());
        }

        beginTest("Re-adding moves an edit to the front without duplicates");
        {
            EditFileOps::addToRecentEdits(storage, files[5]);
            const auto recent = EditFileOps::getRecentEdits(storage);
            expectEquals(recent.size(), 8);
            expectEquals(recent[0], files[5].getFullPathName());
            expectEquals(recent[1], files[8].getFullPathName());
            expectEquals(recent.indexOf(files[5].getFullPathName(), false, 1), -1);
        }

        beginTest("Clearing empties the list read from the same storage");
        {
            EditFileOps::clearRecentEdits(storage);
            expect(EditFileOps::getRecentEdits(storage).isEmpty());
        }

        root.deleteRecursively();
    }
};

static RecentEditsTest recentEditsTest;

class EditFormatVersionTest : public juce::UnitTest {
public:
    EditFormatVersionTest() : UnitTest("EditFormatVersion", "MoTool") {}

    void runTest() override {
        auto& engine = *tracktion::Engine::getEngines()[0];
        auto root = File::getSpecialLocation(File::tempDirectory).getNonexistentChildFile("uzx-format", "", false);
        root.createDirectory();

        beginTest("A stamped edit saves its format version");
        {
            const auto file = root.getChildFile("stamped.uzx");
            auto edit = tracktion::createEmptyEdit(engine, file);
            EditFileOps::stampEditFormatVersion(*edit);
            bool saved = false;
            tracktion::EditFileOperations(*edit).save(false, true, false, [&saved](bool ok) { saved = ok; });
            expect(saved);
            expectEquals(EditFileOps::readEditFormatVersion(file), EditFileOps::editFormatVersion);
        }

        beginTest("Files from before versioning, and non-XML ones, read as 0");
        {
            const auto old = root.getChildFile("old.uzx");
            old.replaceWithText(R"(<EDIT appVersion="uZX 0.6.0"><TRACKS/></EDIT>)");
            expectEquals(EditFileOps::readEditFormatVersion(old), 0);

            const auto junk = root.getChildFile("junk.uzx");
            junk.replaceWithText("not xml");
            expectEquals(EditFileOps::readEditFormatVersion(junk), 0);
        }

        beginTest("A newer file reads its own version");
        {
            const auto newer = root.getChildFile("newer.uzx");
            newer.replaceWithText(R"(<EDIT uzxFormatVersion="99"><TRACKS/></EDIT>)");
            expectEquals(EditFileOps::readEditFormatVersion(newer), 99);
        }

        root.deleteRecursively();
    }
};

static EditFormatVersionTest editFormatVersionTest;

}  // namespace MoTool::Tests
