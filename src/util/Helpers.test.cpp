#include <JuceHeader.h>

#include "../models/EditUtilities.h"
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

        beginTest("A tracker module comes in with its title, note table and row grid");
        {
            const File samples(UZX_ZXTUNE_REGRESSION_DIR);
            auto edit = te::Edit::createSingleTrackEdit(engine);
            const auto clips = Helpers::importPsgFiles(*edit, {samples.getChildFile("hypergy.pt3")}, {},
                                                       te::getAudioTracks(*edit).getFirst());
            expectEquals(clips.size(), 1);
            if (clips.size() == 1) {
                expectEquals(clips[0]->getTrack()->getName(), String("hypergy #2"), "the module's title");
                const auto& tuning = clips[0]->getTuning();
                expect(tuning.tableChosen && tuning.table.has_value(), "the module's table is chosen");
                if (tuning.table)
                    expectEquals(String(tuning.table->getLabel().data()),
                                 String(BuiltinTuningType(BuiltinTuningEnum::CustomPT_2_ASM).getLabel().data()));
                const auto framesPerBeat = juce::roundToInt(clips[0]->getPsg().getFramesPerBeat());
                expect(framesPerBeat > 0 && framesPerBeat % 5 == 0, "beats of whole speed-5 rows");
                expect(Helpers::getGridSubdivisionPattern(*edit, framesPerBeat)
                           == std::vector<int>(4, framesPerBeat / 4), "rows as the grid");
            }
        }

        beginTest("A Turbo Sound module lands on two tracks as one tune");
        {
            const File samples(UZX_ZXTUNE_REGRESSION_DIR);
            MemoryBlock first, second;
            samples.getChildFile("hypergy.pt3").loadFileAsData(first);
            samples.getChildFile(";-).pt3").loadFileAsData(second);
            MemoryOutputStream pair;
            pair << first << second;
            pair.write("PT3!", 4);
            pair.writeShort((short) first.getSize());
            pair.write("PT3!", 4);
            pair.writeShort((short) second.getSize());
            pair.write("02TS", 4);
            auto file = root.getChildFile("pair.pt3");
            file.replaceWithData(pair.getData(), pair.getDataSize());

            auto edit = te::Edit::createSingleTrackEdit(engine);
            const auto clips = Helpers::importPsgFiles(*edit, {file}, {}, te::getAudioTracks(*edit).getFirst());
            expectEquals(clips.size(), 2);
            if (clips.size() == 2) {
                expectEquals(clips[0]->getTrack()->getName(), String("hypergy #2/:-) 1"));
                expectEquals(clips[1]->getTrack()->getName(), String("hypergy #2/:-) 2"));
                expectEquals(clips[0]->getTuneClips().size(), 2, "one tune");
                expect(clips[1]->getTuning().tableChosen, "the table applies to both chips");
            }
        }

        beginTest("A Vortex Tracker module brings its chip clock and frame rate");
        {
            MemoryBlock text;
            File(UZX_ZXTUNE_REGRESSION_DIR).getChildFile("little+girl+with+big+yellow+bow.txt").loadFileAsData(text);
            const auto vt2 = text.toString().replace("Speed=", "ChipFreq=1750000\nIntFreq=48828\nSpeed=");
            auto file = root.getChildFile("girl.vt2");
            file.replaceWithText(vt2);

            auto edit = te::Edit::createSingleTrackEdit(engine);
            const auto clips = Helpers::importPsgFiles(*edit, {file}, {}, te::getAudioTracks(*edit).getFirst());
            expectEquals(clips.size(), 2, "a Turbo Sound module");
            for (auto* clip : clips) {
                auto& chip = Helpers::ensureAYPluginOnTrack(*clip->getAudioTrack());
                expectWithinAbsoluteError(chip.staticParams.chipClock.getStoredValue(), 1.75, 1e-9, "Pentagon clock");
            }
            expectWithinAbsoluteError(Helpers::getProjectFps(*edit), 48.828, 1e-9, "Pentagon interrupt rate");
        }

        root.deleteRecursively();
    }
};

static ImportPsgFilesTest importPsgFilesTest;

}  // namespace MoTool::Tests
