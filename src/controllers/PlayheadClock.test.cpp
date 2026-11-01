#include <JuceHeader.h>

#include "PlayheadClock.h"

namespace MoTool {

class PlayheadClockTest : public UnitTest
{
public:
    PlayheadClockTest() : UnitTest("PlayheadClock", "MoTool") {}

    void runTest() override {
        testSteadyPlayback();
        testHoldAndStart();
        testJumps();
        testReportsStopping();
    }

private:
    static constexpr double frameMs = 1000.0 / 60.0;
    static constexpr double blockSec = 512.0 / 44100.0;

    void testSteadyPlayback() {
        beginTest("Steady playback shows even forward steps close to the true position");
        PlayheadClock clock;
        const double startSec = 1.0, startMs = 1000.0;
        clock.report(startSec, startMs, false);

        // Reports every 22.4 ms +/- 3 ms of jitter, rounded down to a 512-sample block.
        auto reportTimeMs = [](int i) { return i * 22.4 + (i % 2 == 0 ? -3.0 : 3.0); };
        int nextReport = 1;
        double previous = 0.0, worstStepError = 0.0, smallestStepMs = 1e9, worstOffset = 0.0;

        for (int frame = 0; frame * frameMs < 3000.0; ++frame) {
            const auto elapsedMs = frame * frameMs;
            for (; reportTimeMs(nextReport) <= elapsedMs; ++nextReport) {
                const auto trueSec = startSec + reportTimeMs(nextReport) / 1000.0;
                const auto blockSecs = std::floor(trueSec / blockSec) * blockSec;
                clock.report(blockSecs, startMs + reportTimeMs(nextReport), true);
            }

            const auto shown = clock.positionAt(startMs + elapsedMs);
            if (elapsedMs > 500.0) {
                const auto stepMs = (shown - previous) * 1000.0;
                worstStepError = std::max(worstStepError, std::abs(stepMs - frameMs));
                smallestStepMs = std::min(smallestStepMs, stepMs);
                worstOffset = std::max(worstOffset, std::abs(shown - (startSec + elapsedMs / 1000.0)));
            }
            previous = shown;
        }

        expectLessThan(worstStepError, 2.0);
        expectGreaterThan(smallestStepMs, 0.0);
        expectLessThan(worstOffset, blockSec + 0.003);
    }

    void testHoldAndStart() {
        beginTest("A non-moving report holds exactly and does not run on");
        PlayheadClock clock;
        clock.report(5.0, 1000.0, false);
        expectWithinAbsoluteError(clock.positionAt(1000.0), 5.0, 1e-12);
        expectWithinAbsoluteError(clock.positionAt(1500.0), 5.0, 1e-12);
        expectWithinAbsoluteError(clock.positionAt(3000.0), 5.0, 1e-12);

        beginTest("Starting from a hold runs on from the held position without a jump");
        clock.report(5.0, 4000.0, true);
        expectWithinAbsoluteError(clock.positionAt(4000.0), 5.0, 1e-12);
        expectWithinAbsoluteError(clock.positionAt(4100.0), 5.1, 0.003);
    }

    void testJumps() {
        beginTest("A backwards jump signalled by a non-moving report is followed immediately");
        PlayheadClock clock;
        clock.report(10.0, 1000.0, true);
        expectWithinAbsoluteError(clock.positionAt(1010.0), 10.01, 0.003);
        clock.report(2.0, 1020.0, false);
        expectWithinAbsoluteError(clock.positionAt(1025.0), 2.0, 1e-12);

        beginTest("A moving report far from the shown position snaps to it");
        clock.report(2.0, 2000.0, true);
        clock.report(8.0, 2020.0, true);
        expectWithinAbsoluteError(clock.positionAt(2030.0), 8.01, 1e-9);
    }

    void testReportsStopping() {
        beginTest("The clock stops advancing once reports are older than the extrapolation limit");
        PlayheadClock clock;
        clock.report(3.0, 1000.0, true);
        const auto running = clock.positionAt(1000.0 + PlayheadClock::maxExtrapolationMs / 2);
        expectGreaterThan(running, 3.0);

        const auto stopped = clock.positionAt(1000.0 + PlayheadClock::maxExtrapolationMs + 50.0);
        const auto later = clock.positionAt(1000.0 + PlayheadClock::maxExtrapolationMs + 150.0);
        expectWithinAbsoluteError(later, stopped, 1e-12);
    }
};

static PlayheadClockTest playheadClockTest;

}  // namespace MoTool
