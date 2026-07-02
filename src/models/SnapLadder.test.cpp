#include <JuceHeader.h>

#include "SnapLadder.h"

namespace MoTool {

class SnapLadderTest : public juce::UnitTest {
public:
    SnapLadderTest() : UnitTest("SnapLadder", "MoTool") {}

    void expectTime(te::TimePosition actual, te::TimePosition expected, const juce::String& what) {
        expectWithinAbsoluteError(actual.inSeconds(), expected.inSeconds(), 1.0e-9, what);
    }

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        constexpr double fps = 50.0;
        auto frameTime = [](double frame) { return te::TimePosition::fromSeconds(frame / fps); };

        beginTest("Ladder construction for barsBeatsFrames");
        {
            SnapLadder ladder(TimecodeDisplayMode::barsBeatsFrames, fps, 25);

            // frames x1, x2, x5 (a multiple must fit the beat 4 times), beat, 9 bar levels
            expectEquals(ladder.getNumLevels(), 13, "level count");
            expect(ladder.getKind(0) == SnapLadder::LevelKind::frames, "level 0 is frames");
            expect(ladder.getKind(2) == SnapLadder::LevelKind::frames, "level 2 is frames");
            expectEquals(ladder.getBeatLevel(), 3, "beat level index");
            expect(ladder.getKind(3) == SnapLadder::LevelKind::beat, "level 3 is beat");
            expect(ladder.getKind(4) == SnapLadder::LevelKind::bar, "level 4 is bar");
            expect(ladder.getKind(12) == SnapLadder::LevelKind::bar, "last level is bar");
        }

        beginTest("Frame rounding at 26 frames per beat");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(3000.0 / 26.0);  // beat = 0.52s = 26 frames
            const auto& ts = edit->tempoSequence;
            const auto beatEnd = ts.toTime(te::BeatPosition::fromBeats(1.0));

            SnapLadder ladder(TimecodeDisplayMode::barsBeatsFrames, fps, 26);
            const int x5 = 2;  // frames x5 level: 0, 5, 10, 15, 20, 25, |26

            expectTime(ladder.roundDown(x5, frameTime(24.0), ts), frameTime(20.0), "round down 24 -> 20");
            expectTime(ladder.roundNearest(x5, frameTime(24.0), ts), frameTime(25.0), "round nearest 24 -> 25");
            expectTime(ladder.roundUp(x5, frameTime(21.0), ts), frameTime(25.0), "round up 21 -> 25");
            expectTime(ladder.roundUp(x5, frameTime(25.5), ts), beatEnd, "round up past the last multiple carries to the beat");
            expectTime(ladder.getNextTick(x5, frameTime(25.0), ts), beatEnd, "next tick after 25 is the beat boundary");
            expectTime(ladder.getNextTick(x5, beatEnd, ts), beatEnd + te::TimeDuration::fromSeconds(5.0 / fps),
                       "frame grid restarts at the next beat");

            expect(ladder.isOnGrid(x5, frameTime(25.0), ts), "frame 25 is on the x5 grid");
            expect(ladder.isOnGrid(x5, beatEnd, ts), "beat boundary is on every frame grid");
            expect(! ladder.isOnGrid(x5, frameTime(24.0), ts), "frame 24 is off the x5 grid");
        }

        beginTest("Subdivision pattern 6-7-6-7 within a 26-frame beat");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(3000.0 / 26.0);
            const auto& ts = edit->tempoSequence;
            const auto beatEnd = ts.toTime(te::BeatPosition::fromBeats(1.0));

            SnapLadder ladder(TimecodeDisplayMode::barsBeatsFrames, fps, 26, { 6, 7, 6, 7 });

            // min group is 6, so only frames x1 fits below the subdivision
            expect(ladder.getKind(0) == SnapLadder::LevelKind::frames, "level 0 is frames");
            expect(ladder.getKind(1) == SnapLadder::LevelKind::subdivision, "level 1 is the subdivision");
            expectEquals(ladder.getBeatLevel(), 2, "beat level index");

            const int subdivision = 1;
            auto tick = ladder.getNextTick(subdivision, te::TimePosition(), ts);
            expectTime(tick, frameTime(6.0), "first subdivision boundary at frame 6");
            tick = ladder.getNextTick(subdivision, tick, ts);
            expectTime(tick, frameTime(13.0), "second subdivision boundary at frame 13");
            tick = ladder.getNextTick(subdivision, tick, ts);
            expectTime(tick, frameTime(19.0), "third subdivision boundary at frame 19");
            tick = ladder.getNextTick(subdivision, tick, ts);
            expectTime(tick, beatEnd, "subdivision carries to the beat boundary");

