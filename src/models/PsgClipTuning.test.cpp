#include <JuceHeader.h>

#include <cmath>

#include "../formats/psg/PsgData.h"
#include "Ids.h"
#include "PsgClip.h"

namespace MoTool {

/** PsgClip::getTuning(): which stored choice wins, which stored values are ignored, and
    when the cached result is rebuilt. */
class PsgClipTuningTest : public UnitTest
{
public:
    PsgClipTuningTest() : UnitTest("PsgClipTuning", "MoTool") {}

    /** A clip playing every PT #1 note of an A minor scale on channel A, one frame each;
        only the pitch classes `keep` accepts, when given. */
    static uZX::PsgData aMinorOnPt1(std::function<bool (int)> keep = {}) {
        uZX::PsgData data{{}, {50.0, 1}};
        const auto& grid = getBuiltinPeriodGrid(BuiltinTuningType::CustomPT_1_ST);
        for (const auto& [period, note] : grid) {
            const int pc = note % 12;
            if (note < 45 || note > 81 || !(pc == 9 || pc == 11 || pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7))
                continue;
            if (keep && ! keep(pc))
                continue;
            const int weight = pc == 9 ? 6 : pc == 4 || pc == 0 ? 3 : 1;  // tonic, fifth, third lead
            for (int k = 0; k < weight; ++k)
                data.frames.push_back({{uZX::PsgRegType::VolumeA, 12},
                                       {uZX::PsgRegType::TonePeriodFineA, static_cast<uint8_t>(period & 0xff)},
                                       {uZX::PsgRegType::TonePeriodCoarseA, static_cast<uint8_t>(period >> 8)}});
        }
        return data;
    }

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto insert = [&edit](uZX::PsgData data) {
            const auto length = te::TimeDuration::fromSeconds(data.getLengthSeconds());
            return PsgClip::insertTo(*te::getAudioTracks(*edit)[0], data, {{te::TimePosition(), length}, {}}, "t");
        };

        beginTest("Import weights each register state by how long it sounds");
        {
            // PsgList keeps only frames that write a register: a held A is one stored frame
            // followed by silence in the register stream. Counting stored frames would let
            // the eight one-frame C-major notes outweigh it.
            const auto& grid = getBuiltinPeriodGrid(BuiltinTuningType::CustomPT_1_ST);
            auto periodOf = [&grid](int note) {
                for (const auto& [period, n] : grid)
                    if (n == note)
                        return period;
                return 0;
            };
            auto noteFrame = [](int period) {
                return uZX::PsgRegsFrame{{uZX::PsgRegType::VolumeA, 12},
                                         {uZX::PsgRegType::TonePeriodFineA, static_cast<uint8_t>(period & 0xff)},
                                         {uZX::PsgRegType::TonePeriodCoarseA, static_cast<uint8_t>(period >> 8)}};
            };
            uZX::PsgData data{{}, {50.0, 1}};
            data.frames.push_back(noteFrame(periodOf(57)));  // A3, held
            for (int i = 0; i < 199; ++i)
                data.frames.push_back({});
            for (int note : {60, 64, 67, 71, 62, 65, 60, 64})  // C E G B D F C E, one frame each
                data.frames.push_back(noteFrame(periodOf(note)));
            data.frames.push_back(noteFrame(periodOf(57)));  // A3 again, held to the end
            for (int i = 0; i < 49; ++i)
                data.frames.push_back({});

            auto held = insert(data);
            expect(held != nullptr);
            if (held != nullptr) {
                const auto hist = held->getPitchHistogram();
                expectEquals(
                    hist.tone.at(periodOf(57)), 250, "both held As count every frame, the last to the clip end");
                expectEquals(hist.tone.at(periodOf(60)), 2);
                expectEquals(hist.getTotal(), 258);
                expectEquals(held->getTuning().key.getName(), String("A minor"));
            }
        }

