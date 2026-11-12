#include <JuceHeader.h>

#include "../models/Behavior.h"
#include "../models/EditUtilities.h"
#include "../models/PsgClip.h"
#include "../formats/psg/PsgFile.h"
#include "../plugins/uZX/aychip/AYPlugin.h"
#include "../plugins/uZX/notes_to_psg/NotesToPsgPlugin.h"
#include "FileOps.h"
#include "Helpers.h"
#include "TestHelpers.h"

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

class RenderTrackTest : public juce::UnitTest
{
public:
    RenderTrackTest() : UnitTest("RenderTrack", "MoTool") {}

    void runTest() override {
        struct TestStorage : tracktion::PropertyStorage {
            explicit TestStorage(const File& folder) : PropertyStorage("RenderTrackTest"), root(folder) {}
            File getAppPrefsFolder() override { return root; }
            File root;
        };

        // The app's behaviour, which makes PSG clips, with no devices and a clip limit to set
        struct TestEngine : ExtEngineBehaviour {
            bool autoInitialiseDeviceManager() override { return false; }
            bool addSystemAudioIODeviceTypes() override { return false; }
            tracktion::EditLimits getEditLimits() override {
                auto limits = tracktion::EngineBehaviour::getEditLimits();
                limits.maxClipsInTrack = clipLimit;
                return limits;
            }
            int clipLimit = 1500;
        };

        struct TestUI : tracktion::UIBehaviour {
            void showWarningMessage(const String& message) override { warning = message; }
            void runTaskWithProgressBar(tracktion::ThreadPoolJobWithProgress& job) override {
                if (cancel) {
                    auto& task = dynamic_cast<tracktion::Renderer::RenderTask&>(job);
                    job.runJob();
                    canCancel = job.canCancel();
                    partialFileWritten = task.params.destFile.existsAsFile();
                } else {
                    while (job.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain) {}
                }
                job.signalJobShouldExit();
            }
            String warning;
            bool cancel = false;
            bool canCancel = false;
            bool partialFileWritten = false;
        };

        beginTest("A failed output directory leaves the project and existing file intact");
        const juce::TemporaryFile workspace(String{});
        auto root = workspace.getFile();
        expect(root.createDirectory().wasOk());
        {
            auto ui = std::make_unique<TestUI>();
            auto behaviour = std::make_unique<TestEngine>();
            auto& testUI = *ui;
            auto& testEngine = *behaviour;
            tracktion::Engine engine(std::make_unique<TestStorage>(root), std::move(ui), std::move(behaviour));
            tracktion::HostedAudioDeviceInterface::Parameters deviceParams;
            deviceParams.useMidiDevices = false;
            engine.getDeviceManager().getHostedAudioDeviceInterface().initialise(deviceParams);

            const auto editFile = root.getChildFile("RenderTrack.uzx");
            expect(editFile.create().wasOk());
            auto edit = tracktion::Edit::createSingleTrackEdit(engine);
            edit->editFileRetriever = [editFile] { return editFile; };
            auto* source = tracktion::getAudioTracks(*edit).getFirst();
            expect(source->insertMIDIClip("Source", {0_tp, 0.15_tp}, nullptr) != nullptr);
            tracktion::SelectionManager selection(engine);
            selection.selectOnly(source);
            const auto renders = EditFileOps::getRendersDirectory(*edit);
            const auto trackCount = tracktion::getAudioTracks(*edit).size();
            const auto checkUnchanged = [&](bool muted) {
                expectEquals(tracktion::getAudioTracks(*edit).size(), trackCount);
                expect(selection.getSelectedObject(0) == source);
                expectEquals(int(source->isMuted(false)), int(muted));
                expect(renders.findChildFiles(File::findFiles, false).isEmpty());
            };

            source->setMute(true);
            expect(renders.replaceWithText("existing"));
            expect(Helpers::renderSelectedTracksToAudioTrack(*edit, selection) == nullptr);
            checkUnchanged(true);
            expect(testUI.warning.isNotEmpty());
            expectEquals(renders.loadFileAsString(), String("existing"));
            expect(renders.deleteFile());

            beginTest("Cancellation cleans up the partial file without adding a track");
            source->setMute(false);
            testUI.warning.clear();
            testUI.cancel = true;
            expect(Helpers::renderSelectedTracksToAudioTrack(*edit, selection) == nullptr);
            expect(testUI.canCancel && testUI.partialFileWritten);
            checkUnchanged(false);
            expect(testUI.warning.isEmpty());
            testUI.cancel = false;

            beginTest("Clip insertion failure rolls back only the new track and its file");
            source->setMute(true);
            testEngine.clipLimit = 0;
            edit->getUndoManager().clearUndoHistory();
            expect(Helpers::renderSelectedTracksToAudioTrack(*edit, selection) == nullptr);
            checkUnchanged(true);
            expect(!edit->getUndoManager().canUndo() && !edit->getUndoManager().canRedo());
            expect(testUI.warning.contains("add the rendered audio"));
            testEngine.clipLimit = 1500;

            for (bool initiallyMuted : {false, true}) {
                beginTest(initiallyMuted ? "Rendering a muted track commits the clip"
                                         : "Rendering commits the clip and mutes the source");
                source->setMute(initiallyMuted);
                edit->getUndoManager().clearUndoHistory();
                auto* rendered = Helpers::renderSelectedTracksToAudioTrack(*edit, selection);
                expect(rendered != nullptr);
                if (rendered != nullptr) {
                    expectEquals(tracktion::getAudioTracks(*edit).size(), trackCount + 1);
                    expect(selection.getSelectedObject(0) == rendered);
                    expect(source->isMuted(false));
                    expectEquals(rendered->getClips().size(), 1);
                    expectEquals(renders.findChildFiles(File::findFiles, false, "*.wav").size(), 1);
                    TestHelpers::flushMessageQueue();
                    expect(edit->getUndoManager().canUndo());
                    edit->undo();
                    expectEquals(tracktion::getAudioTracks(*edit).size(), trackCount);
                    expectEquals(int(source->isMuted(false)), int(initiallyMuted));
                    Helpers::removeUnusedRenderFiles(*edit);
                }
                selection.selectOnly(source);
            }

            beginTest("A render to PSG needs MIDI to PSG on the selected track");
            source->setMute(false);
            testUI.warning.clear();
            expect(!Helpers::canRenderSelectedTrackToPsg(selection));
            expect(Helpers::renderSelectedTrackToPsgTrack(*edit, selection) == nullptr);
            checkUnchanged(false);
            expect(testUI.warning.contains("MIDI to PSG"));

            beginTest("A render to PSG keeps what the chip is told: a file, and a clip on a new track with a chip");
            engine.getPluginManager().createBuiltInType<uZX::NotesToPsgPlugin>();
            engine.getPluginManager().createBuiltInType<uZX::AYChipPlugin>();
            source->pluginList.insertPlugin(edit->getPluginCache().createNewPlugin(uZX::NotesToPsgPlugin::xmlTypeName, {}),
                                            0, nullptr);
            // A4 for a tenth of a second, in a clip of 0.15 s
            dynamic_cast<tracktion::MidiClip*>(source->getClips()[0])
                ->getSequence()
                .addNote(69, tracktion::BeatPosition(), tracktion::BeatDuration::fromBeats(0.2), 127, 0, nullptr);
            edit->getUndoManager().clearUndoHistory();
            testUI.warning.clear();
            expect(Helpers::canRenderSelectedTrackToPsg(selection));

            testEngine.clipLimit = 0;
            expect(Helpers::renderSelectedTrackToPsgTrack(*edit, selection) == nullptr,
                   "a track that takes no clip is rolled back");
            expectEquals(tracktion::getAudioTracks(*edit).size(), trackCount);
            expect(!source->isMuted(false) && selection.getSelectedObject(0) == source);
            expect(testUI.warning.contains("Renders folder"), "and the warning says where the file is");
            for (const auto& left : renders.findChildFiles(File::findFiles, false))
                expect(left.deleteFile());
            testEngine.clipLimit = 1500;
            edit->getUndoManager().clearUndoHistory();
            testUI.warning.clear();

            auto* rendered = Helpers::renderSelectedTrackToPsgTrack(*edit, selection);
            expect(rendered != nullptr, testUI.warning);
            if (rendered != nullptr) {
                expectEquals(tracktion::getAudioTracks(*edit).size(), trackCount + 1);
                expect(selection.getSelectedObject(0) == rendered);
                expect(source->isMuted(false));
                expectEquals(rendered->getClips().size(), 1);
                expect(dynamic_cast<PsgClip*>(rendered->getClips()[0]) != nullptr, "a PSG clip");
                expect(rendered->pluginList.findFirstPluginOfType<uZX::AYChipPlugin>() != nullptr, "and a chip to play it");

                const auto files = renders.findChildFiles(File::findFiles, false, "*.psg");
                expectEquals(files.size(), 1);
                expectEquals(renders.findChildFiles(File::findFiles, false).size(), 1, "the audio it played is not kept");
                if (files.size() == 1) {
                    uZX::PsgFile file(files[0]);
                    expect(file.ensureRead());
                    const auto& frames = file.getData().frames;
                    expect(frames.size() >= 6, "0.15 s at 50 frames a second, or the frame the note ends in");
                    if (frames.size() >= 6) {
                        expectEquals(int(frames[0].getVolume(0)), 15, "the note starts at full volume");
                        expectEquals(int(frames[0].getTonePeriod(0)), 252, "A4 at the chip's default clock");
                        // Tracktion ends a note a hair early, so the frame before the tenth of a second it lasts
                        size_t end = 1;
                        while (end < frames.size() && !frames[end].isSet(uZX::PsgRegType::VolumeA))
                            ++end;
                        expect(end == 4 || end == 5, "and ends a tenth of a second on: frame " + String(int(end)));
                        expect(end < frames.size() && frames[end].getVolume(0) == 0);
                    }
                    expect(files[0].deleteFile());
                }
                TestHelpers::flushMessageQueue();
                expect(edit->getUndoManager().canUndo());
                edit->undo();
                expectEquals(tracktion::getAudioTracks(*edit).size(), trackCount);
                expect(!source->isMuted(false));
            }

            beginTest("The MPE chord example renders to a PSG of the chord it plays");
            {
                // tests/fixtures/MPE chord.uzx, see scripts/make_mpe_fixture.py: a cluster that opens, over two
                // seconds of four, into C3, B3 and G4 with the envelope at E3 as a sawtooth
                auto chord = tracktion::loadEditFromFile(engine, File(UZX_FIXTURES_DIR).getChildFile("MPE chord.uzx"));
                const auto chordFile = root.getChildFile("MPE chord.uzx");
                chord->editFileRetriever = [chordFile] { return chordFile; };
                expect(chordFile.create().wasOk());
                tracktion::SelectionManager chordSelection(engine);
                chordSelection.selectOnly(tracktion::getAudioTracks(*chord).getFirst());
                testUI.warning.clear();
                expect(Helpers::renderSelectedTrackToPsgTrack(*chord, chordSelection) != nullptr, testUI.warning);

                const auto files = EditFileOps::getRendersDirectory(*chord).findChildFiles(File::findFiles, false, "*.psg");
                expectEquals(files.size(), 1);
                if (files.size() == 1) {
                    uZX::PsgFile file(files[0]);
                    expect(file.ensureRead());
                    // What a chip that played the file holds after each frame
                    std::vector<uZX::PsgRegsFrame> held;
                    uZX::PsgRegsFrame chip;
                    int shapeWrites = 0;
                    for (const auto& frame : file.getData().frames) {
                        chip.update(frame);
                        held.push_back(chip);
                        shapeWrites += frame.hasEnvelopeShapeSet() ? 1 : 0;
                    }
                    expect(held.size() >= 200, "four seconds at 50 frames a second");
                    if (held.size() >= 200) {
                        const auto periods = [](const uZX::PsgRegsFrame& regs) {
                            juce::Array<int> all;
                            for (size_t n = 0; n < 3; ++n)
                                all.addUsingDefaultSort(int(regs.getTonePeriod(n)));
                            return all;
                        };
                        // 1773400 / 16 / the frequency of G4, B3 and C3; / 256 / that of E3 for the envelope
                        expect(periods(held[150]) == juce::Array<int> {283, 449, 847}, "in tune from two seconds on");
                        expectEquals(int(held[150].getEnvelopePeriod()), 42);
                        expect(periods(held[0]) != periods(held[150]) && periods(held[50]) != periods(held[150]),
                               "and gliding there before");
                        expectEquals(int(held[150].getEnvelopeShape()), 12);
                        expectEquals(shapeWrites, 1, "the shape is written once: the envelope runs on");
                        for (size_t n = 0; n < 3; ++n) {
                            expect(held[150].getVolume(n) == 15 || held[150].getEnvMod(n), "every voice sounds");
                            expect(held.back().getVolume(n) == 0 && !held.back().getEnvMod(n),
                                   "and the file ends in silence");
                        }
                    }
                    // To take the file: UZX_RENDER_PSG_TO=<file.psg> uZXTests RenderTrack
                    if (const File copy(juce::SystemStats::getEnvironmentVariable("UZX_RENDER_PSG_TO", {})); copy != File())
                        expect(files[0].copyFileTo(copy));
                }
            }
        }
        expect(root.deleteRecursively());
    }
};

static RenderTrackTest renderTrackTest;

}  // namespace MoTool::Tests
