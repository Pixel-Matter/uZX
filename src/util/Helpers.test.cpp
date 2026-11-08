#include <JuceHeader.h>

#include "../models/PsgClip.h"
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "Helpers.h"

namespace MoTool::Tests {

class ImportPsgFilesTest : public juce::UnitTest {
public:
    ImportPsgFilesTest() : UnitTest("ImportPsgFiles", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        auto root = File::getSpecialLocation(File::tempDirectory).getNonexistentChildFile("uzx-import", "", false);
        root.createDirectory();
        Array<File> files;
        for (auto name : {"tune_1.psg", "tune_2.psg"}) {
            const std::vector<uint8> bytes {'P', 'S', 'G', 0x1A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                            0xFF, 0x08, 0x0F, 0xFF, 0x08, 0x00};
            auto file = root.getChildFile(name);
            file.replaceWithData(bytes.data(), bytes.size());
            files.add(file);
        }

        beginTest("Each file lands on a new track after the existing ones");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            const auto clips = Helpers::importPsgFiles(*edit, files);
            expectEquals(clips.size(), 2);
            const auto tracks = te::getAudioTracks(*edit);
            expectEquals(tracks.size(), 3);
            if (tracks.size() == 3 && clips.size() == 2) {
                for (int i = 0; i < 2; ++i) {
                    auto* track = tracks[i + 1];
                    expectEquals(track->getName(), files[i].getFileNameWithoutExtension());
                    expect(track->pluginList.findFirstPluginOfType<uZX::AYChipPlugin>() != nullptr);
                    expectEquals(clips[i]->getPosition().getStart().inSeconds(), 0.0);
                    expect(clips[i]->getTrack() == track);
                }
            }
        }

        beginTest("The first file goes on the given track");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            auto* first = te::getAudioTracks(*edit).getFirst();
            const auto clips = Helpers::importPsgFiles(*edit, files, {}, first);
            expectEquals(clips.size(), 2);
            expectEquals(te::getAudioTracks(*edit).size(), 2);
            if (clips.size() == 2) {
                expect(clips[0]->getTrack() == first);
                expect(clips[1]->getTrack() != first);
            }
        }

        root.deleteRecursively();
    }
};

static ImportPsgFilesTest importPsgFilesTest;

}  // namespace MoTool::Tests
