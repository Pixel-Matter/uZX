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
            data.frames[(size_t) t].setTonePeriod(0, (uint16_t) (200 + (row % 5) * 20));
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

    void runTest() override {
        beginTest("Speed 6, accent every 4 rows: 24 frames per beat at 125 BPM");
        {
            auto result = PsgTempo::detect(makeTune(3000, {6}, 4));
            expect(static_cast<bool>(result));
            expectEquals(result->speedA, 6);
            expectEquals(result->speedB, 6);
            expectEquals(result->rowsPerBeat, 4);
            expectEquals(result->framesPerBeat, 24);
            expectWithinAbsoluteError(result->bpm, 125.0, 0.01);
            expect(result->subdivisionPattern() == std::vector<int>{6, 6, 6, 6});
        }

        beginTest("Speed 3, accent every 8 rows: rows per beat doubles to stay in range");
        {
            auto result = PsgTempo::detect(makeTune(3000, {3}, 8));
            expect(static_cast<bool>(result));
            expectEquals(result->speedA, 3);
            expectEquals(result->rowsPerBeat, 8);
            expectEquals(result->framesPerBeat, 24);
        }

        beginTest("6-7 swing gives a 6 7 6 7 pattern, 26 frames per beat");
        {
            auto result = PsgTempo::detect(makeTune(3000, {6, 7}, 4));
            expect(static_cast<bool>(result));
            expectEquals(result->speedA, 6);
            expectEquals(result->speedB, 7);
            expectEquals(result->framesPerBeat, 26);
            expect(result->subdivisionPattern() == std::vector<int>{6, 7, 6, 7});
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
        // verdict for every .psg there, to compare against scripts/psg_tempo.py.
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
                    logMessage(String::formatted("%d-%d rpb=%d fpb=%d bpm=%.1f g=%.2f ",
                                                 r->speedA,
                                                 r->speedB,
                                                 r->rowsPerBeat,
                                                 r->framesPerBeat,
                                                 r->bpm,
                                                 r->gridShare)
                               + file.getFileName());
                else
                    logMessage("? " + file.getFileName());
            }
        }

        beginTest("Importing into an empty edit sets tempo and grid; a second clip keeps them");
        {
            auto& engine = *te::Engine::getEngines()[0];
            auto edit = Edit::createSingleTrackEdit(engine);
            auto* track = getAudioTracks(*edit)[0];
            auto tune = makeTune(3000, {6}, 4);
            const te::ClipPosition pos{{0_tp, te::TimeDuration::fromSeconds(tune.getLengthSeconds())}, {}};

            auto clip = PsgClip::insertTo(*track, tune, pos, "first");
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), 125.0, 0.01);
            expectWithinAbsoluteError(clip->getPsg().getFramesPerBeat(), 24.0, 0.01);
            expect(Helpers::getGridSubdivisionPattern(*edit, 24) == std::vector<int>{6, 6, 6, 6});

            auto second = makeTune(3000, {5}, 4);
            PsgClip::insertTo(*track,
                              second,
                              {{te::TimePosition::fromSeconds(70.0), te::TimeDuration::fromSeconds(60.0)}, {}},
                              "second");
            expectWithinAbsoluteError(edit->tempoSequence.getTempoAt(0_tp).getBpm(), 125.0, 0.01);
        }
    }
};

static PsgTempoDetectorTests psgTempoDetectorTests;

}  // namespace MoTool::Tests