        beginTest("Choosing a table re-ranks the keys on that table");
        {
            // An A minor phrase written on PT #0 is detected on PT #3, which names every
            // note a semitone higher. Choosing PT #0 must bring the key back to A minor.
            const auto& pt0 = getBuiltinPeriodGrid(BuiltinTuningType::CustomPT_0_PT);
            uZX::PsgData data{{}, {50.0, 1}};
            for (const auto& [period, note] : pt0) {
                const int pc = note % 12;
                if (note < 45 || note > 81
                    || !(pc == 9 || pc == 11 || pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7))
                    continue;
                const int weight = pc == 9 ? 6 : pc == 4 || pc == 0 ? 3 : 1;
                for (int k = 0; k < weight; ++k)
                    data.frames.push_back({{uZX::PsgRegType::VolumeA, 12},
                                           {uZX::PsgRegType::TonePeriodFineA, static_cast<uint8_t>(period & 0xff)},
                                           {uZX::PsgRegType::TonePeriodCoarseA, static_cast<uint8_t>(period >> 8)}});
            }
            auto pt0Clip = insert(data);
            expect(pt0Clip != nullptr);
            if (pt0Clip != nullptr) {
                expect(*pt0Clip->getTuning().table == BuiltinTuningType::CustomPT_3_REAL,
                       "detection merges PT #0 into PT #3");
                expectEquals(pt0Clip->getTuning().key.getName(), String::fromUTF8("A\xe2\x99\xaf minor"));
                pt0Clip->chooseTuningTable(BuiltinTuningType::CustomPT_0_PT);
                expectEquals(pt0Clip->getTuning().key.getName(), String("A minor"));
                expectEquals(pt0Clip->getTuning().keys.front().getName(), String("A minor"));
                pt0Clip->chooseKey({Scale::Tonic(4), true});  // an explicit key survives a table change
                pt0Clip->chooseTuningTable(BuiltinTuningType::CustomPT_3_REAL);
                expectEquals(pt0Clip->getTuning().key.getName(), String("E minor"));
            }
        }

        beginTest("Without a choice the detection is in effect");
        auto clip = insert(aMinorOnPt1());
        expect(clip != nullptr);
        if (clip == nullptr)
            return;
        {
            const auto& tuning = clip->getTuning();
            expect(tuning.table.has_value() && *tuning.table == BuiltinTuningType::CustomPT_1_ST, tuning.tableLabel);
            expect(tuning.hasKey && !tuning.tableChosen && !tuning.keyChosen);
            expectEquals(tuning.key.getName(), String("A minor"));
        }

        beginTest("The cached tuning is reused until something changes");
        {
            const auto* first = &clip->getTuning();
            const int revision = first->revision;
            expect(&clip->getTuning() == first);
            expectEquals(clip->getTuning().revision, revision);
            clip->chooseTuningTable(BuiltinTuningType::CustomPT_2_ASM);
            expect(clip->getTuning().revision != revision, "a choice must rebuild the tuning");
            expect(clip->getTuning().tableChosen && *clip->getTuning().table == BuiltinTuningType::CustomPT_2_ASM);
        }

        beginTest("Clip length, offset and tempo context rebuild the cached detection");
        {
            // 400 sounds from frame 0, 200 from frame 10 to the clip's end at frame 20.
            uZX::PsgData data{{}, {50.0, 1}};
            for (int period : {400, 200}) {
                data.frames.push_back({{uZX::PsgRegType::VolumeA, 12},
                                       {uZX::PsgRegType::TonePeriodFineA, static_cast<uint8_t>(period & 0xff)},
                                       {uZX::PsgRegType::TonePeriodCoarseA, static_cast<uint8_t>(period >> 8)}});
                for (int i = 0; i < 9; ++i)
                    data.frames.push_back({});
            }
            auto timed = insert(data);
            auto& um = edit->getUndoManager();
            auto lastNoteFrames = [&timed] { return timed->getTuningDetection().histogram.tone.at(200); };
            const auto start = timed->getPosition().getStart();
            expectEquals(lastNoteFrames(), 10);
            timed->chooseKey({Scale::Tonic(2), false});

            const int detectionRevision = timed->getTuningDetectionRevision();
            const int tuningRevision = timed->getTuning().revision;
            um.beginNewTransaction();
            timed->setLength(te::TimeDuration::fromSeconds(0.6), false);
            expectEquals(lastNoteFrames(), 20, "a longer clip holds the last note longer");
            expect(timed->getTuningDetectionRevision() != detectionRevision, "the picker sees a new detection");
            expect(timed->getTuning().revision != tuningRevision, "the painter sees a new tuning");
            expect(timed->getTuning().keyChosen && timed->getTuning().key.getName() == "D major",
                   "an explicit key survives the rebuild");
            um.undo();
            expectEquals(lastNoteFrames(), 10, "undo restores the length");
            um.redo();
            expectEquals(lastNoteFrames(), 20, "redo extends it again");
            um.undo();

            timed->setPosition({{start, te::TimeDuration::fromSeconds(0.4)}, te::TimeDuration::fromSeconds(0.1)});
            expectEquals(lastNoteFrames(), 15, "an offset moves the source end");
            timed->setPosition({{start, te::TimeDuration::fromSeconds(0.4)}, {}});
            expectEquals(lastNoteFrames(), 10);

            const auto framesPerBeat = timed->getPsg().getFramesPerBeat();
            timed->getPsg().setFramesPerBeat(framesPerBeat * 2, nullptr);
            expectEquals(lastNoteFrames(), 30, "frames per beat sets how many frames the clip spans");
            timed->getPsg().setFramesPerBeat(framesPerBeat, nullptr);
            expectEquals(lastNoteFrames(), 10);

            // A tempo twice as fast at 10 s: moved past it, the clip spans twice the frames.
            auto& tempos = edit->tempoSequence;
            const double bpm = tempos.getTempoAt(start).getBpm();
            tempos.insertTempo(tempos.toBeats(te::TimePosition::fromSeconds(10.0)), bpm * 2, 0.0f);
            const auto clipLength = timed->getPosition().getLength();
            timed->setPosition({{te::TimePosition::fromSeconds(20.0), clipLength}, {}});
            expectEquals(lastNoteFrames(), 30, "crossing a tempo change rebuilds the detection");
        }

