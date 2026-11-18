#include <JuceHeader.h>

#include "FileOps.h"
#include "Helpers.h"
#include "TestHelpers.h"

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

        beginTest("The Open dialog starts in a folder that exists, the nearest to the one last used");
        {
            const auto gone = root.getChildFile("moved/away");
            gone.createDirectory();
            EditFileOps::setRecentEditsDirectory(storage, gone);
            expectEquals(EditFileOps::getRecentEditsDirectory(storage).getFullPathName(), gone.getFullPathName());
            root.getChildFile("moved").deleteRecursively();
            expectEquals(EditFileOps::getRecentEditsDirectory(storage).getFullPathName(), root.getFullPathName());
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

class AutoSaveTest : public juce::UnitTest {
public:
    AutoSaveTest() : UnitTest("AutoSave", "MoTool") {}

    void runTest() override {
        auto& engine = *tracktion::Engine::getEngines()[0];
        auto& storage = engine.getPropertyStorage();
        auto root = File::getSpecialLocation(File::tempDirectory).getNonexistentChildFile("uzx-autosave", "", false);
        root.createDirectory();

        // A user's project of a tune, as someone shared it
        const auto tune = root.getChildFile("tune.psg");
        const std::vector<uint8> bytes {'P', 'S', 'G', 0x1A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                        0xFF, 0x08, 0x0F, 0xFF, 0x08, 0x00};
        tune.replaceWithData(bytes.data(), bytes.size());
        const auto shared = root.getChildFile("shared.uzx");
        {
            auto edit = tracktion::createEmptyEdit(engine, shared);
            Helpers::importPsgFiles(*edit, {tune});
            EditFileOps::stampEditFormatVersion(*edit);
            EditFileOps::saveEdit(*edit, false, true, false);
        }

        const auto anHourAgo = [] { return Time::getCurrentTime() - RelativeTime::hours(1); };
        const auto writtenNow = [](const File& f) {
            return f.getLastModificationTime() > Time::getCurrentTime() - RelativeTime::minutes(1);
        };
        // As BaseController::setEdit opens it, and long enough for the engine's own change timers to run
        const auto open = [&](const File& f) {
            f.setLastModificationTime(anHourAgo());
            auto edit = tracktion::loadEditFromFile(engine, f);
            edit->resetChangedStatus();
            EditFileOps::autoSaveEdit(*edit);
            TestHelpers::flushMessageQueue();
            MessageManager::getInstance()->runDispatchLoopUntil(1000);
            return edit;
        };

        beginTest("A user's file that is only opened and played is not written, but is the most recent");
        {
            EditFileOps::clearRecentEdits(storage);
            auto edit = open(shared);
            int clips = 0;
            for (auto* track : tracktion::getAudioTracks(*edit))
                clips += track->getClips().size();
            expectEquals(clips, 1, "the tune's clip");
            expect(! edit->hasChangedSinceSaved());
            EditFileOps::autoSaveEdit(*edit);  // as on quit
            expect(! writtenNow(shared));
            expectEquals(EditFileOps::getRecentEdits(storage)[0], shared.getFullPathName());
        }

        beginTest("A user's file is written once it changes");
        {
            auto edit = open(shared);
            tracktion::getAudioTracks(*edit).getFirst()->setMute(true);
            EditFileOps::autoSaveEdit(*edit);
            expect(writtenNow(shared));
        }

        beginTest("A working edit is written every time");
        {
            const auto working = EditFileOps::getUnnamedEditFile(storage);
            EditFileOps::autoSaveEdit(*tracktion::createEmptyEdit(engine, working));
            expect(working.existsAsFile(), "a new one is on disk at once");
            open(working);
            expect(writtenNow(working), "the copies opened last are the ones pruning keeps");
            working.deleteFile();
        }

        root.deleteRecursively();
    }
};

static AutoSaveTest autoSaveTest;

}  // namespace MoTool::Tests
