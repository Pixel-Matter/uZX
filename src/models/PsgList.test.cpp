#include <JuceHeader.h>
#include "controllers/EditState.h"
#include "models/EditUtilities.h"
#include "formats/psg/PsgData.h"
#include "PsgClip.h"
#include "PsgList.h"
#include "Ids.h"
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

        beginTest("getFrameAt returns full accumulated state");
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

            // Use actual beat positions from frames
            auto beat0 = frames[0]->getBeatPosition();
            auto beat2 = frames[2]->getBeatPosition();

            auto* f0 = psgList.getFrameAt(beat0);
            expect(f0 != nullptr, "getFrameAt should find frame at beat0");
            expectEquals(int(f0->getData().getRaw(PsgParamType::VolumeA)), 10, "State at beat0: VolumeA=10");
            expectEquals(int(f0->getData().getRaw(PsgParamType::VolumeB)), 0, "State at beat0: VolumeB=0 (not yet set)");

            auto* f2 = psgList.getFrameAt(beat2);
            expect(f2 != nullptr, "getFrameAt should find frame at beat2");
            expectEquals(int(f2->getData().getRaw(PsgParamType::VolumeA)), 10, "State at beat2: VolumeA accumulated=10");
            expectEquals(int(f2->getData().getRaw(PsgParamType::VolumeB)), 8, "State at beat2: VolumeB=8");
            expectEquals(int(f2->getData().getRaw(PsgParamType::VolumeC)), 6, "State at beat2: VolumeC=6");
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
            psgList.addFrameEvent(te::BeatPosition::fromBeats(0.0), f0data, nullptr);

            PsgParamFrameData f1data {{
                {PsgParamType::VolumeA, 5},
                {PsgParamType::VolumeB, 8},
            }};
            psgList.addFrameEvent(te::BeatPosition::fromBeats(1.0), f1data, nullptr);

            PsgParamFrameData f2data {{
                {PsgParamType::TonePeriodA, 200},
            }};
            psgList.addFrameEvent(te::BeatPosition::fromBeats(2.0), f2data, nullptr);

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

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("Preserve mode keeps PSG frame edit time when frames per beat changes");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);

            SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);

            PsgData data {
                {
                    {},
                    {},
                    {{PsgRegType::VolumeA, 10}},
                },
                {50.0, 1}
            };

            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "timing");
            auto frame = clip->getPsg().getFrame(0);
            const auto originalTime = frame->getEditTime(*clip);

            editViewState.setPreservePsgTimingOnTempoChange(true);
            editViewState.setFramesPerBeat(30);

            const auto preservedTime = frame->getEditTime(*clip);
            expectWithinAbsoluteError(preservedTime.inSeconds(), originalTime.inSeconds(), 1.0e-9,
                                      "PSG frame should keep its absolute edit time");
        }

        beginTest("Non-preserving mode retimes the PSG frame when frames per beat changes");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);

            SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);

            PsgData data {
                {
                    {},
                    {},
                    {{PsgRegType::VolumeA, 10}},
                },
                {50.0, 1}
            };

            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "timing");
            auto frame = clip->getPsg().getFrame(0);
            const auto originalTime = frame->getEditTime(*clip);

            editViewState.setPreservePsgTimingOnTempoChange(false);
            editViewState.setFramesPerBeat(30);

            const auto retimedTime = frame->getEditTime(*clip);
            expect(retimedTime > originalTime, "Beat-based (non-preserving) mode should retime the PSG frame");
        }

        beginTest("Preserve mode keeps raw frame time even with clip quantisation enabled");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);

            SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);

            PsgData data {
                {
                    {},
                    {},
                    {{PsgRegType::VolumeA, 10}},
                },
                {50.0, 1}
            };

            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "timing");

            // Snap to whole beats: an off-grid frame would be quantised away on a
            // round-trip through getEditTime/setEditTime.
            clip->getQuantisation().setType("1 beat");

            // Empty frames are dropped on load, so the only stored frame is the
            // VolumeA write at tick 2 (2/50 s) -> an off-grid fraction of a beat.
            expect(clip->getPsg().getNumFrames() == 1, "Sanity: expected a single PSG frame");
            auto frame = clip->getPsg().getFrame(0);
            expect(frame != nullptr, "Sanity: frame must exist");

            const auto originalRawTime = frame->getRawEditTime(*clip);
            expect(originalRawTime.inSeconds() > 0.0, "Sanity: frame should be off the beat grid");
            expect(! approximatelyEqual(frame->getEditBeats(*clip).inBeats(),
                                        frame->getRawEditBeats(*clip).inBeats()),
                   "Sanity: quantisation should actually move this frame");

            editViewState.setPreservePsgTimingOnTempoChange(true);
            editViewState.setFramesPerBeat(30);

            const auto preservedRawTime = frame->getRawEditTime(*clip);
            expectWithinAbsoluteError(preservedRawTime.inSeconds(), originalRawTime.inSeconds(), 1.0e-9,
                                      "Raw frame time must survive the tempo change despite quantisation");
        }
    }
};