            expect(ladder.isOnGrid(subdivision, frameTime(13.0), ts), "frame 13 is a subdivision boundary");
            expect(! ladder.isOnGrid(subdivision, frameTime(10.0), ts), "frame 10 is not a subdivision boundary");
            expectTime(ladder.roundNearest(subdivision, frameTime(15.0), ts), frameTime(13.0), "nearest of 15 is 13");
            expectTime(ladder.roundNearest(subdivision, frameTime(17.0), ts), frameTime(19.0), "nearest of 17 is 19");
            expectEquals(ladder.getMilestoneAfter(0), subdivision,
                         "milestone above single frames is the subdivision when a pattern is active");
            expectEquals(ladder.getMilestoneAfter(subdivision), ladder.getBeatLevel(),
                         "milestone above the subdivision is the beat");
        }

        beginTest("Frame levels restart at subdivision boundaries");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(3000.0 / 26.0);
            const auto& ts = edit->tempoSequence;

            edit->tempoSequence.getTempo(0)->setBpm(3000.0 / 32.0);
            SnapLadder ladder(TimecodeDisplayMode::barsBeatsFrames, fps, 32, { 16, 16 });
            const int x4 = 1;  // frames x4: 0, 4 .. 12, then restart 16, 20 .. 28

            expect(ladder.isOnGrid(x4, frameTime(12.0), ts), "frame 12 on grid");
            expect(ladder.isOnGrid(x4, frameTime(16.0), ts), "frame 16 restarts the grid");
            expect(! ladder.isOnGrid(x4, frameTime(17.0), ts), "frame 17 off grid after restart");
            expect(ladder.isOnGrid(x4, frameTime(20.0), ts), "frame 20 on grid");
            expectTime(ladder.getNextTick(x4, frameTime(12.0), ts), frameTime(16.0), "next after 12 is the group boundary");
            expectTime(ladder.getNextTick(x4, frameTime(16.0), ts), frameTime(20.0), "next after 16 is 20");
        }

        beginTest("Subdivision pattern redistributes when it doesn't match the beat");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(3000.0 / 26.0);
            const auto& ts = edit->tempoSequence;

            // Sum is 20, beat has 26 frames: 4 groups spread as 6, 7, 6, 7.
            SnapLadder ladder(TimecodeDisplayMode::barsBeatsFrames, fps, 26, { 5, 5, 5, 5 });
            const int subdivision = 1;

            auto tick = ladder.getNextTick(subdivision, te::TimePosition(), ts);
            expectTime(tick, frameTime(6.0), "redistributed boundary at frame 6");
            tick = ladder.getNextTick(subdivision, tick, ts);
            expectTime(tick, frameTime(13.0), "redistributed boundary at frame 13");
            tick = ladder.getNextTick(subdivision, tick, ts);
            expectTime(tick, frameTime(19.0), "redistributed boundary at frame 19");
        }

        beginTest("Best level selection follows the pixel budget");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);  // beat = 0.5s = 25 frames
            const auto& ts = edit->tempoSequence;
            const auto& tempo = ts.getTempoAt(te::TimePosition());

            SnapLadder ladder(TimecodeDisplayMode::barsBeatsFrames, fps, 25);

            const auto zoomedIn = te::TimeDuration::fromSeconds(0.001);   // 20 px per frame
            expectEquals(ladder.getBestLevel(zoomedIn, 12.0, tempo, false), 0, "single frames when zoomed in");
            expectEquals(ladder.getBestLevel(zoomedIn, 48.0, tempo, false), 1, "x4 frames for the 48px budget");

            const auto zoomedOut = te::TimeDuration::fromSeconds(0.016);  // 1.25 px per frame
            expectEquals(ladder.getBestLevel(zoomedOut, 12.0, tempo, false), ladder.getBeatLevel(),
                         "frames give way to beats when zoomed out");
        }

        beginTest("Milestones and escalation");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            const auto& ts = edit->tempoSequence;

            SnapLadder ladder(TimecodeDisplayMode::barsBeatsFrames, fps, 25);
            const int beat = ladder.getBeatLevel(), bar = beat + 1;

            expectEquals(ladder.getMilestoneAfter(0), beat, "milestone above frames is the beat");
            expectEquals(ladder.getMilestoneAfter(beat), bar, "milestone above the beat is the bar");
            expectEquals(ladder.getMilestoneAfter(bar), bar + 2, "milestone above 1 bar is 4 bars");

            expectEquals(ladder.getMaxLevelAt(te::TimePosition(), ts), ladder.getNumLevels() - 1,
                         "the origin lies on the coarsest grid");
            expectEquals(ladder.getMaxLevelAt(te::TimePosition::fromSeconds(0.5), ts), beat, "beat 2 escalates to the beat level");
            expectEquals(ladder.getMaxLevelAt(te::TimePosition::fromSeconds(2.0), ts), bar, "bar 2 escalates to the bar level");
            expectEquals(ladder.getMaxLevelAt(te::TimePosition::fromSeconds(4.0), ts), bar + 1, "bar 3 escalates to 2 bars");
            expectEquals(ladder.getMaxLevelAt(frameTime(5.0), ts), 2, "frame 5 escalates to the x5 level");
            expectEquals(ladder.getMaxLevelAt(frameTime(4.0), ts), 1, "frame 4 escalates to the x2 level");
            expectEquals(ladder.getMaxLevelAt(frameTime(3.0), ts), 0, "frame 3 stays at single frames");
        }

        beginTest("Labels");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            const auto& ts = edit->tempoSequence;

            SnapLadder ladder(TimecodeDisplayMode::barsBeatsFrames, fps, 25);
            const int beat = ladder.getBeatLevel(), bar = beat + 1;

            expectEquals(ladder.getLabel(0, frameTime(5.0), ts), juce::String("1.1.05"), "frame label");
            expectEquals(ladder.getLabel(beat, te::TimePosition::fromSeconds(0.5), ts), juce::String("1.2"), "beat label");
            expectEquals(ladder.getLabel(bar, te::TimePosition::fromSeconds(2.0), ts), juce::String("2"), "bar label");

            SnapLadder frames(TimecodeDisplayMode::framesOnly, fps, 25);
            expectEquals(frames.getLabel(0, frameTime(5.0), ts), juce::String("0:00:05"), "frames-only label");
        }

        beginTest("Frames-only grid restarts at each second");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            const auto& ts = edit->tempoSequence;

            SnapLadder ladder(TimecodeDisplayMode::framesOnly, 60.0, 25);
            const int x10 = 3;  // x1, x2, x5, x10 fit 60 frames per second
            expect(ladder.getKind(x10) == SnapLadder::LevelKind::frames, "level 3 is frames");
            expect(ladder.getKind(x10 + 1) == SnapLadder::LevelKind::teSnap, "seconds level follows the frames");
            expectEquals(ladder.getBeatLevel(), -1, "no beat level in frames-only mode");

            const auto frame50 = te::TimePosition::fromSeconds(50.0 / 60.0);
            expectTime(ladder.getNextTick(x10, frame50, ts), te::TimePosition::fromSeconds(1.0),
                       "the x10 grid lands on the second boundary");
            expectTime(ladder.getNextTick(x10, te::TimePosition::fromSeconds(1.0), ts),
                       te::TimePosition::fromSeconds(1.0 + 10.0 / 60.0), "and restarts after it");
        }

        beginTest("Tick iteration advances strictly across many beat boundaries");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(3000.0 / 26.0);  // awkward beat length
            const auto& ts = edit->tempoSequence;

            SnapLadder ladder(TimecodeDisplayMode::barsBeatsFrames, fps, 26, { 6, 7, 6, 7 });

            for (int level = 0; level < ladder.getNumLevels(); ++level) {
                auto time = te::TimePosition();
                for (int i = 0; i < 200; ++i) {
                    // Nudge slightly below the tick to exercise float round-trip error.
                    auto probe = time - te::TimeDuration::fromSeconds(1.0e-10);
                    auto next = ladder.getNextTick(level, probe > te::TimePosition() ? probe : time, ts);
                    expect(next > time, "tick iteration stalled at level " + juce::String(level)
                                        + ", step " + juce::String(i));
                    if (next <= time)
                        break;
                    time = next;
                    if (time > te::TimePosition::fromSeconds(120.0))
                        break;
                }
            }
        }

        beginTest("barsBeats mode wraps the native te snap levels");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            const auto& ts = edit->tempoSequence;

            SnapLadder ladder(TimecodeDisplayMode::barsBeats, fps, 25);
            expectEquals(ladder.getNumLevels(), 19, "all te barsBeats levels present");
            expectEquals(ladder.getMilestoneAfter(5), 9, "milestone above sub-beat levels is the beat");
            expectEquals(ladder.getMilestoneAfter(9), 10, "milestone above the beat is the bar");
            expectEquals(ladder.getLabel(9, te::TimePosition::fromSeconds(0.5), ts), juce::String("1.2"), "beat label normalized");
            expectEquals(ladder.getLabel(10, te::TimePosition::fromSeconds(2.0), ts), juce::String("2"), "bar label normalized");
        }
    }
};

static SnapLadderTest snapLadderTest;

}  // namespace MoTool