        beginTest("A stored A4 wins over a stored table; a damaged A4 is ignored");
        {
            clip->state.setProperty(IDs::clipTuningA4, 436.0, nullptr);
            expect(clip->getTuning().fit.has_value() && !clip->getTuning().table.has_value());
            expectEquals(clip->getTuning().tableLabel, String("12TET 436 Hz"));
            for (const var& damaged : {var(0.0), var(-440.0), var(std::nan("")), var("abc"), var(5000.0)}) {
                clip->state.setProperty(IDs::clipTuningA4, damaged, nullptr);
                expect(!clip->getTuning().fit.has_value(), "ignored A4 " + damaged.toString());
                expect(clip->getTuning().table.has_value()
                       && *clip->getTuning().table == BuiltinTuningType::CustomPT_2_ASM);
            }
            clip->state.setProperty(IDs::clipTuningA4, 440.0, nullptr);
            clip->state.setProperty(IDs::clipTuningClock, 2000000.0, nullptr);
            expectEquals(clip->getTuning().tableLabel, String("12TET 440 Hz Atari ST"));
            for (const var& damaged : {var(0.0), var(std::nan("")), var("abc"), var(1.0e9)}) {
                clip->state.setProperty(IDs::clipTuningClock, damaged, nullptr);
                expect(!clip->getTuning().fit.has_value(), "ignored clock " + damaged.toString());
            }
            clip->state.removeProperty(IDs::clipTuningClock, nullptr);
            clip->state.removeProperty(IDs::clipTuningA4, nullptr);
        }

        beginTest("A chosen fit stores its system, tonic, A4 and clock");
        {
            const double pentagon = uZX::ChipClockEnum::clockValues[uZX::ChipClockEnum::Pentagon_1_75_MHz];
            clip->chooseFit({TuningSystemType::Just5Limit, Scale::Tonic::D, 433.0, pentagon});
            const auto& tuning = clip->getTuning();
            expect(tuning.tableChosen && tuning.fit.has_value() && !tuning.table.has_value());
            expectEquals(tuning.tableLabel, String("5LJI D 433 Hz Pentagon"));
            expect(tuning.grid == makeFitGrid({TuningSystemType::Just5Limit, Scale::Tonic::D, 433.0, pentagon}));
            for (const auto& [id, damaged] : {std::pair{&IDs::clipTuningSystem, var("NoSuchSystem")},
                                              std::pair{&IDs::clipTuningSystem, var("CustomRational")},
                                              std::pair{&IDs::clipTuningTonic, var(12)}}) {
                const auto kept = clip->state[*id];
                clip->state.setProperty(*id, damaged, nullptr);
                expect(!clip->getTuning().fit.has_value(), "ignored " + id->toString() + " " + damaged.toString());
                clip->state.setProperty(*id, kept, nullptr);
            }
            clip->chooseTuningTable(BuiltinTuningType::CustomPT_2_ASM);
            for (const auto* id : {&IDs::clipTuningA4, &IDs::clipTuningClock, &IDs::clipTuningSystem, &IDs::clipTuningTonic})
                expect(!clip->state.hasProperty(*id), "a table choice drops " + id->toString());
        }

        beginTest("An unknown table name falls back to the detection");
        {
            clip->state.setProperty(IDs::clipTuningTable, "NoSuchTable", nullptr);
            const auto& tuning = clip->getTuning();
            expect(!tuning.tableChosen);
            expect(tuning.table.has_value() && *tuning.table == BuiltinTuningType::CustomPT_1_ST);
        }

        beginTest("A key choice needs a tonic in range");
        {
            clip->state.setProperty(IDs::clipKeyTonic, 14, nullptr);
            expect(!clip->getTuning().keyChosen);
            expectEquals(clip->getTuning().key.getName(), String("A minor"));
            clip->chooseKey({Scale::Tonic(4), true});
            expect(clip->getTuning().keyChosen);
            expectEquals(clip->getTuning().key.getName(), String("E minor"));
            clip->clearTuningChoice();
            expect(!clip->getTuning().keyChosen && !clip->getTuning().tableChosen);
            expectEquals(clip->getTuning().key.getName(), String("A minor"));
        }

