#include "PsgTempoDetector.h"

#include <JuceHeader.h>

#include <numeric>

#include "PsgClip.h"
#include "formats/psg/PsgData.h"
#include "formats/psg/PsgFile.h"
#include "models/EditUtilities.h"
#include "models/PsgFrameRetimer.h"
#include "formats/tracker/TrackerModule.h"
#include "models/PsgRowEvidence.h"

namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion;
using namespace tracktion::literals;
using namespace juce;
using namespace MoTool::uZX;

//==============================================================================
class PsgTempoDetectorTests : public UnitTest
{
public:
    PsgTempoDetectorTests() : UnitTest("PsgTempo", "MoTool") {}

    /** A tracker-like dump: channel A plays a short note on every row, channel B
        accents every accentRows-th row. rowLengths cycles (e.g. {6, 7} for swing). */
    static PsgData makeTune(int numFrames, std::vector<int> rowLengths, int accentRows) {
        PsgData data;
        data.options = {50.0, 1};
        data.frames.resize((size_t) numFrames);
        int t = 0, row = 0;
        while (t < numFrames) {
            data.frames[(size_t) t].setTonePeriod(0, (uint16_t) (200 + (row % 4) * 20));
            data.frames[(size_t) t].setVolume(0, 15);
            if (row % accentRows == 0) {
                data.frames[(size_t) t].setTonePeriod(1, 400);
                data.frames[(size_t) t].setVolume(1, 14);
            }
            if (t + 2 < numFrames) {
                data.frames[(size_t) t + 2].setVolume(0, 0);
                data.frames[(size_t) t + 2].setVolume(1, 0);
            }
            t += rowLengths[(size_t) row % rowLengths.size()];
            ++row;
        }
        return data;
    }

    /** Like makeTune, plus a bar structure: a kick and beat hats on channel C noise
        and a bass line on channel B that repeat every bar. */
    static PsgData makeBarTune(int numFrames, int beatsPerBar, int rowsPerBeat = 4, int speed = 6) {
        auto data = makeTune(numFrames, {speed}, rowsPerBeat);
        data.frames[0].setMixer(0x1c);  // tone A and B, noise C
        data.frames[0].setVolume(1, 12);
        const int rowsPerBar = rowsPerBeat * beatsPerBar;
        for (int row = 0; row * speed < numFrames; ++row) {
            const auto t = (size_t) (row * speed);
            const int rowInBar = row % rowsPerBar;
            data.frames[t].setTonePeriod(1, (uint16_t) (1000 + 40 * rowInBar));
            if (rowInBar % rowsPerBeat == 0) {  // kick on the downbeat, hat on every other beat
                data.frames[t].setNoisePeriod(rowInBar == 0 ? 20 : 1);
                data.frames[t].setVolume(2, 15);
                if (t + 2 < (size_t) numFrames)
                    data.frames[t + 2].setVolume(2, 0);
            }
        }
        return data;
    }

    /** Prepends numFrames frames; audible ones hold a tone on channel C, as in a
        dump that started mid-row. */
    static PsgData withLeadIn(PsgData data, int numFrames, bool audible) {
        data.frames.insert(data.frames.begin(), (size_t) numFrames, PsgRegsFrame{});
        if (audible) {
            data.frames[0].setTonePeriod(2, 300);
            data.frames[0].setVolume(2, 8);
        }
        return data;
    }

    /** One chip of a Turbo Sound tune: of rows `speed` frames long, it plays every
        numChips-th one, starting at row `chip`; the chips together play every row. */
    static PsgData makeChip(int numFrames, int chip, int numChips, int speed = 6) {
        PsgData data;
        data.options = {50.0, 1};
        data.frames.resize((size_t) numFrames);
        for (int row = chip; row * speed < numFrames; row += numChips) {
            const auto t = (size_t) (row * speed);
            data.frames[t].setTonePeriod(0, (uint16_t) (200 + (row % 4) * 20));
            data.frames[t].setVolume(0, 15);
            if (t + 2 < (size_t) numFrames)
                data.frames[t + 2].setVolume(0, 0);
        }
        return data;
    }

    /** Inserts the dump as a clip and applies its tempo, as importing a file does. */
    static PsgClip::Ptr importTune(AudioTrack& track, PsgData& tune, const ClipPosition& pos, const String& name = {}) {
        auto clip = PsgClip::insertTo(track, tune, pos, name);
        PsgTempo::applyToEdit({clip.get()}, {&tune});
        return clip;
    }

    /** The frame every row starts at, as a tracker module reports its rows: lengths cycle
        from `first`. */
    static std::vector<size_t> rowsOf(int numFrames, std::vector<int> rowLengths, size_t first = 0) {
        std::vector<size_t> starts;
        for (size_t t = first, row = 0; t < (size_t) numFrames; t += (size_t) rowLengths[row++ % rowLengths.size()])
            starts.push_back(t);
        return starts;
    }

    static String rowPattern(const std::vector<int>& rowFrames) {
        StringArray rows;
        for (int row : rowFrames)
            rows.add(String(row));
        return rows.joinIntoString("-");
    }

