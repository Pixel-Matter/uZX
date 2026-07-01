#include <JuceHeader.h>
#include "controllers/EditState.h"
#include "models/EditUtilities.h"
#include "formats/psg/PsgData.h"
#include "PsgClip.h"
#include "PsgList.h"
#include "PsgMidi.h"
#include "PsgParameter.h"

namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace tracktion;
using namespace juce;
using namespace MoTool::uZX;

//==============================================================================
class PsgListAccumulatedStateTests : public UnitTest {
public:
    PsgListAccumulatedStateTests() : UnitTest("PsgListAcc", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("Each frame stores accumulated state in values array");
        {
            // Create PSG data with sparse changes
            PsgData data {
                {
                    {{PsgRegType::VolumeA, 10}, {PsgRegType::TonePeriodFineA, 100}},  // F0
                    {},                                                               // F1: Empty - no changes
                    {{PsgRegType::VolumeA, 5},  {PsgRegType::VolumeB, 8}},            // F2: Change Volumes
                    {{PsgRegType::TonePeriodFineA, 200}},                             // F3: Change tone period
                },
                {50.0, 1}
            };

            auto edit = Edit::createSingleTrackEdit(engine);
            PsgList psgList;
            psgList.loadFrom(data, *edit, nullptr);

            auto& frames = psgList.getFrames();
            expectEquals(int(frames.size()), 3, "Should have 3 non-empty frames");

            {   // Frame 0: VolumeA=10, TonePeriodA=100
                auto& f = frames[0]->getData();
                // Check masks (what changed)
                expect(f.isSet(PsgParamType::VolumeA), "Frame 0: VolumeA should be set (changed)");
                expect(f.isSet(PsgParamType::TonePeriodA), "Frame 0: TonePeriodA should be set (changed)");
                expect(!f.isSet(PsgParamType::VolumeB), "Frame 0: VolumeB should NOT be set (not changed)");

                // Check accumulated values
                expectEquals(int(f.getRaw(PsgParamType::VolumeA)), 10, "Frame 0: VolumeA value=10");
                expectEquals(int(f.getRaw(PsgParamType::TonePeriodA)), 100, "Frame 0: TonePeriodA value=100");
            }

            {   // Frame 1: VolumeA=5, VolumeB=8, TonePeriodA still 100
                auto& f = frames[1]->getData();
                // Check masks
                expect(f.isSet(PsgParamType::VolumeA), "Frame 1: VolumeA should be set (changed)");
                expect(f.isSet(PsgParamType::VolumeB), "Frame 1: VolumeB should be set (changed)");
                expect(!f.isSet(PsgParamType::TonePeriodA), "Frame 1: TonePeriodA should NOT be set (not changed)");

                // Check accumulated values - TonePeriodA should persist from frame 0
                expectEquals(int(f.getRaw(PsgParamType::VolumeA)), 5, "Frame 1: VolumeA value=5");
                expectEquals(int(f.getRaw(PsgParamType::VolumeB)), 8, "Frame 1: VolumeB value=8");
                expectEquals(int(f.getRaw(PsgParamType::TonePeriodA)), 100, "Frame 1: TonePeriodA accumulated=100");
            }

            {   // Frame 2: TonePeriodA=200, VolumeA still 5, VolumeB still 8
                auto& f = frames[2]->getData();
                // Check masks
                expect(f.isSet(PsgParamType::TonePeriodA), "Frame 2: TonePeriodA should be set (changed)");
                expect(!f.isSet(PsgParamType::VolumeA), "Frame 2: VolumeA should NOT be set (not changed)");
                expect(!f.isSet(PsgParamType::VolumeB), "Frame 2: VolumeB should NOT be set (not changed)");

                // Check accumulated values
                expectEquals(int(f.getRaw(PsgParamType::TonePeriodA)), 200, "Frame 2: TonePeriodA value=200");
                expectEquals(int(f.getRaw(PsgParamType::VolumeA)), 5, "Frame 2: VolumeA accumulated=5");
                expectEquals(int(f.getRaw(PsgParamType::VolumeB)), 8, "Frame 2: VolumeB accumulated=8");
            }
        }

        beginTest("Mixer flags accumulate correctly");
        {
            PsgData data {
                {
                    // Frame 0: Mixer set (tone A on, noise B on)
                    {{PsgRegType::Mixer, 0b00101110}},  // ~(ToneA=0, NoiseB=0) = bits 0,4 clear
                    // Frame 1: Change volume only
                    {{PsgRegType::VolumeA, 10}},
                },
                {50.0, 1}
            };

            auto edit = Edit::createSingleTrackEdit(engine);
            PsgList psgList;
            psgList.loadFrom(data, *edit, nullptr);

            auto& frames = psgList.getFrames();
            expectEquals(int(frames.size()), 2, "Should have 2 frames");

            // Frame 1 should have mixer values accumulated from frame 0
            auto& f1 = frames[1]->getData();
            expect(!f1.isSet(PsgParamType::ToneIsOnA), "Frame 1: Mixer flags should NOT be set (not changed)");

            // But values should be accumulated
            expectEquals(int(f1.getRaw(PsgParamType::ToneIsOnA)), 1, "Frame 1: ToneIsOnA accumulated=1");
            expectEquals(int(f1.getRaw(PsgParamType::NoiseIsOnB)), 1, "Frame 1: NoiseIsOnB accumulated=1");
        }

        beginTest("getFrameAtIndex returns full accumulated state");
        {
            PsgData data {
                {
                    {{PsgRegType::VolumeA, 10}},  // frame 0
                    {},
                    {{PsgRegType::VolumeB, 8}},   // frame 2
                    {},
                    {{PsgRegType::VolumeC, 6}},   // frame 4
                },
                {50.0, 1}
            };

            auto edit = Edit::createSingleTrackEdit(engine);
            PsgList psgList;
            psgList.loadFrom(data, *edit, nullptr);

            auto& frames = psgList.getFrames();
            expectEquals(int(frames.size()), 3, "Should have 3 non-empty frames");

            auto* f0 = psgList.getFrameAtIndex(0);
            expect(f0 != nullptr, "getFrameAtIndex should find frame 0");
            expectEquals(int(f0->getData().getRaw(PsgParamType::VolumeA)), 10, "State at frame 0: VolumeA=10");
            expectEquals(int(f0->getData().getRaw(PsgParamType::VolumeB)), 0, "State at frame 0: VolumeB=0 (not yet set)");

            auto* f4 = psgList.getFrameAtIndex(4);
            expect(f4 != nullptr, "getFrameAtIndex should find frame 4");
            expectEquals(int(f4->getData().getRaw(PsgParamType::VolumeA)), 10, "State at frame 4: VolumeA accumulated=10");
            expectEquals(int(f4->getData().getRaw(PsgParamType::VolumeB)), 8, "State at frame 4: VolumeB=8");
            expectEquals(int(f4->getData().getRaw(PsgParamType::VolumeC)), 6, "State at frame 4: VolumeC=6");
        }

        beginTest("Accumulated state survives ValueTree round-trip (simulating file load)");
        {
            // First, create a PsgList from PsgData (which computes accumulated state)
            PsgData data {
                {
                    {{PsgRegType::VolumeA, 10}, {PsgRegType::TonePeriodFineA, 100}},
                    {},
                    {{PsgRegType::VolumeA, 5},  {PsgRegType::VolumeB, 8}},
                    {{PsgRegType::TonePeriodFineA, 200}},
                },
                {50.0, 1}
            };

            auto edit = Edit::createSingleTrackEdit(engine);
            PsgList original;
            original.loadFrom(data, *edit, nullptr);

            // Copy the ValueTree state (simulates saving/loading a file)
            auto stateCopy = original.state.createCopy();

            // Construct a new PsgList from the copied state
            PsgList loaded(stateCopy, nullptr);

            auto& frames = loaded.getFrames();
            expectEquals(int(frames.size()), 3, "Loaded: Should have 3 non-empty frames");

            {   // Frame 0: VolumeA=10, TonePeriodA=100
                auto& f = frames[0]->getData();
                expect(f.isSet(PsgParamType::VolumeA), "Loaded F0: VolumeA should be set");
                expect(f.isSet(PsgParamType::TonePeriodA), "Loaded F0: TonePeriodA should be set");
                expectEquals(int(f.getRaw(PsgParamType::VolumeA)), 10, "Loaded F0: VolumeA=10");
                expectEquals(int(f.getRaw(PsgParamType::TonePeriodA)), 100, "Loaded F0: TonePeriodA=100");
            }

            {   // Frame 1: VolumeA=5, VolumeB=8, TonePeriodA accumulated=100
                auto& f = frames[1]->getData();
                expectEquals(int(f.getRaw(PsgParamType::VolumeA)), 5, "Loaded F1: VolumeA=5");
                expectEquals(int(f.getRaw(PsgParamType::VolumeB)), 8, "Loaded F1: VolumeB=8");
                expectEquals(int(f.getRaw(PsgParamType::TonePeriodA)), 100, "Loaded F1: TonePeriodA accumulated=100");
            }

            {   // Frame 2: TonePeriodA=200, VolumeA accumulated=5, VolumeB accumulated=8
                auto& f = frames[2]->getData();
                expectEquals(int(f.getRaw(PsgParamType::TonePeriodA)), 200, "Loaded F2: TonePeriodA=200");
                expectEquals(int(f.getRaw(PsgParamType::VolumeA)), 5, "Loaded F2: VolumeA accumulated=5");
                expectEquals(int(f.getRaw(PsgParamType::VolumeB)), 8, "Loaded F2: VolumeB accumulated=8");
            }
        }

        beginTest("Accumulated state works with addFrameEvent API");
        {
            PsgList psgList;

            // Add frames manually (as if user is editing)
            PsgParamFrameData f0data {{
                {PsgParamType::VolumeA, 10},
                {PsgParamType::TonePeriodA, 100},
            }};
            psgList.addFrameEvent(0, f0data, nullptr);

            PsgParamFrameData f1data {{
                {PsgParamType::VolumeA, 5},
                {PsgParamType::VolumeB, 8},
            }};
            psgList.addFrameEvent(1, f1data, nullptr);

            PsgParamFrameData f2data {{
                {PsgParamType::TonePeriodA, 200},
            }};
            psgList.addFrameEvent(2, f2data, nullptr);

            auto& frames = psgList.getFrames();
            expectEquals(int(frames.size()), 3, "addFrameEvent: Should have 3 frames");

            {   // Frame 1 should have TonePeriodA accumulated from frame 0
                auto& f = frames[1]->getData();
                expectEquals(int(f.getRaw(PsgParamType::VolumeA)), 5, "addFrame F1: VolumeA=5");
                expectEquals(int(f.getRaw(PsgParamType::VolumeB)), 8, "addFrame F1: VolumeB=8");
                expectEquals(int(f.getRaw(PsgParamType::TonePeriodA)), 100, "addFrame F1: TonePeriodA accumulated=100");
            }

            {   // Frame 2 should have VolumeA/B accumulated from frame 1
                auto& f = frames[2]->getData();
                expectEquals(int(f.getRaw(PsgParamType::TonePeriodA)), 200, "addFrame F2: TonePeriodA=200");
                expectEquals(int(f.getRaw(PsgParamType::VolumeA)), 5, "addFrame F2: VolumeA accumulated=5");
                expectEquals(int(f.getRaw(PsgParamType::VolumeB)), 8, "addFrame F2: VolumeB accumulated=8");
            }
        }
    }
};

