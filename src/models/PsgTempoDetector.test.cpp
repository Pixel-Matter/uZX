#include "PsgTempoDetector.h"

#include <JuceHeader.h>

#include "PsgClip.h"
#include "formats/psg/PsgData.h"
#include "formats/psg/PsgFile.h"
#include "models/EditUtilities.h"
#include "models/PsgFrameRetimer.h"
#include "formats/tracker/TrackerModule.h"

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

            // A grid needs no notes once the module gives its rows; one row is no grid.
            auto quiet = makeTune(60, {6}, 4);
            expect(static_cast<bool>(PsgTempo::detect({&quiet}, rowsOf(60, {6}))));
            expect(!PsgTempo::detect({&quiet}, {0}), "a single row falls back to detection");
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
            // 4/4 at 24 frames per beat: a bar is 96 frames, so the clip moves 91 frames right
            expectWithinAbsoluteError(shifted->getPosition().getStart().inSeconds(), 91.0 / 50.0, 1.0e-9);
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
                // every clip moves alike, so the first row (frame 5) starts bar 2
                expectWithinAbsoluteError(clip->getPosition().getStart().inSeconds(), (barFrames - 5) / 50.0, 1.0e-9);
                expectWithinAbsoluteError(clip->getPosition().getOffset().inSeconds(), 0.0, 1.0e-9);
            }
        }

        testCorpusFixtures();
        testFrameRateChanges();
        runCorpusReport();
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
                    chips.push_back(&chip);
                const auto at50 = PsgTempo::detect(chips, module->rowStarts);
                for (auto& chip : module->chips)
                    chip.options.frameRate = 200.0;
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
};

static PsgTempoDetectorTests psgTempoDetectorTests;

}  // namespace MoTool::Tests