    void runTest() override {
        beginTest("Speed 6, accent every 4 rows: 24 frames per beat at 125 BPM");
        {
            auto result = PsgTempo::detect(makeTune(3000, {6}, 4));
            expect(static_cast<bool>(result));
            expect(result->rowFrames == std::vector<int>{6});
            expectEquals(result->rowsPerBeat, 4);
            expectEquals(result->framesPerBeat, 24);
            expectWithinAbsoluteError(result->bpm, 125.0, 0.01);
            expect(result->subdivisionPattern() == std::vector<int>{6, 6, 6, 6});
        }

        beginTest("Speed 3, accent every 8 rows: rows per beat doubles to stay in range");
        {
            auto result = PsgTempo::detect(makeTune(3000, {3}, 8));
            expect(static_cast<bool>(result));
            expect(result->rowFrames == std::vector<int>{3});
            expectEquals(result->rowsPerBeat, 8);
            expectEquals(result->framesPerBeat, 24);
            expect(result->subdivisionPattern() == std::vector<int>{6, 6, 6, 6}, "8 rows of 3 show as 4 groups");
        }

        beginTest("6-7 swing gives a 6 7 6 7 pattern, 26 frames per beat");
        {
            auto result = PsgTempo::detect(makeTune(3000, {6, 7}, 4));
            expect(static_cast<bool>(result));
            expect(result->rowFrames == std::vector<int>{6, 7});
            expectEquals(result->framesPerBeat, 26);
            expect(result->subdivisionPattern() == std::vector<int>{6, 7, 6, 7});
        }

        beginTest("A 4-4-3-3 groove gives 28 frames per beat at 107 BPM");
        {
            auto result = PsgTempo::detect(makeTune(4000, {4, 4, 3, 3}, 8));
            expect(static_cast<bool>(result));
            expect(result->rowFrames == std::vector<int>{4, 4, 3, 3});
            expectEquals(result->framesPerBeat, 28);
            expectWithinAbsoluteError(result->bpm, 3000.0 / 28.0, 0.01);
            expect(result->subdivisionPattern() == std::vector<int>{8, 6, 8, 6}, "rows are paired into 4 groups");
        }

        beginTest("Swing with triplet accents still gets an even number of rows per beat");
        {
            auto result = PsgTempo::detect(makeTune(3000, {6, 7}, 3));
            expect(static_cast<bool>(result));
            expectEquals(result->rowsPerBeat % 2, 0, "a swing pair must not split across beats");
            expectEquals(result->framesPerBeat, 13 * result->rowsPerBeat / 2);
        }

        beginTest("Rows are grouped into beats of 3, 4 or 5 by what repeats");
        {
            auto triplet = PsgTempo::detect(makeBarTune(4000, 4, 3, 3));
            expect(static_cast<bool>(triplet));
            expectEquals(triplet->rowsPerBeat, 6, "3-row groups at speed 3, doubled into range");
            expect(triplet->subdivisionPattern() == std::vector<int>{6, 6, 6}, "a triplet beat shows 3 groups");

            auto quintuplet = PsgTempo::detect(makeBarTune(6000, 4, 5, 3));
            expect(static_cast<bool>(quintuplet));
            expectEquals(quintuplet->rowsPerBeat, 10, "5-row groups at speed 3, doubled into range");
            expectWithinAbsoluteError(quintuplet->bpm, 100.0, 0.01);
            expect(quintuplet->subdivisionPattern() == std::vector<int>{6, 6, 6, 6, 6}, "10 rows show as 5 groups");
        }

        beginTest("A beat of 7 rows is found by how alike the tune is to itself a beat on");
        {
            // Nothing in the drums or the bass says 7; the accents come back every 21 frames.
            auto result = PsgTempo::detect(makeTune(6000, {3}, 7));
            expect(static_cast<bool>(result));
            expectEquals(result->rowsPerBeat, 7);
            expectEquals(result->framesPerBeat, 21);
        }

        beginTest("Plain rows with a stray note start between them are not a groove");
        {
            // Speed 4 on channels A and C. In every strayEvery-th round of three rows, channel B
            // starts a note two frames into the second row: frames 0, 4, 6 and 8 of 12 then hold
            // everything, which looks like a 4-2-2-4 groove, and a beat of 24 frames for one of 32.
            const auto build = [](int strayEvery, int numFrames) {
                PsgData data;
                data.options = {50.0, 1};
                data.frames.resize((size_t) numFrames);
                data.frames[0].setMixer(0x38);
                for (int t = 0; t + 2 < numFrames; t += 4) {
                    data.frames[(size_t) t].setTonePeriod(0, 300);
                    data.frames[(size_t) t].setVolume(0, 15);
                    data.frames[(size_t) t].setTonePeriod(2, 500);
                    data.frames[(size_t) t].setVolume(2, 13);
                    data.frames[(size_t) t + 2].setVolume(0, 0);
                    data.frames[(size_t) t + 2].setVolume(2, 0);
                }
                for (int t = 12 * strayEvery + 6; t + 2 < numFrames; t += 12 * strayEvery) {
                    data.frames[(size_t) t].setTonePeriod(1, 200);
                    data.frames[(size_t) t].setVolume(1, 15);
                    data.frames[(size_t) t + 2].setVolume(1, 0);
                }
                return data;
            };
            // The strays must hold more than rows a frame apart may lose (1.5%) and less than a
            // row of a groove starts (2%): spaced by what a row and a stray weigh as evidence.
            const auto probe = PsgRowEvidence::of(build(1, 240));
            const double row = probe[24], stray = probe[18];
            expect(row > 0.0 && stray > 0.0, "both count as evidence");
            const int strayEvery = juce::roundToInt(stray * (1.0 - 0.0175) / (0.0175 * 3.0 * row));
            expect(strayEvery >= 2);

            auto result = PsgTempo::detect(build(strayEvery, 12 * strayEvery * 40));
            expect(static_cast<bool>(result));
            expectEquals((int) result->rowFrames.size(), 1, rowPattern(result->rowFrames));
            expectEquals(result->framesPerBeat, 32);
        }

        beginTest("A slow triplet beat keeps 3 rows instead of halving to 1");
        {
            auto result = PsgTempo::detect(makeBarTune(8000, 4, 3, 16));
            expect(static_cast<bool>(result));
            expectEquals(result->rowsPerBeat, 3);
            expectEquals(result->framesPerBeat, 48);
        }

        beginTest("The first row start and lead-in silence are reported");
        {
            auto plain = PsgTempo::detect(makeTune(3000, {6}, 4));
            expectEquals(plain->firstRowFrame, 0);

            auto silent = PsgTempo::detect(withLeadIn(makeTune(3000, {6}, 4), 5, false));
            expectEquals(silent->firstRowFrame, 5);
            expect(silent->silentLeadIn);

            auto audible = PsgTempo::detect(withLeadIn(makeTune(3000, {6}, 4), 5, true));
            expectEquals(audible->firstRowFrame, 5);
            expect(!audible->silentLeadIn);

            // an intro at speed 7 before the speed-6 tune: the opening doesn't follow the grid
            auto intro = makeTune(3000, {6}, 4);
            auto introRows = makeTune(5 + 7 * 30, {7}, 4);
            introRows.frames.erase(introRows.frames.begin(), introRows.frames.begin() + 2);
            intro.frames.insert(intro.frames.begin(), introRows.frames.begin(), introRows.frames.end());
            auto withIntro = PsgTempo::detect(intro);
            expect(static_cast<bool>(withIntro));
            expect(withIntro->rowFrames == std::vector<int>{6});
            expectEquals(withIntro->firstRowFrame, 0, "leave the clip alone when the opening is off the grid");
        }

        beginTest("Bars of 3 beats give 3/4, bars of 4 beats give 4/4");
        {
            auto triple = PsgTempo::detect(makeBarTune(4000, 3));
            expect(static_cast<bool>(triple));
            expectEquals(triple->rowsPerBeat, 4);
            expectEquals(triple->beatsPerBar, 3);

            expect(triple->meterClear);

            auto quadruple = PsgTempo::detect(makeBarTune(4000, 4));
            expect(static_cast<bool>(quadruple));
            expectEquals(quadruple->beatsPerBar, 4);
            expect(quadruple->meterClear);

            auto noBars = PsgTempo::detect(makeTune(3000, {6}, 4));
            expectEquals(noBars->beatsPerBar, 4, "no bar-level repetition keeps 4/4");
        }

        beginTest("Rows that slip a frame now and then hold no grid, but the tune keeps its beat");
        {
            // Speed 6, and every 40th row lasts 7 frames: the rows drift through every phase.
            std::vector<int> slipping(40, 6);
            slipping.back() = 7;
            auto result = PsgTempo::detect(makeTune(6000, slipping, 4));
            expect(static_cast<bool>(result));
            if (result) {
                expect(result->rowsArePulse, "found by what repeats, not as rows");
                expectEquals(result->framesPerBeat, 24);
                expectWithinAbsoluteError(result->bpm, 125.0, 0.01);
                expectEquals(result->firstRowFrame, 0, "nothing says where the first row is");
            }
            expect(!PsgTempo::detect(makeTune(6000, {6}, 4))->rowsArePulse, "the same tune without slips has its rows");
        }

        beginTest("A pulse that is not a whole number of frames: the beat follows where the tune repeats");
        {
            // rows of 4 frames, every eighth one of 3, none accented: the tune pulses every 4 frames and
            // goes round in 31, which no multiple of 4 reaches
            auto result = PsgTempo::detect(makeTune(6000, {4, 4, 4, 4, 4, 4, 4, 3}, 6000));
            expect(result.hasValue());
            if (result) {
                expect(result->rowsArePulse, "no row grid holds");
                expectEquals(result->framesPerBeat, 31);
                expectWithinAbsoluteError(result->bpm, 3000.0 / 31.0, 0.01);
                // and with the beat known, the rows inside it are read off the tune, stretch by stretch
                expect(result->rowFrames == std::vector<int>({4, 4, 4, 4, 4, 4, 4, 3}), rowPattern(result->rowFrames));
                expectEquals(result->rowsPerBeat, 8);
                expect(result->subdivisionPattern() == std::vector<int>({8, 8, 8, 7}), "the groove the grid shows");
            }

            // Every second row accented, the short round now and then a frame longer: only the accents stand
            // out, and the rows are read as the pulses they make
            std::vector<int> rounds;
            for (int round = 0; round < 200; ++round)
                for (int row : {4, 4, 4, 4, 4, 4, 4, round % 5 == 4 ? 4 : 3})
                    rounds.push_back(row);
            auto slipping = PsgTempo::detect(makeTune(6000, rounds, 2));
            expect(slipping.hasValue());
            if (slipping) {
                expectEquals(slipping->framesPerBeat, 31);
                expectEquals(slipping->rowsPerBeat, 4, rowPattern(slipping->rowFrames));
                auto pulses = slipping->rowFrames;
                std::sort(pulses.begin(), pulses.end());
                expect(pulses == std::vector<int>({7, 8, 8, 8}), "8-8-8-7 at some turn");
            }
        }

        beginTest("A module's own rows replace the grid detection");
        {
            // A swing as wide as speeds 2 and 6: detection only takes rows up to 2 frames apart.
            auto wide = makeTune(3000, {2, 6}, 4);
            auto known = PsgTempo::detect({&wide}, rowsOf(3000, {2, 6}));
            expect(static_cast<bool>(known));
            expect(known->rowFrames == std::vector<int>{2, 6}, rowPattern(known->rowFrames));
            // 4 rows (16 frames) would be 187.5 BPM: 8 rows, 4 whole swing pairs, make the beat.
            expectEquals(known->rowsPerBeat, 8);
            expectEquals(known->framesPerBeat, 32);
            expectWithinAbsoluteError(known->gridShare, 1.0, 1e-9);

            // An 8-row groove (Bad Apple!! by nq) keeps its 8 rows in one beat of 21 frames.
            const std::vector<int> groove {3, 3, 3, 2, 3, 2, 3, 2};
            auto grooved = makeTune(3000, groove, 8);
            auto eight = PsgTempo::detect({&grooved}, rowsOf(3000, groove));
            expect(eight->rowFrames == groove, rowPattern(eight->rowFrames));
            expectEquals(eight->rowsPerBeat, 8);
            expectEquals(eight->framesPerBeat, 21);
            expect(eight->subdivisionPattern() == std::vector<int>{6, 5, 5, 5}, "4 groups of 2 rows");

            // An intro of 10 rows at speed 7 before speed 6: the module's first row is still the downbeat.
            auto tune = makeTune(3000, {6}, 4);
            auto rows = rowsOf(70, {7});
            for (auto start : rowsOf(3000 - 70, {6}))
                rows.push_back(70 + start);
            auto withIntro = PsgTempo::detect({&tune}, rows);
            expect(withIntro->rowFrames == std::vector<int>{6}, "the speed 90% of the rows have");
            expectEquals(withIntro->firstRowFrame, 0);

            // An intro at speed 4 before a 5-5-3-3 groove (Hibernation by MmcM): under 90% of the rows
            // follow the groove by count, but every 16 frames a round starts, intro included.
            auto afterIntro = rowsOf(160, {4});
            for (auto start : rowsOf(960, {5, 5, 3, 3}))
                afterIntro.push_back(160 + start);
            auto long16 = makeTune(160 + 960, {4}, 4);
            auto intro = PsgTempo::detect({&long16}, afterIntro);
            expect(intro->rowFrames == std::vector<int>{5, 5, 3, 3}, rowPattern(intro->rowFrames));
            expectEquals(intro->framesPerBeat, 32);

            // A groove that varies (Not Picking Up by Pator): every fourth 3-5-3-3 is a 4-4-3-3, so no
            // row pattern repeats, but the tune is made of 14-frame rounds.
            const std::vector<int> varied {3, 5, 3, 3, 3, 5, 3, 3, 3, 5, 3, 3, 4, 4, 3, 3};
            auto variedTune = makeTune(2800, {4}, 4);
            auto variedRows = PsgTempo::detect({&variedTune}, rowsOf(2800, varied));
            expect(variedRows->rowFrames == std::vector<int>{3, 5, 3, 3}, rowPattern(variedRows->rowFrames));
            expectEquals(variedRows->framesPerBeat, 28);

            // Two speeds, neither for 90% of the rows, share no pulse: the commoner one stands.
            auto twoSpeeds = rowsOf(700, {7});
            for (auto start : rowsOf(1350, {9}))
                twoSpeeds.push_back(700 + start);
            auto twoTune = makeTune(700 + 1350, {4}, 4);
            auto sections = PsgTempo::detect({&twoTune}, twoSpeeds);
            expect(sections->rowFrames == std::vector<int>{9}, rowPattern(sections->rowFrames));

            // A grid needs no notes once the module gives its rows; one row is no grid.
            auto quiet = makeTune(60, {6}, 4);
            expect(static_cast<bool>(PsgTempo::detect({&quiet}, rowsOf(60, {6}))));
            expect(!PsgTempo::detect({&quiet}, {0}), "a single row falls back to detection");
        }

        beginTest("A module whose rows follow no pattern: its beat lasts as long as that many rows usually do");
        {
            // Rounds of eight rows with one of 3 frames, at a place of its own in each, and every fourth round
            // with two: no pattern of 1, 2, 4 or 8 rows holds and no pulse does, most rows are 4 frames, yet
            // eight rows usually last 31.
            const std::vector<std::vector<int>> rounds{
                {4, 4, 4, 4, 4, 4, 4, 3}, {4, 4, 4, 3, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 3, 4, 4}, {4, 4, 3, 4, 4, 4, 3, 4}};
            std::vector<int> rows;
            for (const auto& round : rounds)
                rows.insert(rows.end(), round.begin(), round.end());
            auto tune = makeTune(6000, rows, 8);
            auto result = PsgTempo::detect({&tune}, rowsOf(6000, rows));
            expect(result.hasValue());
            if (result) {
                // six or eight rows to the beat, and one of them is the short one
                expectEquals(result->framesPerBeat, 4 * result->rowsPerBeat - 1, "not so many times the commonest row");
                expect(std::find(result->rowFrames.begin(), result->rowFrames.end(), 3) != result->rowFrames.end(),
                       rowPattern(result->rowFrames));
            }
        }

        beginTest("A module's patterns are bars: its beat and its meter fill them");
        {
            // Speed 3 with accents every third row reads as beats of 6 rows; what the patterns hold decides
            auto tune = makeBarTune(4000, 4, 3, 3);
            const auto rows = rowsOf(4000, {3});
            auto patternsOf = [&](size_t rowsEach) {
                std::vector<size_t> starts;
                for (size_t row = 0; row < rows.size(); row += rowsEach)
                    starts.push_back(rows[row]);
                return starts;
            };
            auto free = PsgTempo::detect({&tune}, rows);
            expectEquals(free->rowsPerBeat, 6, "by what repeats, without patterns");
            auto fits = PsgTempo::detect({&tune}, rows, patternsOf(48));
            expectEquals(fits->rowsPerBeat, 6, "48 rows hold 8 beats of 6 rows");
            auto vetoed = PsgTempo::detect({&tune}, rows, patternsOf(64));
            expectEquals(vetoed->rowsPerBeat, 8, "64 rows hold no whole number of 6-row beats");
            expectEquals(vetoed->framesPerBeat, 24);
            expect(vetoed->meterClear && vetoed->beatsPerBar == 4, "8 beats a pattern: bars of 4, not of 3");

            // Bars of 3 and of 4 beats both fill a pattern of 12 beats: the meter stays what the repetition says
            auto plain = makeTune(3000, {6}, 4);
            const auto plainRows = rowsOf(3000, {6});
            std::vector<size_t> twelveBeats;
            for (size_t row = 0; row < plainRows.size(); row += 48)
                twelveBeats.push_back(plainRows[row]);
            auto either = PsgTempo::detect({&plain}, plainRows, twelveBeats);
            expect(! either->meterClear, "an unclear meter is not made clear by patterns both fill");
        }

        beginTest("A module with a pickup: its bars start where most of its patterns do");
        {
            // Speed 6, 4 rows to the beat. The first pattern is one beat long, the rest 8 beats each.
            auto tune = makeTune(3000, {6}, 4);
            const auto rows = rowsOf(3000, {6});
            std::vector<size_t> patterns{rows[0]};
            for (size_t row = 4; row < rows.size(); row += 32)
                patterns.push_back(rows[row]);
            auto result = PsgTempo::detect({&tune}, rows, patterns);
            expect(result.hasValue());
            if (result) {
                expectEquals(result->framesPerBeat, 24);
                expectEquals(result->firstRowFrame, 24, "the first bar line comes after the pickup beat");
                expect(! result->silentLeadIn, "the pickup sounds");
            }
            // Patterns that all start on the first row's place in the bar leave it the downbeat
            std::vector<size_t> plain;
            for (size_t row = 0; row < rows.size(); row += 32)
                plain.push_back(rows[row]);
            expectEquals(PsgTempo::detect({&tune}, rows, plain)->firstRowFrame, 0);

            // On import the clip stays on its bar line and starts with rests, so the bar line after the pickup is bar 2
            auto& engine = *tracktion::Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit(engine);
            const auto length = tracktion::TimeDuration::fromSeconds(tune.getLengthSeconds());
            auto clip = PsgClip::insertTo(*getAudioTracks(*edit)[0], tune, {{0_tp, length}, {}}, "pickup");
            PsgTempo::applyToEdit({clip.get()}, {&tune}, rows, patterns);
            expectWithinAbsoluteError(clip->getPosition().getStart().inSeconds(), 0.0, 1.0e-9);
            expectEquals(clip->getLeadInFrames(), 96 - 24);
            expectWithinAbsoluteError(PsgList::getEditTime(*clip, 24).inSeconds(), 96.0 / 50.0, 1.0e-6, "bar 2");
            expectWithinAbsoluteError(clip->getPosition().getLength().inSeconds(), (3000.0 + 72.0) / 50.0, 1.0e-6);
        }

        beginTest("Too few notes or no grid gives no result");
        {
            expect(!PsgTempo::detect(makeTune(60, {6}, 4)));

            PsgData noise;
            noise.options = {50.0, 1};
            Random random(42);
            for (int i = 0; i < 3000; ++i) {
                PsgRegsFrame frame;
                frame.setVolume(0, (uint8_t) (random.nextBool() ? 15 : 0));
                frame.setTonePeriod(0, (uint16_t) random.nextInt(4096));
                noise.frames.push_back(frame);
            }
            expect(!PsgTempo::detect(noise));
        }

        beginTest("Importing into an empty edit sets tempo and grid; a second clip keeps them");
        {
            auto& engine = *tracktion::Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit(engine);
            auto* track = getAudioTracks(*edit)[0];
            auto tune = makeTune(3000, {6}, 4);
            const tracktion::ClipPosition pos{{0_tp, tracktion::TimeDuration::fromSeconds(tune.getLengthSeconds())},
                                              {}};

            auto clip = importTune(*track, tune, pos, "first");
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), 125.0, 0.01);
            expectWithinAbsoluteError(clip->getPsg().getFramesPerBeat(), 24.0, 0.01);
            expect(Helpers::getGridSubdivisionPattern(*edit, 24) == std::vector<int>{6, 6, 6, 6});
            expectEquals(edit->tempoSequence.getTimeSig(0)->numerator.get(), 4);