static PsgListAccumulatedStateTests psgListAccumulatedStateTests;

//==============================================================================
class PsgListInitialStateExportTests : public UnitTest {
public:
    PsgListInitialStateExportTests() : UnitTest("PsgListInitState", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("exportToPlaybackMidiSequence emits all params at time 0");
        {
            // PSG data where frame 0 only sets VolumeA — a typical sparse PSG start
            PsgData data {
                {
                    {{PsgRegType::VolumeA, 10}},  // F0: only volume A set
                    {{PsgRegType::VolumeB, 8}},   // F1
                },
                {50.0, 1}
            };

            auto edit = Edit::createSingleTrackEdit(engine);

            // Create a PsgClip to call exportToPlaybackMidiSequence
            auto t = te::getAudioTracks(*edit)[0];
            auto* clip = CustomClip::insertClipWithState(*t, {}, {}, CustomClip::Type::psg,
                {{0_tp, 4_td}, {}}, te::DeleteExistingClips::no, false);
            auto* psgClip = dynamic_cast<PsgClip*>(clip);
            expect(psgClip != nullptr, "PsgClip created");

            psgClip->getPsg().loadFrom(data, *edit, nullptr);

            auto seq = psgClip->getPsg().exportToPlaybackMidiSequence(
                *psgClip, te::MidiList::TimeBase::seconds);

            // Count distinct MIDI CC (controller number, channel) pairs at time 0
            // Each PsgParamType maps to one or more CC events on specific channels.
            // With the fix, the first frame should produce CC events for ALL param types.
            double firstTime = -1.0;
            int eventsAtTime0 = 0;
            for (int i = 0; i < seq.getNumEvents(); ++i) {
                auto& msg = seq.getEventPointer(i)->message;
                if (msg.isController()) {
                    if (firstTime < 0.0)
                        firstTime = msg.getTimeStamp();
                    if (std::abs(msg.getTimeStamp() - firstTime) < 1.0e-9)
                        eventsAtTime0++;
                }
            }

            // Minimum: 22 param types, some produce 2 CCs = at least 25 CC events
            expect(eventsAtTime0 > 10,
                "Should have many CC events at time 0 for complete AY init, got " + String(eventsAtTime0));

            // The accumulated value for VolumeA (set in frame 0) must be emitted at time 0.
            // VolumeA -> CC7 (Volume) on the base channel (psgChan 0).
            const int baseChannel = psgClip->getPsg().getMidiChannel().getChannelNumber();
            bool foundVolumeA = false;
            for (int i = 0; i < seq.getNumEvents(); ++i) {
                auto& msg = seq.getEventPointer(i)->message;
                if (std::abs(msg.getTimeStamp() - firstTime) < 1.0e-9
                    && msg.isControllerOfType(static_cast<int>(MidiCCType::Volume))
                    && msg.getChannel() == baseChannel) {
                    foundVolumeA = true;
                    expectEquals(msg.getControllerValue(), 10, "VolumeA accumulated value at time 0");
                }
            }
            expect(foundVolumeA, "VolumeA CC emitted at time 0");
        }
    }
};

