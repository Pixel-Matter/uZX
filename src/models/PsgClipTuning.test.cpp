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

    /** A clip playing every PT #1 note of an A minor scale on channel A, one frame each. */
    static uZX::PsgData aMinorOnPt1() {
        uZX::PsgData data{{}, {50.0, 1}};
        const auto& grid = getBuiltinPeriodGrid(BuiltinTuningType::CustomPT_1_ST);
        for (const auto& [period, note] : grid) {
            const int pc = note % 12;
            if (note < 45 || note > 81 || !(pc == 9 || pc == 11 || pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7))
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

        beginTest("A stored A4 wins over a stored table; a damaged A4 is ignored");
        {
            clip->state.setProperty(IDs::clipTuningA4, 436.0, nullptr);
            expect(clip->getTuning().isEqualTemperament && !clip->getTuning().table.has_value());
            expectEquals(clip->getTuning().tableLabel, String("12TET 436 Hz"));
            for (const var& damaged : {var(0.0), var(-440.0), var(std::nan("")), var("abc"), var(5000.0)}) {
                clip->state.setProperty(IDs::clipTuningA4, damaged, nullptr);
                expect(!clip->getTuning().isEqualTemperament, "ignored A4 " + damaged.toString());
                expect(clip->getTuning().table.has_value()
                       && *clip->getTuning().table == BuiltinTuningType::CustomPT_2_ASM);
            }
            clip->state.removeProperty(IDs::clipTuningA4, nullptr);
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
    }
};

static PsgClipTuningTest psgClipTuningTest;

}  // namespace MoTool