        beginTest("A clip without audible notes has no table, key or doubt");
        {
            uZX::PsgData silent{{{{uZX::PsgRegType::VolumeA, 0}, {uZX::PsgRegType::TonePeriodFineA, 100}}}, {50.0, 1}};
            auto quiet = insert(silent);
            expect(quiet != nullptr);
            if (quiet != nullptr) {
                const auto& tuning = quiet->getTuning();
                expect(tuning.grid.empty() && tuning.tableLabel.isEmpty());
                expect(!tuning.hasKey && !tuning.uncertain);
                quiet->chooseTuningTable(BuiltinTuningType::CustomPT_1_ST);
                expectEquals(quiet->getTuning().tableLabel, String("PT #1"));
            }
        }

        beginTest("Clips played together are detected together, as a Turbo Sound tune");
        {
            auto setEdit = te::Edit::createSingleTrackEdit(engine);
            setEdit->ensureNumberOfAudioTracks(2);
            const auto tracks = te::getAudioTracks(*setEdit);
            const auto insertOn = [](te::AudioTrack& track, uZX::PsgData data) {
                const auto length = te::TimeDuration::fromSeconds(data.getLengthSeconds());
                return PsgClip::insertTo(track, data, {{te::TimePosition(), length}, {}}, "t");
            };
            // the tonic triad on one chip, the rest of the scale on the other
            auto triad = insertOn(*tracks[0], aMinorOnPt1([](int pc) { return pc == 9 || pc == 0 || pc == 4; }));
            auto rest = insertOn(*tracks[1], aMinorOnPt1([](int pc) { return pc == 11 || pc == 2 || pc == 5 || pc == 7; }));
            const auto together = detectTuningTogether({triad.get(), rest.get()});
            expect(together.hasNotes);
            expectEquals(together.keys.front().getName(), String("A minor"));
            expectEquals(together.getTableLabel(), String("PT #1"));
            expectEquals(together.histogram.getTotal(),
                         triad->getTuningDetection().histogram.getTotal() + rest->getTuningDetection().histogram.getTotal());

            // Linked as one tune, every clip has the tune's detection, and a choice on one is on all.
            PsgClip::linkAsOneTune({triad.get(), rest.get()});
            for (auto* clip : {triad.get(), rest.get()}) {
                expectEquals(clip->getTuneClips().size(), 2);
                expectEquals(clip->getTuningDetection().histogram.getTotal(), together.histogram.getTotal());
                expectEquals(clip->getTuning().key.getName(), String("A minor"));
                expectEquals(clip->getTuning().tableLabel, String("PT #1"));
            }
            rest->chooseKey({Scale::Tonic(0), false, 1.0f});
            expectEquals(triad->getTuning().key.getName(), String("C major"));
            expect(triad->getTuning().keyChosen);
            triad->clearTuningChoice();
            expect(! rest->getTuning().keyChosen);
            expectEquals(rest->getTuning().key.getName(), String("A minor"));
        }

        beginTest("Removing a frame, and undoing it, rebuilds the cached histogram");
        {
            auto noteFrame = [](int period) {
                return uZX::PsgRegsFrame{{uZX::PsgRegType::VolumeA, 12},
                                         {uZX::PsgRegType::TonePeriodFineA, static_cast<uint8_t>(period & 0xff)},
                                         {uZX::PsgRegType::TonePeriodCoarseA, static_cast<uint8_t>(period >> 8)}};
            };
            uZX::PsgData data{{}, {50.0, 1}};
            for (int period : {400, 200}) {
                data.frames.push_back(noteFrame(period));
                for (int i = 0; i < 9; ++i)
                    data.frames.push_back({});
            }
            auto clip = insert(data);
            auto& psg = clip->getPsg();
            auto& um = edit->getUndoManager();
            expectEquals(clip->getPitchHistogram().tone.at(200), 10);

            um.beginNewTransaction();
            psg.state.removeChild(psg.getFrames()[1]->state, &um);
            expectEquals(clip->getPitchHistogram().tone.count(200), size_t(0), "the removed note is gone");
            expectEquals(clip->getPitchHistogram().tone.at(400), 20, "the first note now sounds to the clip end");

            um.undo();
            expectEquals(clip->getPitchHistogram().tone.at(200), 10, "undo brings the note back");
            expectEquals(clip->getPitchHistogram().tone.at(400), 10);
        }
    }
};

static PsgClipTuningTest psgClipTuningTest;

}  // namespace MoTool