static PsgListInitialStateExportTests psgListInitialStateExportTests;

//==============================================================================
class PsgListTempoChangeTimingTests : public UnitTest {
public:
    PsgListTempoChangeTimingTests() : UnitTest("PsgListTempoTiming", "MoTool") {}

    // Empty frames are dropped on load, so this leaves a single VolumeA frame at
    // tick 2 (2/50 s) -> an off-grid fraction of a beat at 120 BPM.
    static PsgData makeOffGridFrameData() {
        return PsgData {
            {
                {},
                {},
                {{PsgRegType::VolumeA, 10}},
            },
            {50.0, 1}
        };
    }

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("Load captures the machine-frame index and frame rate");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            auto data = makeOffGridFrameData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "timing");

            expectEquals(clip->getPsg().getNumFrames(), 1, "Single surviving frame");
            auto frame = clip->getPsg().getFrame(0);
            expect(frame->hasFrameIndex(), "Frame should carry a machine-frame index");
            expectEquals(frame->getFrameIndex(), 2, "Frame index should be the source tick");
            expectWithinAbsoluteError(clip->getPsg().getFrameRate(), 50.0, 1.0e-9,
                                      "Frame rate should be captured from the imported data");
            expectWithinAbsoluteError(clip->getPsg().getFramesPerBeat(), 25.0, 1.0e-9,
                                      "Frames per beat should match 50 fps at 120 BPM");
            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), 50.0, 1.0e-9,
                                      "Imported PSG should initially play at its source frame rate");
        }

        beginTest("Imported PSG frames store only frame indices");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            auto data = makeOffGridFrameData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "timing");

            auto frame = clip->getPsg().getFrame(0);
            expect(! frame->state.hasProperty(te::IDs::b), "PSG frame state should not store beat property");

            const auto expectedBeat = 2.0 / clip->getPsg().getFramesPerBeat();
            expectWithinAbsoluteError(frame->getFrameBeatPosition(*clip).inBeats(), expectedBeat, 1.0e-9,
                                      "Beat position should be derived from frame index and frames per beat");
        }

        beginTest("Preserve timing scales PSG frames per beat when project frames per beat changes");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);

            SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);

            auto data = makeOffGridFrameData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "timing");
            auto frame = clip->getPsg().getFrame(0);
            const auto originalTime = frame->getEditTime(*clip);
            const auto originalFrameIndex = frame->getFrameIndex();

            editViewState.setFramesPerBeat(30);

            const auto retimedTime = frame->getEditTime(*clip);
            expectWithinAbsoluteError(retimedTime.inSeconds(), originalTime.inSeconds(), 1.0e-9,
                                      "Preserve mode should keep PSG wall-clock time fixed");
            expectWithinAbsoluteError(clip->getPsg().getFramesPerBeat(), 30.0, 1.0e-9,
                                      "PSG frames per beat should scale by the beat-length ratio");
            expectEquals(frame->getFrameIndex(), originalFrameIndex, "Tempo preservation must not touch frame indices");
        }

        beginTest("PSG frame time stays fixed across a direct BPM change with quantisation");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);

            SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);

            auto data = makeOffGridFrameData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "timing");

            // Quantisation must not perturb the stored, frame-index-anchored position.
            clip->getQuantisation().setType("1 beat");

            auto frame = clip->getPsg().getFrame(0);
            const auto originalRawTime = frame->getRawEditTime(*clip);
            expect(originalRawTime.inSeconds() > 0.0, "Sanity: frame should be off the beat grid");
            expect(! approximatelyEqual(frame->getEditBeats(*clip).inBeats(),
                                        frame->getRawEditBeats(*clip).inBeats()),
                   "Sanity: quantisation should actually move the quantised view");

            editViewState.setBpmSnappedToFps(90.0);

            const auto preservedRawTime = frame->getRawEditTime(*clip);
            expectWithinAbsoluteError(preservedRawTime.inSeconds(), originalRawTime.inSeconds(), 1.0e-9,
                                      "Raw frame time must survive the tempo change despite quantisation");
            expectWithinAbsoluteError(clip->getPsg().getFramesPerBeat(), 33.0, 1.0e-9,
                                      "Snapped BPM change should scale PSG frames per beat in preserve mode");
        }

        beginTest("Unchecking preserve timing leaves PSG frames per beat fixed");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);

            SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            editViewState.setPreservePsgTimingOnTempoChange(false);

            auto data = makeOffGridFrameData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "timing");
            auto frame = clip->getPsg().getFrame(0);
            const auto originalRawTime = frame->getRawEditTime(*clip);
            const auto originalBeat = frame->getFrameBeatPosition(*clip);
            const auto originalFramesPerBeat = clip->getPsg().getFramesPerBeat();
            const auto originalFrameIndex = frame->getFrameIndex();

            editViewState.setBpmSnappedToFps(90.0);

            expectWithinAbsoluteError(clip->getPsg().getFramesPerBeat(), originalFramesPerBeat, 1.0e-9,
                                      "Unchecked mode should not scale PSG frames per beat");
            expectWithinAbsoluteError(frame->getFrameBeatPosition(*clip).inBeats(), originalBeat.inBeats(), 1.0e-9,
                                      "Unchecked mode should leave PSG frame beats unchanged");
            expectEquals(frame->getFrameIndex(), originalFrameIndex, "Unchecked mode must not touch frame indices");

            const auto retimedRawTime = frame->getRawEditTime(*clip);
            expect(retimedRawTime.inSeconds() > originalRawTime.inSeconds(),
                   "Unchecked mode should let PSG wall-clock timing follow the slower project tempo");
        }

        beginTest("Legacy beat-only frames are migrated to frame indices");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            auto track = getAudioTracks(*edit)[0];
            uZX::PsgData empty { {}, {50.0, 1} };
            auto clip = PsgClip::insertTo(*track, empty, {{0_tp, 4_td}, {}}, "legacy");

            // Beat 0.12 at 120 BPM == 0.06 s == frame 3 at 50 fps (off the .5 boundary).
            auto legacyState = juce::ValueTree(IDs::PSG);
            legacyState.setProperty(te::IDs::ver, 1, nullptr);
            legacyState.setProperty(te::IDs::channelNumber, te::MidiChannel(1), nullptr);

            auto legacyFrame = PsgParamFrame::createPsgFrameValueTree(
                -1, PsgParamFrameData {{PsgParamType::VolumeA, 7}});
            legacyFrame.removeProperty(IDs::i, nullptr);
            legacyFrame.setProperty(te::IDs::b, 0.12, nullptr);
            legacyState.addChild(legacyFrame, -1, nullptr);

            PsgList legacyList(legacyState, nullptr);
            auto frame = legacyList.getFrame(0);
            expect(! frame->hasFrameIndex(), "Sanity: legacy frame has no index");
            expect(frame->state.hasProperty(te::IDs::b), "Sanity: legacy frame has a beat property");
            expectWithinAbsoluteError(legacyList.getFrameRate(), 0.0, 1.0e-9,
                                      "Sanity: legacy list has no frame rate");
            expectWithinAbsoluteError(legacyList.getFramesPerBeat(), 0.0, 1.0e-9,
                                      "Sanity: legacy list has no frames-per-beat metadata");

            legacyList.migrateToFrameIndicesIfNeeded(*clip, 50.0, nullptr);

            expect(frame->hasFrameIndex(), "Frame should gain an index after migration");
            expectEquals(frame->getFrameIndex(), 3, "Migration should recover the nearest tick");
            expectWithinAbsoluteError(legacyList.getFrameRate(), 50.0, 1.0e-9,
                                      "List should adopt the migration frame rate");
            expectWithinAbsoluteError(legacyList.getFramesPerBeat(), 25.0, 1.0e-9,
                                      "List should backfill frames per beat from the current tempo");
            expect(! frame->state.hasProperty(te::IDs::b), "Migration should remove the legacy beat property");
            expectWithinAbsoluteError(frame->getFrameBeatPosition(*clip).inBeats(), 0.12, 1.0e-9,
                                      "Migrated frame should derive the original beat from its frame index");

            // Second call is a no-op once the frame rate is present.
            legacyList.migrateToFrameIndicesIfNeeded(*clip, 25.0, nullptr);
            expectWithinAbsoluteError(legacyList.getFrameRate(), 50.0, 1.0e-9,
                                      "Already-migrated list keeps its frame rate");
        }

        beginTest("Indexed legacy lists backfill frames per beat without touching indices");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            auto track = getAudioTracks(*edit)[0];
            uZX::PsgData empty { {}, {50.0, 1} };
            auto clip = PsgClip::insertTo(*track, empty, {{0_tp, 4_td}, {}}, "indexed legacy");

            auto legacyState = juce::ValueTree(IDs::PSG);
            legacyState.setProperty(te::IDs::ver, 1, nullptr);
            legacyState.setProperty(te::IDs::channelNumber, te::MidiChannel(1), nullptr);
            legacyState.setProperty(IDs::frameRate, 50.0, nullptr);
            legacyState.addChild(
                PsgParamFrame::createPsgFrameValueTree(2, PsgParamFrameData {{PsgParamType::VolumeA, 7}}),
                -1,
                nullptr);

            PsgList legacyList(legacyState, nullptr);
            auto frame = legacyList.getFrame(0);
            expectEquals(frame->getFrameIndex(), 2, "Sanity: indexed frame keeps its source index");
            expectWithinAbsoluteError(legacyList.getFramesPerBeat(), 0.0, 1.0e-9,
                                      "Sanity: indexed legacy list is missing frames per beat");

            legacyList.migrateToFrameIndicesIfNeeded(*clip, 25.0, nullptr);

            expectEquals(frame->getFrameIndex(), 2, "Backfill must not rewrite existing frame indices");
            expectWithinAbsoluteError(legacyList.getFrameRate(), 50.0, 1.0e-9,
                                      "Existing source frame rate should win over migration fallback");
            expectWithinAbsoluteError(legacyList.getFramesPerBeat(), 25.0, 1.0e-9,
                                      "Frames per beat should be inferred from frame rate and current tempo");
            expect(! frame->state.hasProperty(te::IDs::b), "Indexed legacy frame should not gain a beat property");
            expectWithinAbsoluteError(frame->getFrameBeatPosition(*clip).inBeats(), 0.08, 1.0e-9,
                                      "Indexed legacy beat should derive from frame index and frames per beat");
        }
    }
};

static PsgListTempoChangeTimingTests psgListTempoChangeTimingTests;

}  // namespace MoTool::Tests