static PsgListTempoChangeTimingTests psgListTempoChangeTimingTests;

//==============================================================================
class PsgListFramesPerBeatTests : public UnitTest {
public:
    PsgListFramesPerBeatTests() : UnitTest("PsgListFramesPerBeat", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        auto denseData = [] {
            return PsgData {
                {
                    {{PsgRegType::VolumeA, 1}},
                    {{PsgRegType::VolumeA, 2}},
                    {{PsgRegType::VolumeA, 3}},
                    {{PsgRegType::VolumeA, 4}},
                    {{PsgRegType::VolumeA, 5}},
                },
                {50.0, 1}
            };
        };

        beginTest("Effective fps is read from imported frames-per-beat metadata");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            auto data = denseData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "fps");

            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), 50.0, 1.0e-6,
                                      "At import tempo the metadata reproduces the PSG frame rate");
        }

        beginTest("Sparse imports keep dense frames-per-beat metadata");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            PsgData data {
                {
                    {{PsgRegType::VolumeA, 1}},
                    {},
                    {{PsgRegType::VolumeA, 2}},
                    {},
                    {{PsgRegType::VolumeA, 3}},
                },
                {50.0, 1}
            };

            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "sparse");

            expectEquals(clip->getPsg().getNumFrames(), 3, "Sanity: empty source frames are dropped");
            expectWithinAbsoluteError(clip->getPsg().getFramesPerBeat(), 25.0, 1.0e-9,
                                      "Sparse import stores dense source frames per beat");
            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), 50.0, 1.0e-6,
                                      "Sparse import reports the imported effective fps");
        }

        beginTest("A clip with one stored frame still reports metadata-derived rate");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            PsgData data {
                { {}, {}, {{PsgRegType::VolumeA, 10}} },  // one stored frame
                {50.0, 1}
            };
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "single");

            expectWithinAbsoluteError(clip->getPsg().getFramesPerBeat(), 25.0, 1.0e-9,
                                      "One stored frame still carries imported frames-per-beat metadata");
            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), 50.0, 1.0e-6,
                                      "One stored frame still reports imported effective fps");
            expect(clip->getPsg().hasFpsMismatch(*clip, 25.0),
                   "Known 50 fps effective rate should warn against a 25 fps edit");
        }

        beginTest("Frames per beat is stored from import tempo");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            // 60*50/26 = 115.384... BPM -> a clean 26 frames/beat, matching real legacy data.
            edit->tempoSequence.getTempo(0)->setBpm(60.0 * 50.0 / 26.0);

            auto data = denseData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "fpb");

            expectWithinAbsoluteError(clip->getPsg().getFramesPerBeat(), 26.0, 1.0e-9,
                                      "26 frames/beat is stored from PSG frame rate and import tempo");
            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), 50.0, 1.0e-6,
                                      "Effective fps comes from stored frames-per-beat + tempo");
        }

        beginTest("Legacy PSG without frames-per-beat is back-filled once from geometry");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(60.0 * 50.0 / 26.0);

            auto data = denseData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "legacy");

            auto legacyState = clip->getPsg().state.createCopy();
            legacyState.removeProperty(IDs::framesPerBeat, nullptr);

            PsgList reloaded(legacyState, nullptr);
            expect(reloaded.state.hasProperty(IDs::framesPerBeat),
                   "Legacy PSG should persist the inferred frames-per-beat value on load");
            expectWithinAbsoluteError(reloaded.getFramesPerBeat(), 26.0, 1.0e-9,
                                      "Legacy geometry fallback is used only to back-fill missing metadata");
        }

        beginTest("Effective fps scales with tempo (frames per beat stays fixed)");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            auto data = denseData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "scale");

            const auto fpb = clip->getPsg().getFramesPerBeat();
            auto beatLength = edit->tempoSequence.getTempoAt(0_tp).getApproxBeatLength().inSeconds();
            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip),
                                      fpb / beatLength, 1.0e-9,
                                      "effectiveFps = framesPerBeat / beatLength");

            // Frames keep their beats -> doubling the tempo doubles the firing rate.
            edit->tempoSequence.getTempo(0)->setBpm(240.0);
            beatLength = edit->tempoSequence.getTempoAt(0_tp).getApproxBeatLength().inSeconds();
            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip),
                                      fpb / beatLength, 1.0e-9,
                                      "Effective fps doubles when the BPM doubles");
        }

        beginTest("Effective fps ignores the timeline fps (a display setting)");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            auto data = denseData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "indep");

            const auto before = clip->getPsg().getEffectiveFps(*clip);

            // The timeline fps is only what the ruler displays; it must not retime the clip.
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::fps60);

            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), before, 1.0e-9,
                                      "Effective fps depends only on frame spacing and tempo");
        }

        beginTest("Changing BPM without preserving time rescales the clip's effective fps");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);

            auto data = denseData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "rescale");

            const auto before = clip->getPsg().getEffectiveFps(*clip);

            // Preserve OFF: frames keep their beats -> effective fps scales with the BPM ratio.
            editViewState.setPreservePsgTimingOnTempoChange(false);
            editViewState.setBpmSnappedToFps(60.0);

            const auto ratio = edit->tempoSequence.getTempoAt(0_tp).getBpm() / 120.0;
            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), before * ratio, 1.0e-6,
                                      "Non-preserving BPM change rescales the effective fps");
        }

        beginTest("BPM round-trip returns effective fps to exactly the same rate");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);

            auto data = denseData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "roundtrip");
            const auto before = clip->getPsg().getEffectiveFps(*clip);

            editViewState.setPreservePsgTimingOnTempoChange(false);
            edit->tempoSequence.getTempo(0)->setBpm(60.0);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            // Stored frames-per-beat is unchanged by non-preserving direct tempo edits.
            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), before, 1.0e-12,
                                      "Effective fps must not drift across a BPM round-trip");
            expect(! clip->getPsg().hasFpsMismatch(*clip, before),
                   "Round-trip must not produce a spurious mismatch");
        }

        beginTest("Fps mismatch tolerates float error but flags real differences");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            auto data = denseData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "tol");
            auto& psg = clip->getPsg();
            const auto eff = psg.getEffectiveFps(*clip);

            expect(! psg.hasFpsMismatch(*clip, eff), "Exact match must not warn");
            expect(! psg.hasFpsMismatch(*clip, eff - 1.0e-9), "Tiny float error must not warn");
            expect(psg.hasFpsMismatch(*clip, eff + 5.0), "A real fps difference must warn");
        }

        beginTest("Changing BPM with preserved time leaves the effective fps alone");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);

            auto data = denseData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "preserve");

            const auto before = clip->getPsg().getEffectiveFps(*clip);

            editViewState.setPreservePsgTimingOnTempoChange(true);
            editViewState.setBpmSnappedToFps(60.0);

            // Preserve rewrites frame beats and scales frames-per-beat, so effective fps stays put.
            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), before, 1.0e-6,
                                      "Preserving PSG timing keeps effective fps constant");
        }

        beginTest("Repeated preserved frames-per-beat changes do not drift metadata");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);

            SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);

            auto data = denseData();
            auto track = getAudioTracks(*edit)[0];
            auto clip = PsgClip::insertTo(*track, data, {{0_tp, 4_td}, {}}, "drift");

            editViewState.setPreservePsgTimingOnTempoChange(true);
            for (int i = 0; i < 20; ++i) {
                editViewState.setFramesPerBeat(30);
                editViewState.setFramesPerBeat(25);
            }

            expectWithinAbsoluteError(clip->getPsg().getFramesPerBeat(), 25.0, 1.0e-9,
                                      "Integer snapping should return frames-per-beat to the exact original value");
            expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), 50.0, 1.0e-6,
                                      "Effective fps should not drift after repeated preserved changes");
            expect(! clip->getPsg().hasFpsMismatch(*clip, 50.0),
                   "Round-tripped metadata should not produce a mismatch warning");
        }
    }
};

static PsgListFramesPerBeatTests psgListFramesPerBeatTests;

}  // namespace MoTool::Tests