            auto waltzEdit = Edit::createSingleTrackEdit(engine);
            auto waltz = makeBarTune(4000, 3);
            importTune(*getAudioTracks(*waltzEdit)[0],
                              waltz,
                              {{0_tp, tracktion::TimeDuration::fromSeconds(waltz.getLengthSeconds())}, {}},
                              "waltz");
            expectEquals(waltzEdit->tempoSequence.getTimeSig(0)->numerator.get(), 3);
            expectEquals(waltzEdit->tempoSequence.getTimeSig(0)->denominator.get(), 4);

            auto second = makeTune(3000, {5}, 4);
            importTune(
                *track,
                second,
                {{tracktion::TimePosition::fromSeconds(70.0), tracktion::TimeDuration::fromSeconds(60.0)}, {}},
                "second");
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), 125.0, 0.01);
        }

        beginTest("A clip's register dump detects as the file it was imported from");
        {
            auto& engine = *tracktion::Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit(engine);
            auto tune = makeTune(3000, {4, 4, 3, 3}, 8);
            const auto length = tracktion::TimeDuration::fromSeconds(tune.getLengthSeconds());
            auto clip = PsgClip::insertTo(*getAudioTracks(*edit)[0], tune, {{0_tp, length}, {}}, "tune");
            const auto dump = clip->getRegisterDump();
            const auto fromFile = PsgTempo::analyse({&tune}), fromClip = PsgTempo::analyse({&dump});
            // the dump ends with the last frame that changes a register; the file may go on in silence
            expect(dump.frames.size() <= tune.frames.size() && dump.frames.size() + 8 > tune.frames.size());
            expect(std::equal(fromClip.evidence.begin(), fromClip.evidence.end(), fromFile.evidence.begin()),
                   "the row evidence is the same frame by frame");
            expect(fromClip.result && fromClip.result->rowFrames == fromFile.result->rowFrames);
            expectWithinAbsoluteError(fromClip.result->bpm, fromFile.result->bpm, 1e-6);
            expect(fromFile.rowsFrom == PsgTempo::Analysis::RowsFrom::evidence);
            expect(! fromFile.grids.empty() && ! fromFile.beats.empty(), "what was tried is listed");
        }

        beginTest("Importing into a project that already has clips keeps its tempo and meter");
        {
            auto& engine = *tracktion::Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit(engine);
            auto* track = getAudioTracks(*edit)[0];
            track->insertMIDIClip("arrangement", {0_tp, 8_tp}, nullptr);
            edit->tempoSequence.getTimeSig(0)->numerator = 3;
            auto tune = makeTune(3000, {6}, 4);
            importTune(
                *track, tune, {{10_tp, tracktion::TimeDuration::fromSeconds(tune.getLengthSeconds())}, {}}, "psg");
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), 120.0, 0.01);
            expectEquals(edit->tempoSequence.getTimeSig(0)->numerator.get(), 3);
        }

        beginTest("The tempo is set where the clip starts, not at the transport");
        {
            auto& engine = *tracktion::Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.insertTempo(tracktion::BeatPosition::fromBeats(64), 90.0, 0.0f);
            edit->getTransport().setPosition(tracktion::TimePosition::fromSeconds(60.0));
            auto tune = makeTune(3000, {6}, 4);
            importTune(*getAudioTracks(*edit)[0],
                              tune,
                              {{0_tp, tracktion::TimeDuration::fromSeconds(tune.getLengthSeconds())}, {}},
                              "psg");
            expectWithinAbsoluteError(edit->tempoSequence.getTempo(0)->getBpm(), 125.0, 0.01);
            expectWithinAbsoluteError(edit->tempoSequence.getTempo(1)->getBpm(), 90.0, 0.01);
        }

        beginTest("An unclear meter leaves a user-set time signature alone");
        {
            auto& engine = *tracktion::Engine::getEngines()[0];
            auto unclear = makeTune(3000, {6}, 4);
            expect(!PsgTempo::detect(unclear)->meterClear, "the plain tune has no bar-level repetition");
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTimeSig(0)->numerator = 3;
            importTune(*getAudioTracks(*edit)[0],
                              unclear,
                              {{0_tp, tracktion::TimeDuration::fromSeconds(unclear.getLengthSeconds())}, {}},
                              "psg");
            expectEquals(edit->tempoSequence.getTimeSig(0)->numerator.get(), 3);
        }

        beginTest("Import puts the first row on a bar line: silence is trimmed, a mid-row start is shifted");
        {
            auto& engine = *tracktion::Engine::getEngines()[0];
            auto firstRowTime = [](PsgClip& clip) {
                for (auto* frame : clip.getPsg().getFrames())
                    if (frame->getFrameIndex() == 5)
                        return frame->getEditTime(clip);
                return tracktion::TimePosition::fromSeconds(-1.0);
            };
            auto import = [&](PsgData tune) {
                auto edit = Edit::createSingleTrackEdit(engine);
                const tracktion::ClipPosition pos{{0_tp, tracktion::TimeDuration::fromSeconds(tune.getLengthSeconds())},
                                                  {}};
                auto clip = importTune(*getAudioTracks(*edit)[0], tune, pos, "lead-in");
                return std::pair{std::move(edit), clip};
            };

            auto [silentEdit, trimmed] = import(withLeadIn(makeTune(3000, {6}, 4), 5, false));
            expectWithinAbsoluteError(trimmed->getPosition().getStart().inSeconds(), 0.0, 1.0e-9);
            expectWithinAbsoluteError(trimmed->getPosition().getOffset().inSeconds(), 0.1, 1.0e-9);
            expectWithinAbsoluteError(firstRowTime(*trimmed).inSeconds(), 0.0, 1.0e-6);

            auto [audibleEdit, shifted] = import(withLeadIn(makeTune(3000, {6}, 4), 5, true));
            // 4/4 at 24 frames per beat: a bar is 96 frames, so the clip starts with 91 frames of rests
            expectWithinAbsoluteError(shifted->getPosition().getStart().inSeconds(), 0.0, 1.0e-9);
            expectEquals(shifted->getLeadInFrames(), 91);
            // the rests are the clip's own: they stay when it moves, and its playback starts after them
            auto played = shifted->createPlaybackMidiSequence(tracktion::MidiList::TimeBase::seconds);
            double firstNote = -1.0;
            for (int i = 0; i < played.getNumEvents() && firstNote < 0.0; ++i)
                if (played.getEventPointer(i)->message.getTimeStamp() > 1.0e-9)
                    firstNote = played.getEventPointer(i)->message.getTimeStamp();
            expectWithinAbsoluteError(firstNote, 91.0 / 50.0, 1.0e-6, "nothing plays in the rests");
            shifted->setStart(tracktion::TimePosition::fromSeconds(4.0), false, true);
            expectEquals(shifted->getLeadInFrames(), 91, "kept through a move");
            expectWithinAbsoluteError(firstRowTime(*shifted).inSeconds(), 4.0 + 96.0 / 50.0, 1.0e-6);
            shifted->setStart(0_tp, false, true);
            expectWithinAbsoluteError(shifted->getPosition().getOffset().inSeconds(), 0.0, 1.0e-9);
            const auto barsBeats = audibleEdit->tempoSequence.toBarsAndBeats(firstRowTime(*shifted));
            expectEquals(barsBeats.bars, 1, "first row starts bar 2");
            expectWithinAbsoluteError(barsBeats.beats.inBeats(), 0.0, 1.0e-6);
        }

        beginTest("The chips of a Turbo Sound tune are detected as one and move together");
        {
            std::vector<PsgData> chips;
            for (int chip = 0; chip < 3; ++chip)
                chips.push_back(makeChip(3000, chip, 3));
            const auto alone = PsgTempo::detect(chips[0]);
            expect(static_cast<bool>(alone) && alone->rowFrames != std::vector<int>{6}, "one chip alone misses rows");
            const auto together = PsgTempo::detect(std::vector<const PsgData*>{&chips[0], &chips[1], &chips[2]});
            expect(static_cast<bool>(together));
            if (together) {
                expect(together->rowFrames == std::vector<int>{6});
                expectEquals(together->framesPerBeat, 24);
            }

            // Only the second chip sounds before the first row: no chip's lead-in is silent.
            auto& engine = *tracktion::Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->ensureNumberOfAudioTracks(3);
            chips[0] = withLeadIn(chips[0], 5, false);
            chips[1] = withLeadIn(chips[1], 5, true);
            chips[2] = withLeadIn(chips[2], 5, false);
            juce::Array<PsgClip*> clips;
            std::vector<const PsgData*> data;
            for (int chip = 0; chip < 3; ++chip) {
                auto& tune = chips[(size_t) chip];
                const ClipPosition pos{{0_tp, TimeDuration::fromSeconds(tune.getLengthSeconds())}, {}};
                clips.add(PsgClip::insertTo(*getAudioTracks(*edit)[chip], tune, pos, "chip").get());
                data.push_back(&tune);
            }
            PsgTempo::applyToEdit(clips, data);
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), 125.0, 0.01);
            // Each chip plays every third row with noise on, so the meter may well read 3/4.
            const int barFrames = 24 * edit->tempoSequence.getTimeSig(0)->numerator.get();
            for (auto* clip : clips) {
                // every clip gets the same rests, so the first row (frame 5) starts bar 2
                expectWithinAbsoluteError(clip->getPosition().getStart().inSeconds(), 0.0, 1.0e-9);
                expectEquals(clip->getLeadInFrames(), barFrames - 5);
                expectWithinAbsoluteError(clip->getPosition().getOffset().inSeconds(), 0.0, 1.0e-9);
            }
        }

        testCorpusFixtures();
        testFrameRateChanges();
        runCorpusReport();
        runModuleRowsReport();
    }

    /** "200%" by Abstract 64, Natt, Pator and wbcbz7: a Turbo Sound tune at 200 Hz whose PSG
        files give no rate, so it opens at 50 Hz and four times too slow. */
    void testFrameRateChanges() {
        const auto dir = File(UZX_FIXTURES_DIR).getChildFile("psg");
        PsgFile first(dir.getChildFile("200_percent_partyfinal.1.psg"));
        PsgFile second(dir.getChildFile("200_percent_partyfinal.2.psg"));
        expect(first.ensureRead() && second.ensureRead(), "fixtures read");
        if (first.getData().frames.empty() || second.getData().frames.empty())
            return;
        auto& engine = *tracktion::Engine::getEngines()[0];
        const auto length = tracktion::TimeDuration::fromSeconds(first.getData().getLengthSeconds());

        /** Imports both chips at 50 Hz, as opening the files does. */
        const auto open = [&](Edit& edit) {
            auto a = PsgClip::insertTo(*getAudioTracks(edit)[0], first.getData(), {{0_tp, length}, {}}, "1");
            auto b = PsgClip::insertTo(*getAudioTracks(edit)[1], second.getData(), {{0_tp, length}, {}}, "2");
            PsgClip::linkAsOneTune({a.get(), b.get()});
            Helpers::setProjectFps(edit, 50.0);
            PsgTempo::applyToEdit({a.get(), b.get()}, {&first.getData(), &second.getData()});
            return juce::Array<PsgClip*>{a.get(), b.get()};
        };

        beginTest("Detection reads rows per beat at 50 Hz: the same rows at 100 Hz have twice the BPM");
        {
            const auto at = [&](double fps) {
                auto data = first.getData();
                data.options.frameRate = fps;
                return PsgTempo::detect(data);
            };
            const auto at50 = at(50.0), at100 = at(100.0), at200 = at(200.0);
            expect(at50 && at100 && at200, "detected at every rate");
            if (at50 && at100 && at200) {
                expectEquals(at50->rowsPerBeat, 2, "15-frame rows, 2 a beat");
                expectWithinAbsoluteError(at50->bpm, 100.0, 0.01);
                expectEquals(at100->rowsPerBeat, 2);
                expectWithinAbsoluteError(at100->bpm, 200.0, 0.01);
                expectEquals(at200->rowsPerBeat, 4, "400 BPM is past Tracktion's 300: a beat of twice the rows");
                expectWithinAbsoluteError(at200->bpm, 200.0, 0.01);
            }
        }

        beginTest("A tracker module's rows give the same rows per beat at 200 Hz");
        {
            MemoryBlock bytes;
            File(UZX_FIXTURES_DIR).getChildFile("modules/nq - Frontiers (2016).pt3").loadFileAsData(bytes);
            auto module = uZX::readTrackerModule(bytes);
            expect(module.has_value(), "Frontiers reads");
            if (module) {
                std::vector<const PsgData*> chips;
                for (const auto& chip : module->chips)
                    chips.push_back(&chip.psg);
                const auto at50 = PsgTempo::detect(chips, module->rowStarts);
                for (auto& chip : module->chips)
                    chip.psg.options.frameRate = 200.0;
                const auto at200 = PsgTempo::detect(chips, module->rowStarts);
                expect(at50 && at200, "detected at both rates");
                if (at50 && at200 && at50->bpm * 4.0 <= tracktion::TempoSetting::maxBPM) {
                    expectEquals(at200->rowsPerBeat, at50->rowsPerBeat);
                    expectWithinAbsoluteError(at200->bpm, at50->bpm * 4.0, 0.01);
                }
            }
        }

        beginTest("A tune alone in its project plays at the rate picked for it, and the project follows");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->ensureNumberOfAudioTracks(2);
            const auto clips = open(*edit);
            const auto bpm = edit->tempoSequence.getTempoAt(0_tp).getBpm();  // the two chips give no tempo

            PsgTempo::setFrameRate(clips[0]->getTuneClips(), 200.0);
            expectEquals(Helpers::getProjectFps(*edit), 200.0, "the project follows");
            for (auto* clip : clips) {
                expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), 200.0, 1e-6, "plays at 200 Hz");
                expectWithinAbsoluteError(clip->getPosition().getLength().inSeconds(), 185.78, 0.01, "3:05.78");
                expect(! clip->getPsg().hasFpsMismatch(*clip, 200.0), "no warning");
            }
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), bpm * 4.0 / 2.0, 0.01,
                                      "four times the tempo, halved under 300 BPM");
        }

        beginTest("A tune's tempo scales with its rate: a beat keeps its frames, the grid its groove");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            auto clip = PsgClip::insertTo(*getAudioTracks(*edit)[0], first.getData(), {{2_tp, length}, {}});
            Helpers::setProjectFps(*edit, 50.0);
            PsgTempo::applyToEdit({clip.get()}, {&first.getData()});
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), 100.0, 0.01, "100 BPM at 50 Hz");
            const auto groove = Helpers::getGridSubdivisionPattern(*edit, 30);

            PsgTempo::setFrameRate({clip.get()}, 100.0);
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), 200.0, 0.01, "twice the tempo");
            expectWithinAbsoluteError(PsgTiming::getFramesPerBeat(*edit), 30.0, 1e-6, "the same frames a beat");
            expect(Helpers::getGridSubdivisionPattern(*edit, 30) == groove, "the same groove");
            expectWithinAbsoluteError(clip->getPosition().getStart().inSeconds(), 1.0, 1e-9, "100 frames in, as before");

            PsgTempo::setFrameRate({clip.get()}, 200.0);
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), 200.0, 0.01,
                                      "400 BPM is past 300: a beat of twice the frames");
            expectWithinAbsoluteError(PsgTiming::getFramesPerBeat(*edit), 60.0, 1e-6);
            auto twice = groove;
            twice.insert(twice.end(), groove.begin(), groove.end());
            expect(Helpers::getGridSubdivisionPattern(*edit, 60) == twice, "the groove twice a beat");
        }

        beginTest("The project's fps retimes the clips that played at the old one");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->ensureNumberOfAudioTracks(3);
            const auto clips = open(*edit);
            auto other = PsgClip::insertTo(*getAudioTracks(*edit)[2], first.getData(), {{0_tp, length}, {}}, "other");
            PsgTiming::setClipFrameRate(*other, 60.0);
            const auto bpm = edit->tempoSequence.getTempoAt(0_tp).getBpm();

            PsgTempo::setProjectFrameRate(*edit, 200.0);
            expectEquals(Helpers::getProjectFps(*edit), 200.0);
            for (auto* clip : clips)
                expectWithinAbsoluteError(clip->getPsg().getEffectiveFps(*clip), 200.0, 1e-6, "followed");
            expectWithinAbsoluteError(other->getPsg().getEffectiveFps(*other), 60.0, 1e-6, "kept its own rate");
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), bpm, 0.01,
                                      "with other clips, the tempo stays");
        }

        beginTest("A tune among other clips changes alone: the project and its tempo stay");
        {
            auto edit = Edit::createSingleTrackEdit(engine);
            edit->ensureNumberOfAudioTracks(3);
            const auto clips = open(*edit);
            PsgClip::insertTo(*getAudioTracks(*edit)[2], first.getData(), {{0_tp, length}, {}}, "other");

            PsgTempo::setFrameRate(clips, 200.0);
            expectEquals(Helpers::getProjectFps(*edit), 50.0);
            expect(clips[0]->getPsg().hasFpsMismatch(*clips[0], 50.0), "the clip warns");
        }
    }

    /** Tempos set by ear in the uZX Player (scripts/psg_tempo_labels.csv) for tunes of the
        committed fixture collection, checked on the raw file and through import. */
    void testCorpusFixtures() {
        beginTest("Corpus fixtures detect the tempo heard by ear");
        const auto dir = File(UZX_FIXTURES_DIR).getChildFile("psg");
        if (!dir.isDirectory()) {
            logMessage("fixtures missing at " + dir.getFullPathName() + ", skipping");
            return;
        }
        struct Expected
        {
            const char* prefix;
            int framesPerBeat;
            std::vector<int> groove;  // empty: not checked
        };
        const Expected expected[] = {
            {"02_Mic", 24, {}},                     // 125 BPM; once misread as 3-row triplets
            {"04_Igoval", 20, {}},                  // 150 BPM
            {"05_Sauron", 20, {}},                  // 150 BPM
            {"11_Yerzmyey", 20, {}},                // 150 BPM
            {"24_nq_-_lmao", 24, {}},               // 125 BPM
            {"18_nq", 31, {}},                      // 96.8 BPM, rows 4-4-4-4-4-4-4-3: no multiple of its 8-frame pulse
            {"26_djnzx48", 28, {}},                 // 107 BPM, a 3-5-3-3 groove
            {"29_nq_-_tiratok", 26, {7, 6, 7, 6}},  // 115 BPM, 4-3-3-3 rows shown in pairs
        };
        for (const auto& e : expected) {
            const auto files = dir.findChildFiles(File::findFiles, false, String(e.prefix) + "*.psg");
            expectEquals(files.size(), 1, String("fixture ") + e.prefix);
            if (files.isEmpty())
                continue;
            PsgFile psg(files.getFirst());
            expect(psg.ensureRead(), String(e.prefix) + " reads");
            const auto result = PsgTempo::detect(psg.getData());
            expect(static_cast<bool>(result), String(e.prefix) + " has a tempo");
            if (!result)
                continue;
            expectEquals(result->framesPerBeat, e.framesPerBeat, String(e.prefix) + " frames per beat");
            if (!e.groove.empty())
                expect(result->subdivisionPattern() == e.groove, String(e.prefix) + " groove");

            auto edit = Edit::createSingleTrackEdit(*tracktion::Engine::getEngines()[0]);
            const tracktion::ClipPosition pos{
                {0_tp, tracktion::TimeDuration::fromSeconds(psg.getData().getLengthSeconds())}, {}};
            importTune(*getAudioTracks(*edit)[0], psg.getData(), pos);
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(),
                                      3000.0 / e.framesPerBeat,
                                      0.01,
                                      String(e.prefix) + " imported tempo");
        }

        // "Kick Da Break" has only a pulse, 9 frames, four to the beat, and starts 6 frames before a bar
        // line: the first pattern of its module is that much longer than the others. The opening does not
        // follow the pulse, a swing's rows being on and off it, so the pulse itself places the tune.
        if (const auto files = dir.findChildFiles(File::findFiles, false, "09_fatalsnipe*.psg"); files.size() == 1) {
            PsgFile psg(files.getFirst());
            expect(psg.ensureRead());
            const auto result = PsgTempo::detect(psg.getData());
            expect(result.hasValue());
            if (result) {
                expect(result->rowsArePulse && result->rowsPerBeat == 4);
                expectEquals(result->firstRowFrame, 6, "the first downbeat is the first pulse");
                expect(!result->silentLeadIn, "the six frames before it sound: a pickup");
            }
        }
    }

    /** Development report, run only when UZX_TEMPO_CORPUS names a folder of PSG files: one
        tab-separated "TEMPO" line per file with the row pattern, rows and frames per beat,
        BPM, grid share, meter and its clarity, and the first-row placement, or "-" when no
        tempo is found. scripts/validate_tempo_detection.py compares it against PT3 speeds
        and the ear labels. */
    void runCorpusReport() {
        const auto dirName = SystemStats::getEnvironmentVariable("UZX_TEMPO_CORPUS", {});
        if (dirName.isEmpty())
            return;
        beginTest("Corpus report from UZX_TEMPO_CORPUS");
        for (const auto& file : File(dirName).findChildFiles(File::findFiles, false, "*.psg")) {
            PsgFile psg(file);
            const auto r = psg.ensureRead() ? PsgTempo::detect(psg.getData()) : juce::Optional<PsgTempo::Result>{};
            StringArray fields{"TEMPO", file.getFileNameWithoutExtension()};
            if (r)
                fields.addArray(StringArray{rowPattern(r->rowFrames),
                                            String(r->rowsPerBeat),
                                            String(r->framesPerBeat),
                                            String(r->bpm, 2),
                                            String(r->gridShare, 3),
                                            String(r->beatsPerBar),
                                            String((int) r->meterClear),
                                            String(r->firstRowFrame),
                                            String((int) r->silentLeadIn)});
            else
                fields.add("-");
            logMessage(fields.joinIntoString("\t"));
        }
    }

    /** How many of the row starts lie on the grid of repeating row lengths through frame `phase`. */
    static size_t rowsOnGrid(const std::vector<size_t>& starts, const std::vector<int>& rows, size_t phase) {
        const auto period = (size_t) std::accumulate(rows.begin(), rows.end(), 0);
        std::vector<bool> onGrid(period, false);
        for (size_t k = 0, offset = 0; k < rows.size(); offset += (size_t) rows[k++])
            onGrid[offset] = true;
        size_t hits = 0;
        for (auto start : starts)
            hits += onGrid[(start % period + period - phase % period) % period] ? 1 : 0;
        return hits;
    }

    /** The fixed grid that fits a module's row starts best, and how well: for every period up to
        64 frames, the residues that start a row in at least half of the cycles; the fit is the
        lower of the share of the rows on them and the share of their frames that start a row.
        A tune that slips a frame or changes speed stays well below 1. The shortest period within
        0.005 of the best fit wins, so its row lengths are the tune's groove. */
    static std::pair<double, std::vector<int>> bestFixedGrid(const std::vector<size_t>& starts) {
        const auto span = (double) (starts.back() - starts.front() + 1);
        double bestFit = 0.0;
        std::vector<int> bestRows;
        for (size_t period = 2; period <= 64; ++period) {
            std::vector<size_t> counts(period, 0);
            for (auto start : starts)
                ++counts[start % period];
            const double cycles = span / (double) period;
            std::vector<size_t> residues;
            size_t hits = 0;
            for (size_t r = 0; r < period; ++r)
                if ((double) counts[r] >= cycles / 2.0) {
                    residues.push_back(r);
                    hits += counts[r];
                }
            if (residues.empty())
                continue;
            const double fit = std::min((double) hits / (double) starts.size(),
                                        std::min(1.0, (double) hits / ((double) residues.size() * cycles)));
            if (fit <= bestFit + 0.005)
                continue;
            bestFit = fit;
            bestRows.clear();
            for (size_t k = 0; k < residues.size(); ++k)
                bestRows.push_back((int) ((k + 1 < residues.size() ? residues[k + 1] : residues[0] + period) - residues[k]));
        }
        return {bestFit, bestRows};
    }

    /** Development report, run only when UZX_TEMPO_MODULES names a folder of tracker modules. A
        module knows the frame every row starts at, so each gets a tab-separated "ROWS" line that
        sets the grid detected from the registers alone against those rows: frames, rows, the
        row pattern the app takes from the module's rows, the fixed grid that fits the row
        starts best and its fit (see bestFixedGrid), then the detected pattern ("~" before a pulse) or "-", its grid
        share, the share of the module's rows on the detected grid, the share of the detected
        rows that are the module's, rows and frames per beat from the module's rows and
        from the detected ones, and where bars start: the frames before the first bar line as the
        app takes it from the module's patterns, the frame the detected grid starts at, and the
        frames before the first downbeat as detected.
        With UZX_TEMPO_DUMP naming a folder, every chip is also written there as <name>.psg
        (<name>.2.psg for a second chip) with its event cells as <name>.cells (four little-endian
        uint16 a frame, see PsgRowEvidence::cellsOf), the row starts as <name>.rows and the pattern
        starts as <name>.patterns:
        scripts/train_row_evidence.py learns the detector's table from them. scripts/validate_tempo_detection.py --rows sums
        the lines up. */
    void runModuleRowsReport() {
        const auto dirName = SystemStats::getEnvironmentVariable("UZX_TEMPO_MODULES", {});
        if (dirName.isEmpty())
            return;
        beginTest("Row report from UZX_TEMPO_MODULES");
        const auto dumpName = SystemStats::getEnvironmentVariable("UZX_TEMPO_DUMP", {});
        for (const auto& file : File(dirName).findChildFiles(File::findFiles, false)) {
            MemoryBlock bytes;
            if (!file.loadFileAsData(bytes))
                continue;
            const auto module = uZX::readTrackerModule(bytes);
            if (!module || module->chips.empty() || module->rowStarts.size() < 2)
                continue;
            std::vector<const PsgData*> chips;
            for (const auto& chip : module->chips)
                chips.push_back(&chip.psg);
            const auto& starts = module->rowStarts;
            const auto own = PsgTempo::detect(chips, starts, module->patternStarts);
            if (!own)
                continue;
            const auto playRate = (int) std::max<size_t>(1, chips.front()->options.playRate);
            const auto dataRows = [&](const PsgTempo::Result& r) {
                auto rows = r.rowFrames;
                for (auto& row : rows)
                    row /= playRate;
                return rows;
            };

            const auto got = PsgTempo::detect(chips);
            const auto [fit, fixedRows] = bestFixedGrid(starts);
            StringArray fields{"ROWS",
                               file.getFileNameWithoutExtension(),
                               String(chips.front()->frames.size()),
                               String(starts.size()),
                               rowPattern(dataRows(*own)),
                               fixedRows.empty() ? String("-") : rowPattern(fixedRows),
                               String(fit, 3)};

            if (got) {
                const auto rows = dataRows(*got);
                const auto phase = (size_t) (got->gridPhase / playRate);
                std::vector<size_t> span(starts.back() - starts.front() + 1);
                std::iota(span.begin(), span.end(), starts.front());
                const auto hits = rowsOnGrid(starts, rows, phase);
                fields.addArray(StringArray{(got->rowsArePulse ? "~" : "") + rowPattern(rows),
                                            String(got->gridShare, 3),
                                            String((double) hits / (double) starts.size(), 3),
                                            String((double) hits / (double) rowsOnGrid(span, rows, phase), 3),
                                            String(own->rowsPerBeat),
                                            String(own->framesPerBeat),
                                            String(got->rowsPerBeat),
                                            String(got->framesPerBeat)});
            } else {
                fields.addArray(StringArray{"-", "0", "0", "0", String(own->rowsPerBeat), String(own->framesPerBeat), "0", "0"});
            }
            // where bars start: as the app takes it from the module's patterns, then the grid's first
            // row and the frames before the first downbeat as detected from the registers
            fields.add(String(own->firstRowFrame / playRate));
            fields.add(String(got ? got->gridPhase / playRate : 0));
            fields.add(String(got ? got->firstRowFrame / playRate : 0));
            logMessage(fields.joinIntoString("\t"));

            if (dumpName.isEmpty())
                continue;
            const auto dump = File(dumpName);
            const auto name = file.getFileNameWithoutExtension();
            for (size_t chip = 0; chip < module->chips.size(); ++chip) {
                const auto out = dump.getChildFile(name + (chip == 0 ? String() : "." + String(chip + 1)) + ".psg");
                out.deleteFile();
                PsgFile psg(out);
                psg.getData() = module->chips[chip].psg;
                FileOutputStream stream(out);
                expect(stream.openedOk() && psg.writeTo(stream), "dump " + out.getFileName());

                MemoryOutputStream cells;
                for (const auto& frame : PsgRowEvidence::cellsOf(module->chips[chip].psg)) {
                    for (auto cell : frame.channels)
                        cells.writeShort((short) cell);
                    cells.writeShort((short) frame.chip);
                }
                out.withFileExtension("cells").replaceWithData(cells.getData(), cells.getDataSize());
            }
            StringArray rowList;
            for (auto start : starts)
                rowList.add(String(start));
            dump.getChildFile(name + ".rows").replaceWithText(rowList.joinIntoString(" "));
            StringArray patternList;
            for (auto start : module->patternStarts)
                patternList.add(String(start));
            dump.getChildFile(name + ".patterns").replaceWithText(patternList.joinIntoString(" "));
        }
    }
};

static PsgTempoDetectorTests psgTempoDetectorTests;

}  // namespace MoTool::Tests
