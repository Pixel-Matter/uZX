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

class TuneEditsTest : public juce::UnitTest {
public:
    TuneEditsTest() : UnitTest("TuneEdits", "MoTool") {}

    void runTest() override {
        auto& storage = tracktion::Engine::getEngines()[0]->getPropertyStorage();
        auto root = File::getSpecialLocation(File::tempDirectory).getNonexistentChildFile("uzx-tunes", "", false);
        root.createDirectory();
        const auto a = root.getChildFile("a.psg"), b = root.getChildFile("b.psg");
        a.create();
        b.create();

        beginTest("Each set of tune files has its own copy, named after the tune, next to the settings");
        {
            const auto ab = EditFileOps::getTuneEditFile(storage, { a, b }, "Song");
            expectEquals(ab.getFileName(), String("Song.uzx"));
            expect(ab.isAChildOf(EditFileOps::getWorkingEditsLocation(storage)));
            expect(ab.isAChildOf(storage.getPropertiesFile().getFile().getParentDirectory()), "kept with the settings");
            expect(ab == EditFileOps::getTuneEditFile(storage, { b, a }, "Song"), "the order of the files doesn't matter");
            expect(ab.getParentDirectory() != EditFileOps::getTuneEditFile(storage, { a }, "Song").getParentDirectory());
        }

        beginTest("A copy is current until a tune file changes, however often the copy is saved");
        {
            auto& engine = *tracktion::Engine::getEngines()[0];
            const auto copy = EditFileOps::getTuneEditFile(storage, { a }, "a");
            expect(! EditFileOps::isTuneEditCurrent(copy, { a }), "no copy yet");
            copy.getParentDirectory().createDirectory();
            auto edit = tracktion::createEmptyEdit(engine, copy);
            EditFileOps::stampTuneFiles(*edit, { a });
            const auto save = [&edit] {
                tracktion::EditFileOperations(*edit).save(false, true, false, [](bool) {});
            };
            save();
            expect(EditFileOps::isTuneEditCurrent(copy, { a }));
            a.setLastModificationTime(a.getLastModificationTime() + RelativeTime::seconds(10));
            save();
            expect(! EditFileOps::isTuneEditCurrent(copy, { a }), "the tune changed after the copy was made");
            edit.reset();
            copy.getParentDirectory().deleteRecursively();
        }

        beginTest("Pruning keeps the most recently saved copies");
        {
            const auto location = EditFileOps::getWorkingEditsLocation(storage);
            const auto kept = location.getNumberOfChildFiles(File::findDirectories);
            Array<File> copies;
            for (int i = 0; i < 3; ++i) {
                const auto tune = root.getChildFile("t" + String(i) + ".psg");
                tune.create();
                auto copy = EditFileOps::getTuneEditFile(storage, { tune }, "t");
                copy.getParentDirectory().createDirectory();
                copy.create();
                copy.setLastModificationTime(Time::getCurrentTime() + RelativeTime::hours(i + 1));
                copies.add(copy);
            }
            EditFileOps::pruneTuneEdits(storage, kept + 2);
            expect(! copies[0].exists());
            expect(copies[1].exists() && copies[2].exists());
            for (auto& c : copies)
                c.getParentDirectory().deleteRecursively();
        }

        root.deleteRecursively();
    }
};

static TuneEditsTest tuneEditsTest;

}  // namespace MoTool::Tests
