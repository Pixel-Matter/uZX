#include <JuceHeader.h>

#include "../controllers/PropertyStorage.h"
#include "FileOps.h"

namespace MoTool::Tests {

class RecentEditsTest : public juce::UnitTest {
public:
    RecentEditsTest() : UnitTest("RecentEdits", "MoTool") {}

    void runTest() override {
        auto& storage = dynamic_cast<PropertyStorage&>(tracktion::Engine::getEngines()[0]->getPropertyStorage());
        const auto saved = storage.getCustomProperty("recentEdits");
        storage.setCustomProperty("recentEdits", "");

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
                EditFileOps::addToRecentEdits(f);

            const auto recent = EditFileOps::getRecentEdits();
            expectEquals(recent.size(), 8);
            for (int i = 0; i < recent.size(); ++i)
                expectEquals(recent[i], files[9 - i].getFullPathName());
        }

        beginTest("Missing files don't take slots");
        {
            files[9].deleteFile();
            const auto recent = EditFileOps::getRecentEdits();
            expectEquals(recent.size(), 8);
            expectEquals(recent[0], files[8].getFullPathName());
        }

        root.deleteRecursively();
        storage.setCustomProperty("recentEdits", saved);
    }
};

static RecentEditsTest recentEditsTest;

}  // namespace MoTool::Tests
