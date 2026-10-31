#include "PsgTempoDetector.h"

#include <JuceHeader.h>

#include "PsgClip.h"
#include "formats/psg/PsgData.h"
#include "formats/psg/PsgFile.h"
#include "models/EditUtilities.h"

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

        // Developer aid, not a check: UZX_PSG_TEMPO_DIR=<folder> prints the detector's
        // verdict for every .psg there; scripts/psg_tempo_validate.py reads it.
        if (const auto dir = SystemStats::getEnvironmentVariable("UZX_PSG_TEMPO_DIR", ""); dir.isNotEmpty()) {
            beginTest("Real files from UZX_PSG_TEMPO_DIR");
            for (const auto& file : File(dir).findChildFiles(File::findFiles, false, "*.psg")) {
                PsgFile psg(file);
                psg.ensureRead();
                logMessage(String::formatted("frames=%d playRate=%d fps=%.1f ",
                                             (int) psg.getData().frames.size(),
                                             (int) psg.getData().options.playRate,
                                             psg.getData().getFrameRate())
                           + file.getFileName());
                if (auto r = PsgTempo::detect(psg.getData()))
                    logMessage(rowPattern(r->rowFrames)
                               + String::formatted(" rpb=%d fpb=%d bpm=%.1f g=%.2f sig=%d clear=%d first=%d silent=%d ",
                                                   r->rowsPerBeat,
                                                   r->framesPerBeat,
                                                   r->bpm,
                                                   r->gridShare,
                                                   r->beatsPerBar,
                                                   (int) r->meterClear,
                                                   r->firstRowFrame,
                                                   (int) r->silentLeadIn)
                               + file.getFileName());
                else
                    logMessage("? " + file.getFileName());
            }
        }

        beginTest("Importing into an empty edit sets tempo and grid; a second clip keeps them");
        {
            auto& engine = *tracktion::Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit(engine);
            auto* track = getAudioTracks(*edit)[0];
            auto tune = makeTune(3000, {6}, 4);
            const tracktion::ClipPosition pos{{0_tp, tracktion::TimeDuration::fromSeconds(tune.getLengthSeconds())},
                                              {}};

            auto clip = PsgClip::insertTo(*track, tune, pos, "first");
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), 125.0, 0.01);
            expectWithinAbsoluteError(clip->getPsg().getFramesPerBeat(), 24.0, 0.01);
            expect(Helpers::getGridSubdivisionPattern(*edit, 24) == std::vector<int>{6, 6, 6, 6});
            expectEquals(edit->tempoSequence.getTimeSig(0)->numerator.get(), 4);

            auto waltzEdit = Edit::createSingleTrackEdit(engine);
            auto waltz = makeBarTune(4000, 3);
            PsgClip::insertTo(*getAudioTracks(*waltzEdit)[0],
                              waltz,
                              {{0_tp, tracktion::TimeDuration::fromSeconds(waltz.getLengthSeconds())}, {}},
                              "waltz");
            expectEquals(waltzEdit->tempoSequence.getTimeSig(0)->numerator.get(), 3);
            expectEquals(waltzEdit->tempoSequence.getTimeSig(0)->denominator.get(), 4);

            auto second = makeTune(3000, {5}, 4);
            PsgClip::insertTo(
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
            PsgClip::insertTo(
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
            PsgClip::insertTo(*getAudioTracks(*edit)[0],
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
            PsgClip::insertTo(*getAudioTracks(*edit)[0],
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
                auto clip = PsgClip::insertTo(*getAudioTracks(*edit)[0], tune, pos, "lead-in");
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
    }
};

static PsgTempoDetectorTests psgTempoDetectorTests;

}  // namespace MoTool::Tests
