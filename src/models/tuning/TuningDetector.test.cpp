#include "TuningDetector.h"

#include <JuceHeader.h>

#include <algorithm>

#include "../../formats/psg/PsgFile.h"
#include "../PsgClip.h"

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
        runCorpusReport();

        beginTest("Every tracker table is detected from its own periods");
        {
            for (int i = 0; i < static_cast<int>(BuiltinTuningType::size()); ++i) {
                const BuiltinTuningType table(i);
                if (!isTrackerTable(table))
                    continue;  // a computed preset, not a candidate
                const auto result = detectTuning(histogramOf(getBuiltinPeriodGrid(table)));
                expect(!result.usesFit, "table " + String(i) + " fell back to 12TET");
                // Sound Tracker's table is only set from modules: detected, it is PT #1 but for two notes.
                if (table == BuiltinTuningType::CustomST)
                    expectGreaterThan(result.share, 0.9f);
                else
                    expectWithinAbsoluteError(result.share, 1.0f, 0.001f);
                // PT #0 and PT #3 are one grid shifted a semitone: the later entry names it.
                const int expected = i == static_cast<int>(BuiltinTuningType::CustomPT_0_PT)
                                         ? static_cast<int>(BuiltinTuningType::CustomPT_3_REAL)
                                     : i == static_cast<int>(BuiltinTuningType::CustomST)
                                         ? static_cast<int>(BuiltinTuningType::CustomPT_1_ST)
                                         : i;
                expectEquals(static_cast<int>(result.tables.front().table), expected);
            }
        }

        beginTest("A tracker table's clock and A4 describe its periods");
        {
            // The registry gives each table the clock and A4 its sources name; measured at
            // them, its notes must sit near that reference (PT #1 is not exact 12TET).
            for (int i = 0; i < static_cast<int>(BuiltinTuningType::size()); ++i) {
                const BuiltinTuningType table(i);
                if (!isTrackerTable(table))
                    continue;
                const auto tuning = makeBuiltinTuning(table);
                std::vector<double> offsets;
                for (int note = 36; note < 96; ++note)
                    offsets.push_back(std::abs(tuning->getOfftune(note)));
                std::nth_element(offsets.begin(), offsets.begin() + long(offsets.size() / 2), offsets.end());
                expectLessThan(offsets[offsets.size() / 2], 5.0, String(table.getShortLabel().data()) + " median cents");
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
            expect(result.usesFit);
            expectWithinAbsoluteError(result.getFit().a4Frequency, 436.0, 0.5);
            expectGreaterThan(result.share, 0.95f);
            expectEquals(result.getTableLabel(), String("12TET 436 Hz"));
        }

        beginTest("Equal temperament names the clock at which A4 is 440 Hz");
        {
            using Clock = uZX::ChipClockEnum;
            const double st = Clock::clockValues[Clock::Atari_ST_2_MHz];
            // Only the ZX scene's clocks are guessed: a 440 Hz tune at the Atari ST clock
            // reads as a ZX one, two semitones lower, until the ST is chosen by hand.
            const auto atari = detectTuning(histogramOf(makeEqualTemperamentGrid(440.0, st)));
            expect(atari.usesFit);
            expectWithinAbsoluteError(atari.getFit().chipClock, tuningGridClock, 1e-9);
            expectEquals(atari.getTableLabel(), String("12TET 438 Hz"));
            const auto chosen = atClock(atari.getFit(), st);
            expectWithinAbsoluteError(chosen.a4Frequency, 440.0, 0.5);
            expectEquals(getFitLabel(chosen), String("12TET 440 Hz Atari ST"));
            const int a4Period = roundToInt(st / 16.0 / 440.0);
            const auto chosenGrid = makeFitGrid(chosen);
            expect(chosenGrid.count(a4Period) && chosenGrid.at(a4Period) == 69, "notes must be named at the chosen clock");

            // The 12TET preset is a shortcut, not a candidate. 12TET at 440 Hz on the ZX clock
            // is PT 3.3's table #2 to within a period, and the tracker table wins the draw.
            const auto zx = detectTuning(histogramOf(makeEqualTemperamentGrid(440.0, tuningGridClock)));
            for (const auto& candidate : zx.tables)
                expect(candidate.table != BuiltinTuningType::EqualTemperament, "presets are not candidates");
            expect(!zx.usesFit);
            expectEquals(zx.getTableLabel(), String("PT #2 (3.3)"));
            expectWithinAbsoluteError(zx.getFit().chipClock, tuningGridClock, 1e-9);

            // PT #2 is 12TET at 440 Hz on the Pentagon clock: the tracker table still wins,
            // while the fit names its clock.
            const auto pentagon = detectTuning(
                histogramOf(makeEqualTemperamentGrid(440.0, Clock::clockValues[Clock::Pentagon_1_75_MHz])));
            expect(!pentagon.usesFit);
            expectEquals(static_cast<int>(pentagon.tables.front().table),
                         static_cast<int>(BuiltinTuningType::CustomPT_2_ASM));
            expectWithinAbsoluteError(pentagon.getFit().chipClock, Clock::clockValues[Clock::Pentagon_1_75_MHz], 1e-9);
        }

        beginTest("Just intonations are fitted with their tonic, A4 and clock");
        {
            using Clock = uZX::ChipClockEnum;
            const double pentagon = Clock::clockValues[Clock::Pentagon_1_75_MHz];
            const TuningFit justE{TuningSystemType::Just5Limit, Scale::Tonic::E, 440.0, tuningGridClock};
            const auto ji = detectTuning(histogramOf(makeFitGrid(justE)));
            expect(ji.usesFit);
            expect(ji.getFit().system == TuningSystemType::Just5Limit, ji.getTableLabel());
            expectEquals(static_cast<int>(ji.getFit().tonic.asUnderlying()), 4);
            expectWithinAbsoluteError(ji.getFit().a4Frequency, 440.0, 0.5);
            expectEquals(ji.getTableLabel(), String("5LJI E 440 Hz"));
            for (const auto& [period, note] : makeFitGrid(justE))
                if (note >= 36 && note < 84)
                    expect(ji.grid.count(period) && ji.grid.at(period) == note, "the fit must name period " + String(period));

            const TuningFit pythagoreanC{TuningSystemType::Pythagorean, Scale::Tonic::C, 440.0, pentagon};
            const auto pyth = detectTuning(histogramOf(makeFitGrid(pythagoreanC)));
            expect(pyth.getFit().system == TuningSystemType::Pythagorean, pyth.getTableLabel());
            expectEquals(pyth.getTableLabel(), String("3LJI C 440 Hz Pentagon"));

            // A 12TET tune fits every just intonation in part; 12TET must still lead.
            const auto tet = detectTuning(histogramOf(makeEqualTemperamentGrid(436.0, tuningGridClock)));
            expect(tet.getFit().system == TuningSystemType::EqualTemperament);
            expectEquals(static_cast<int>(tet.fits.size()), static_cast<int>(std::size(fittedSystems)));
        }

        beginTest("A fit at another clock keeps its periods and renames its notes");
        {
            const double st = uZX::ChipClockEnum::clockValues[uZX::ChipClockEnum::Atari_ST_2_MHz];
            const TuningFit justE{TuningSystemType::Just5Limit, Scale::Tonic::E, 436.0, tuningGridClock};
            const auto there = atClock(justE, st);
            expectEquals(static_cast<int>(there.tonic.asUnderlying()), 6, "two semitones up at the Atari ST clock");
            const auto before = makeFitGrid(justE), after = makeFitGrid(there);
            for (const auto& [period, note] : before)
                if (note >= 36 && note < 96) {
                    const auto it = after.find(period);
                    expect(it != after.end() && it->second == note + 2, "period " + String(period));
                }
            const auto back = atClock(there, tuningGridClock);
            expectEquals(static_cast<int>(back.tonic.asUnderlying()), 4);
            expectWithinAbsoluteError(back.a4Frequency, 436.0, 1e-6);
        }

        beginTest("Clock warning: the tuning's clock @ the chip's, and how far the notes sound off");
        {
            using Clock = uZX::ChipClockEnum;
            const double zx = Clock::clockValues[Clock::ZX_Spectrum_1_77_MHz];
            const double pentagon = Clock::clockValues[Clock::Pentagon_1_75_MHz];
            const double st = Clock::clockValues[Clock::Atari_ST_2_MHz];
            const String warn = String::fromUTF8("\xE2\x9A\xA0 "), cent = String::fromUTF8("\xC2\xA2");
            expectEquals(getClockMismatchText(pentagon, zx), warn + "Pentagon @ ZX Spectrum +23" + cent);
            expectEquals(getClockMismatchText(st, zx), warn + "Atari ST @ ZX Spectrum -208" + cent);
            expectEquals(getClockMismatchText(pentagon, 1.79 * uZX::MHz), warn + "Pentagon @ 1.7900 MHz +39" + cent);
            expect(getClockMismatchText(zx, zx).isEmpty());
            expect(getClockMismatchText(zx, 1.7734 * uZX::MHz).isEmpty(), "the AY plugin's MHz must still match");
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

        beginTest("A histogram without notes detects nothing");
        {
            const auto result = detectTuning({});
            expect(!result.hasNotes);
            expect(result.tables.empty());
            expect(result.grid.empty());
            expect(!result.tableTie && !result.keyTie);
        }

        beginTest("Envelope pitches whose window spans a semitone are left out of the share");
        {
            expect(!PitchHistogram::isEnvelopeDiscriminating(16));   // envelope period 1: 8..24
            expect(!PitchHistogram::isEnvelopeDiscriminating(256));  // 248..264 is still a semitone
            expect(PitchHistogram::isEnvelopeDiscriminating(1024));
            const auto& grid = getBuiltinPeriodGrid(BuiltinTuningType::CustomPT_1_ST);
            PitchHistogram hist;
            hist.tone[grid.rbegin()->first] = 10;  // one exact tone hit
            hist.envelope[16] = 30;                // would match any grid
            expectWithinAbsoluteError(hist.getShareOnGrid(grid), 1.0f, 0.001f);
            PitchHistogram onlyAmbiguous;
            onlyAmbiguous.envelope[16] = 30;
            expectWithinAbsoluteError(onlyAmbiguous.getShareOnGrid(grid), 0.0f, 0.001f);
            const auto bins = makeFoldedPitchHistogram(onlyAmbiguous, grid, 5);
            expect(std::all_of(bins.begin(), bins.end(), [](float b) { return b == 0.0f; }),
                   "an octave-wide band carries no pitch class");
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
            if (!dir.isDirectory()) {
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
                {"04_Igoval", BuiltinTuningType::CustomPT_2_ASM33, false, "A minor"},  // 12TET 440 Hz at ZX
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

                // The application path: import into a clip, where empty frames are not
                // stored. Its histogram must equal the one counted from the raw stream.
                {
                    uZX::PsgFile psg(files.getFirst());
                    auto edit = te::Edit::createSingleTrackEdit(*te::Engine::getEngines()[0]);
                    const auto length =
                        te::TimeDuration::fromSeconds(psg.ensureRead() ? psg.getData().getLengthSeconds() : 0.0);
                    auto clip =
                        PsgClip::insertTo(*te::getAudioTracks(*edit)[0], psg, {{te::TimePosition(), length}, {}});
                    expect(clip != nullptr);
                    if (clip != nullptr) {
                        const auto imported = clip->getPitchHistogram();
                        expect(imported.tone == hist.tone && imported.envelope == hist.envelope,
                               String(e.prefix) + " import path histogram");
                    }
                }
                logMessage(String(e.prefix) + ": tone-frames " + String(sumFrames(hist.tone)) + ", env-frames "
                           + String(sumFrames(hist.envelope)));
                expect(result.usesFit == e.tet, String(e.prefix) + " fallback");
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
            expectWithinAbsoluteError(factor6.getFit().a4Frequency, 436.0, 1.0);
            expect(factor6.keyTie);
        }
    }

    /** Development report, run only when UZX_TUNING_CORPUS names a folder of PSG files:
        one tab-separated "TUNING" line per file with the chosen grid, its share, the key,
        the tie flags and the pitch-class histogram, and a "FITS" line with every fitted
        system's label and share, best first. scripts/validate_tuning_detection.py
        compares it against PT3 ground truth. */
    void runCorpusReport() {
        const auto dirName = SystemStats::getEnvironmentVariable("UZX_TUNING_CORPUS", {});
        if (dirName.isEmpty())
            return;
        beginTest("Corpus report from UZX_TUNING_CORPUS");
        for (const auto& file : File(dirName).findChildFiles(File::findFiles, false, "*.psg")) {
            const auto r = detectTuning(histogramOfFile(file));
            const String grid =
                r.usesFit ? r.getTableLabel() : String(std::string(r.tables.front().table.getEnumName()));
            StringArray pcs;
            for (auto pc : r.pitchClasses)
                pcs.add(String(pc, 4));
            logMessage("TUNING\t" + file.getFileNameWithoutExtension() + "\t" + grid + "\t"
                       + String(r.getFit().a4Frequency, 1) + "\t" + String(r.share, 3) + "\t"
                       + String((int) r.tableTie) + "\t" + String(static_cast<int>(r.getKey().tonic)) + "\t"
                       + String((int) r.getKey().minor) + "\t" + String((int) r.keyTie) + "\t"
                       + pcs.joinIntoString(","));
            StringArray fits;
            for (const auto& fit : r.fits)
                fits.add(getFitLabel(fit) + " " + String(fit.share, 3));
            logMessage("FITS\t" + file.getFileNameWithoutExtension() + "\t" + fits.joinIntoString("\t"));
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
        PitchHistogram::Builder builder;
        if (!psg.ensureRead())
            return builder.build();
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
            builder.addFrame(params);
        }
        return builder.build();
    }
};

static TuningDetectorTest tuningDetectorTest;
