#include "TuningDetector.h"

#include <JuceHeader.h>

#include "../../formats/psg/PsgFile.h"

using namespace MoTool;

class TuningDetectorTest : public juce::UnitTest
{
public:
    TuningDetectorTest() : UnitTest("TuningDetector", "MoTool") {}

    /** A histogram of every grid note from C2 to B5, each held for `frames` frames. */
    static PitchHistogram histogramOf(const PeriodGrid& grid, int frames = 10) {
        PitchHistogram hist;
        for (const auto& [period, note] : grid)
            if (note >= 36 && note < 84)
                hist.tone[period] = frames;
        return hist;
    }

    void runTest() override {
        beginTest("Every built-in table is detected from its own periods");
        {
            for (int i = 0; i < static_cast<int>(BuiltinTuningType::size()); ++i) {
                const BuiltinTuningType table(i);
                const auto result = detectTuning(histogramOf(getBuiltinPeriodGrid(table)));
                expect(!result.usesEqualTemperament, "table " + String(i) + " fell back to 12TET");
                expectWithinAbsoluteError(result.share, 1.0f, 0.001f);
                // PT #0 and PT #3 are one grid shifted a semitone: the later entry names it.
                const int expected = i == static_cast<int>(BuiltinTuningType::CustomPT_0_PT)
                                         ? static_cast<int>(BuiltinTuningType::CustomPT_3_REAL)
                                         : i;
                expectEquals(static_cast<int>(result.tables.front().table), expected);
            }
        }

        beginTest("Duplicate grids are merged into the later registry entry");
        {
            const auto result = detectTuning(histogramOf(getBuiltinPeriodGrid(BuiltinTuningType::CustomPT_1_ST)));
            for (const auto& candidate : result.tables)
                expect(static_cast<int>(candidate.table) != static_cast<int>(BuiltinTuningType::CustomPT_0_PT),
                       "PT #0 should be absorbed by PT #3");
            expect(!result.tableTie);
        }

        beginTest("Equal temperament fallback finds A4 when no table fits");
        {
            const auto result = detectTuning(histogramOf(makeEqualTemperamentGrid(436.0, 1773400.0)));
            expect(result.usesEqualTemperament);
            expectWithinAbsoluteError(result.equalTemperament.a4Frequency, 436.0, 0.5);
            expectGreaterThan(result.share, 0.95f);
            expectEquals(result.getTableLabel(), String("12TET 436 Hz"));
        }

        beginTest("Envelope pitches match within half an envelope step");
        {
            const auto& grid = getBuiltinPeriodGrid(BuiltinTuningType::CustomPT_1_ST);
            PitchHistogram hist;
            for (const auto& [period, note] : grid)
                if (note >= 36 && note < 48)
                    hist.envelope[period + 5] = 10;  // rounded to tone / 16, as trackers do
            expectWithinAbsoluteError(hist.getShareOnGrid(grid), 1.0f, 0.001f);
            PitchHistogram tone;
            for (const auto& [period, note] : grid)
                if (note >= 36 && note < 48)
                    tone.tone[period + 5] = 10;
            expectWithinAbsoluteError(tone.getShareOnGrid(grid), 0.0f, 0.001f);
        }

        beginTest("Key ranking: A minor pitch classes rank A minor first");
        {
            std::array<float, 12> pcs{};
            pcs[9] = 0.30f;   // A
            pcs[0] = 0.20f;   // C
            pcs[4] = 0.20f;   // E
            pcs[2] = 0.10f;   // D
            pcs[7] = 0.10f;   // G
            pcs[5] = 0.05f;   // F
            pcs[11] = 0.05f;  // B
            const auto keys = rankKeys(pcs);
            expectEquals(keys.front().getName(), String("A minor"));
            expect(keys.front().minor);
            expectEquals(static_cast<int>(keys.front().getScaleType()),
                         static_cast<int>(Scale::ScaleType::AeolianOrMinor));
        }

        beginTest("Folded histogram puts grid notes on bin multiples");
        {
            const auto& grid = getBuiltinPeriodGrid(BuiltinTuningType::CustomPT_1_ST);
            PitchHistogram hist;
            for (const auto& [period, note] : grid)
                if (note == 48 || note == 55)  // C3 and G3
                    hist.tone[period] = 100;
            const auto bins = makeFoldedPitchHistogram(hist, grid, 5);
            expectEquals(static_cast<int>(bins.size()), 60);
            expectWithinAbsoluteError(bins[0], 1.0f, 0.001f);   // C
            expectWithinAbsoluteError(bins[35], 1.0f, 0.001f);  // G
            float offGrid = 0.0f;
            for (size_t i = 0; i < bins.size(); ++i)
                if (i != 0 && i != 35)
                    offGrid += bins[i];
            expectWithinAbsoluteError(offGrid, 0.0f, 0.001f);
        }

        beginTest("Corpus fixtures detect the known table and key");
        {
            const auto dir = File(UZX_FIXTURES_DIR).getChildFile("psg");
            if (! dir.isDirectory()) {
                logMessage("fixtures missing at " + dir.getFullPathName() + ", skipping");
                return;
            }
            struct Expected
            {
                const char* prefix;
                BuiltinTuningType table;
                bool tet;
                const char* key;
            };
            const Expected expected[] = {
                {"01_Klav", BuiltinTuningType::CustomPT_1_ST, false, "A minor"},
                {"08_D-Juice", BuiltinTuningType::CustomPT_2_ASM, false, "C minor"},
                {"04_Igoval", BuiltinTuningType::EqualTemperament, false, "A minor"},
                {"21_Quiet", BuiltinTuningType::CustomPT_2_ASM, false, "E major"},
                {"16_Factor6", BuiltinTuningType::EqualTemperament, true, "F"},  // F♯ minor vs A major tie
            };
            for (const auto& e : expected) {
                const auto files = dir.findChildFiles(File::findFiles, false, String(e.prefix) + "*.psg");
                expectEquals(files.size(), 1, String("fixture ") + e.prefix);
                if (files.isEmpty())
                    continue;
                const auto hist = histogramOfFile(files.getFirst());
                const auto result = detectTuning(hist);
                logMessage(String(e.prefix) + ": tone-frames " + String(sumFrames(hist.tone)) + ", env-frames "
                           + String(sumFrames(hist.envelope)));
                expect(result.usesEqualTemperament == e.tet, String(e.prefix) + " fallback");
                if (!e.tet)
                    expectEquals(static_cast<int>(result.tables.front().table),
                                 static_cast<int>(e.table),
                                 String(e.prefix) + " table");
                expect(result.getKey().getName().startsWith(e.key),
                       String(e.prefix) + " key " + result.getKey().getName());
                logMessage(String(e.prefix) + ": " + result.getTableLabel() + String::formatted(" %.2f, ", result.share)
                           + result.getKey().getName() + (result.tableTie ? " [table tie]" : "")
                           + (result.keyTie ? " [key tie]" : ""));
            }
            const auto factor6 =
                detectTuning(histogramOfFile(dir.findChildFiles(File::findFiles, false, "16_Factor6*.psg").getFirst()));
            expectWithinAbsoluteError(factor6.equalTemperament.a4Frequency, 436.0, 1.0);
            expect(factor6.keyTie);
        }
    }

private:
    static int sumFrames(const std::map<int, int>& part) {
        int total = 0;
        for (const auto& [period, frames] : part)
            total += frames;
        return total;
    }

    /** Accumulates the register stream like PsgList does and feeds every frame in. */
    static PitchHistogram histogramOfFile(const File& file) {
        uZX::PsgFile psg(file);
        PitchHistogram hist;
        if (!psg.ensureRead())
            return hist;
        // Same two-stage accumulation as PsgList::loadFrom: registers first, so a frame
        // that writes only the fine period byte keeps its coarse byte.
        uZX::PsgRegsFrame regsState;
        PsgParamFrameData params;
        params.resetMixer();
        for (const auto& regs : psg.getData().frames) {
            regsState.clear();
            regsState.update(regs);
            params.clear();
            params.update(regsState);
            hist.addFrame(params);
        }
        return hist;
    }
};

static TuningDetectorTest tuningDetectorTest;
