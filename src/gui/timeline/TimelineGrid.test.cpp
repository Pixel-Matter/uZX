#include <JuceHeader.h>

#include "TimelineGrid.h"
#include "Ruler.h"
#include "../../models/EditUtilities.h"
#include "../../models/Ids.h"

namespace MoTool {

class TimelineGridTest : public UnitTest {
public:
    TimelineGridTest() : UnitTest("TimelineGrid", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("A click puts the playhead at its time, never before 0, snapped only when snapping is on");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            editViewState.zoom.setViewWidthPx(1000);
            editViewState.zoom.setRange({te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(1.0)});
            TimelineGrid grid(editViewState);
            const auto at123 = editViewState.zoom.xToTime(123);

            editViewState.setSnapPlayheadToGrid(false);
            expectWithinAbsoluteError(grid.playheadTimeAtX(123).inSeconds(), at123.inSeconds(), 1e-9);
            expectEquals(grid.playheadTimeAtX(-50).inSeconds(), 0.0);

            editViewState.setSnapPlayheadToGrid(true);
            expectEquals(grid.playheadTimeAtX(123).inSeconds(), grid.snapTimeToVisibleGrid(at123).inSeconds());
            expect(grid.playheadTimeAtX(123) != at123, "precondition: 123 px is off the grid");
        }

        beginTest("Bars beats frames grid uses frame subdivisions when zoomed in below a beat");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::barsBeatsFrames);
            Helpers::setProjectFps(*edit, 50.0);

            editViewState.zoom.setViewWidthPx(1000);
            editViewState.zoom.setRange({te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(1.0)});

            TimelineGrid grid(editViewState);
            auto ticks = grid.getTicks();
            StringArray labels;

            auto hasTickAtX = [&ticks](int x) {
                return std::any_of(ticks.begin(), ticks.end(), [x](const auto& tick) {
                    return tick.x == x;
                });
            };

            for (const auto& tick : ticks) {
                if (tick.label.isNotEmpty())
                    labels.add(tick.label);
            }

            expect(hasTickAtX(20), "Expected a grid tick at the first 50 fps frame.");
            expect(hasTickAtX(40), "Expected a grid tick at the second 50 fps frame.");
            expect(hasTickAtX(60), "Expected a grid tick at the third 50 fps frame.");
            expect(! hasTickAtX(16), "Grid should not use 1/32-beat ticks when the selected format is frames.");
            expect(labels.contains("1.1.04"), "Expected ruler labels to use 50 fps frame counts.");
            expect(! labels.contains("1.1.120"),
                   "Ruler labels should not use Tracktion beat ticks for bars/beats/frames.");
            expect(labels.contains("1.2"), "Beat labels should match the native bars/beats format.");
            expect(! labels.contains("1.2.00"), "Beat labels should not add a frame field.");
        }

        beginTest("Bars beats frames grid uses native bars beats when zoomed out");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::barsBeatsFrames);
            Helpers::setProjectFps(*edit, 50.0);

            editViewState.zoom.setViewWidthPx(1000);
            editViewState.zoom.setRange({te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(16.0)});

            TimelineGrid grid(editViewState);
            auto ticks = grid.getTicks();
            StringArray labels;

            auto hasTickAtX = [&ticks](int x) {
                return std::any_of(ticks.begin(), ticks.end(), [x](const auto& tick) {
                    return tick.x == x;
                });
            };

            for (const auto& tick : ticks) {
                if (tick.label.isNotEmpty())
                    labels.add(tick.label);
            }

            expect(hasTickAtX(31), "Expected a native beat tick at the first beat.");
            expect(! hasTickAtX(13), "Zoomed-out bars/beats/frames should not show frame subdivision ticks.");
            expect(labels.contains("1"), "Expected native bar labels when zoomed out.");
            expect(! labels.contains("1.1.05"), "Zoomed-out labels should not show frame subdivisions.");
        }

        beginTest("Visible grid snapping follows the displayed fine level");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::barsBeatsFrames);
            Helpers::setProjectFps(*edit, 50.0);

            editViewState.zoom.setViewWidthPx(1000);
            editViewState.zoom.setRange({te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(1.0)});

            TimelineGrid grid(editViewState);
            expectWithinAbsoluteError(grid.snapTimeToVisibleGrid(te::TimePosition::fromSeconds(0.029)).inSeconds(),
                                      0.02, 1.0e-6,
                                      "Zoomed-in snapping should use visible frame grid lines.");

            editViewState.zoom.setRange({te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(16.0)});
            expectWithinAbsoluteError(grid.snapTimeToVisibleGrid(te::TimePosition::fromSeconds(0.26)).inSeconds(),
                                      0.5, 1.0e-6,
                                      "Zoomed-out snapping should use visible beat grid lines.");
        }

        beginTest("Short subdivision groups retain single-frame ticks and snapping at deep zoom");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(250.0);
            Helpers::setProjectFps(*edit, 50.0);
            Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::barsBeatsFrames);
            te::SelectionManager selection(engine);
            EditViewState view(*edit, selection);
            view.zoom.setViewWidthPx(1200);
            view.zoom.setRange({te::TimePosition(), te::TimeDuration::fromSeconds(0.24)});
            TimelineGrid grid(view);

            // 12 FPB defaults to 3+3+3+3; explicit patterns may have even shorter groups.
            for (const auto& pattern : {std::vector<int>{3, 3, 3, 3}, std::vector<int>{1, 2, 9}}) {
                Helpers::setGridSubdivisionPattern(*edit, pattern);
                const auto ticks = grid.getTicks();
                for (int frame = 0; frame < 12; ++frame) {
                    expect(std::any_of(ticks.begin(), ticks.end(), [frame](const auto& tick) {
                        return tick.x == frame * 100;
                    }), "Each frame should have a visible tick");
                    const auto time = te::TimePosition::fromSeconds(frame / 50.0);
                    expectWithinAbsoluteError(grid.snapTimeToVisibleGrid(time).inSeconds(), time.inSeconds(), 1.0e-9);
                }
            }
        }

        beginTest("Seeking across a tempo change preserves the visible grid and its snapping");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);
            edit->tempoSequence.insertTempo(te::BeatPosition::fromBeats(4.0), 3000.0 / 26.0, 1.0f);
            Helpers::setProjectFps(*edit, 50.0);
            Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::barsBeatsFrames);
            te::SelectionManager selection(engine);
            EditViewState view(*edit, selection);
            view.zoom.setViewWidthPx(1000);
            view.zoom.setRange({te::TimePosition(), te::TimeDuration::fromSeconds(4.0)});
            MessageManager::getInstance()->runDispatchLoopUntil(50);
            TimelineGrid grid(view);
            const auto before = grid.getTicks();
            expect(std::any_of(before.begin(), before.end(), [](const auto& tick) { return tick.x == 20; }));

            edit->getTransport().setPosition(te::TimePosition::fromSeconds(2.1));
            MessageManager::getInstance()->runDispatchLoopUntil(50);
            expectWithinAbsoluteError(grid.snapTimeToVisibleGrid(te::TimePosition::fromSeconds(0.08)).inSeconds(),
                                      0.08, 1.0e-9, "Seeking must not change the grid under a stationary viewport");

            // Rebuild the cache at the same view range: its ticks must still match.
            view.zoom.setRange(view.zoom.getRange());
            MessageManager::getInstance()->runDispatchLoopUntil(50);
            const auto after = grid.getTicks();
            expectEquals(int(after.size()), int(before.size()));
            for (size_t i = 0; i < std::min(before.size(), after.size()); ++i) {
                expectEquals(after[i].x, before[i].x);
                expectEquals(after[i].level, before[i].level);
                expectEquals(after[i].label, before[i].label);
            }
        }

        beginTest("Grid supplies frame label candidates near beat edges for ruler layout");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::barsBeatsFrames);
            Helpers::setProjectFps(*edit, 50.0);

            editViewState.zoom.setViewWidthPx(625);
            editViewState.zoom.setRange({te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(1.0)});

            TimelineGrid grid(editViewState);
            StringArray labels;

            for (const auto& tick : grid.getTicks()) {
                if (tick.label.isNotEmpty())
                    labels.add(tick.label);
            }

            expect(labels.contains("1.1.08"), "Expected middle-of-beat frame labels to remain visible.");
            expect(labels.contains("1.1.04"), "The ruler decides whether a label fits after the beat start.");
            expect(labels.contains("1.1.20"), "The ruler decides whether a label fits before the next beat.");
            expect(labels.contains("1.2"), "Expected native beat labels to remain visible.");
        }

        beginTest("Ruler displays the first frame after a beat when its label fits");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(100.0);
            Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::barsBeatsFrames);
            Helpers::setProjectFps(*edit, 50.0);
            te::SelectionManager selection(engine);
            EditViewState view(*edit, selection);
            view.zoom.setViewWidthPx(2400);
            view.zoom.setRange({te::TimePosition::fromSeconds(9.6), te::TimeDuration::fromSeconds(1.0)});
            TimelineGrid grid(view);
            RulerComponent ruler(*edit, view, grid);
            ruler.setSize(2400, 26);
            // Direct2D images (Windows) expose painted pixels only after the Graphics is destroyed.
            auto paintRuler = [&ruler] {
                Image result(Image::RGB, ruler.getWidth(), ruler.getHeight(), true);
                Graphics g(result);
                ruler.paint(g);
                return result;
            };
            const auto image = paintRuler();

            for (const auto& label : {String("5.1"), String("5.1.01"), String("5.1.02")}) {
                const auto ticks = grid.getTicks();
                const auto tick = std::find_if(ticks.begin(), ticks.end(), [&](const auto& t) {
                    return t.label == label;
                });
                expect(tick != ticks.end(), "Missing label candidate: " + label);
                if (tick == ticks.end())
                    continue;

                bool hasText = false;
                for (int y = 0; y < 10; ++y)
                    for (int x = tick->x + 2; x < tick->x + 30; ++x)
                        hasText = hasText || image.getPixelAt(x, y) != Colors::Theme::background;
                expect(hasText, "Label should be painted: " + label);
            }

            beginTest("Ruler keeps beat labels clear of overlapping preceding frame labels");
            edit->state.setProperty(IDs::gridSubdivision, "", nullptr);
            view.zoom.setViewWidthPx(600);
            view.zoom.setRange({te::TimePosition::fromSeconds(9.6), te::TimeDuration::fromSeconds(1.0)});
            ruler.setSize(600, 26);
            const auto denseImage = paintRuler();

            const auto denseTicks = grid.getTicks();
            const auto crowded = std::find_if(denseTicks.begin(), denseTicks.end(), [](const auto& tick) {
                return tick.label == "5.1.28";
            });
            expect(crowded != denseTicks.end(), "Expected a frame label candidate just before beat 5.2");
            if (crowded != denseTicks.end()) {
                bool hasCrowdedText = false;
                for (int y = 0; y < 10; ++y)
                    for (int x = crowded->x + 2; x < crowded->x + 20; ++x)
                        hasCrowdedText = hasCrowdedText || denseImage.getPixelAt(x, y) != Colors::Theme::background;
                expect(! hasCrowdedText, "The overlapping frame label should yield to the beat label");
            }
        }

        beginTest("Extended frames-only grid snaps use selected FPS");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::framesOnly);
            Helpers::setProjectFps(*edit, 50.0);

            editViewState.zoom.setViewWidthPx(1000);
            editViewState.zoom.setRange({te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(1.0)});

            TimelineGrid grid(editViewState);
            auto ticks = grid.getTicks();
            StringArray labels;

            auto hasTickAtX = [&ticks](int x) {
                return std::any_of(ticks.begin(), ticks.end(), [x](const auto& tick) {
                    return tick.x == x;
                });
            };

            for (const auto& tick : ticks) {
                if (tick.label.isNotEmpty())
                    labels.add(tick.label);
            }

            expect(hasTickAtX(20), "Expected a grid tick at the first 50 fps frame.");
            expect(hasTickAtX(40), "Expected a grid tick at the second 50 fps frame.");
            expect(hasTickAtX(60), "Expected a grid tick at the third 50 fps frame.");
            expect(! hasTickAtX(16), "Frames-only grid should not use musical beat ticks.");
            expect(labels.contains("0:00:05"), "Expected extended frames-only labels to use SMPTE-style frames.");
            expect(! labels.contains("1.1"), "Frames-only labels should not use bars/beats.");
        }

        beginTest("Subdivision pattern draws swing boundaries between frames and beats");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(3000.0 / 26.0);  // beat = 0.52s = 26 frames at 50 fps

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::barsBeatsFrames);
            Helpers::setProjectFps(*edit, 50.0);
            Helpers::setGridSubdivisionPattern(*edit, { 6, 7, 6, 7 });

            editViewState.zoom.setViewWidthPx(1000);
            editViewState.zoom.setRange({te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(1.0)});

            TimelineGrid grid(editViewState);
            auto ticks = grid.getTicks();
            StringArray labels;

            auto hasLabeledTickAtX = [&ticks](int x) {
                return std::any_of(ticks.begin(), ticks.end(), [x](const auto& tick) {
                    return tick.x == x && tick.level >= 1;
                });
            };

            for (const auto& tick : ticks) {
                if (tick.label.isNotEmpty())
                    labels.add(tick.label);
            }

            // 20 px per frame: subdivision boundaries at frames 6, 13, 19 of the first beat.
            expect(hasLabeledTickAtX(120), "Expected a subdivision tick at frame 6.");
            expect(hasLabeledTickAtX(260), "Expected a subdivision tick at frame 13.");
            expect(hasLabeledTickAtX(380), "Expected a subdivision tick at frame 19.");
            expect(! hasLabeledTickAtX(200), "Frame 10 is not a subdivision boundary.");
            expect(labels.contains("1.1.06"), "Expected subdivision labels with frame numbers.");
            expect(labels.contains("1.1.13"), "Expected subdivision labels with frame numbers.");
            expect(labels.contains("1.2"), "Beat labels should remain.");
        }

        beginTest("Subdivision pattern remains visible at very deep zoom");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(3000.0 / 26.0);  // beat = 0.52s = 26 frames at 50 fps

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::barsBeatsFrames);
            Helpers::setProjectFps(*edit, 50.0);
            Helpers::setGridSubdivisionPattern(*edit, { 6, 7, 6, 7 });

            editViewState.zoom.setViewWidthPx(2600);
            editViewState.zoom.setRange({te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(0.52)});

            TimelineGrid grid(editViewState);
            auto ticks = grid.getTicks();

            auto hasSubdivisionTickAtX = [&ticks](int x) {
                return std::any_of(ticks.begin(), ticks.end(), [x](const auto& tick) {
                    return tick.x == x && tick.level >= 1 && tick.label.isNotEmpty();
                });
            };

            // 100 px per frame: the medium pixel budget picks single frames, so the
            // milestone level must still promote 6/7/6/7 boundaries above frame ticks.
            expect(hasSubdivisionTickAtX(600), "Expected a subdivision tick at frame 6.");
            expect(hasSubdivisionTickAtX(1300), "Expected a subdivision tick at frame 13.");
            expect(hasSubdivisionTickAtX(1900), "Expected a subdivision tick at frame 19.");
        }

        beginTest("Tempo changes invalidate grid ticks");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setTimecodeDisplayMode(*edit, TimecodeDisplayMode::barsBeatsFrames);
            Helpers::setProjectFps(*edit, 50.0);

            editViewState.zoom.setViewWidthPx(1000);
            editViewState.zoom.setRange({te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(1.0)});

            class Probe : public TimelineGrid::Listener {
            public:
                bool changed = false;
                void gridChanged() override { changed = true; }
            };

            TimelineGrid grid(editViewState);
            Probe probe;
            grid.addListener(&probe);

            editViewState.setFramesPerBeat(30);
            MessageManager::getInstance()->runDispatchLoopUntil(50);

            expect(probe.changed, "Grid should notify listeners after tempo changes.");
            grid.removeListener(&probe);
        }
    }
};

static TimelineGridTest timelineGridTest;

}  // namespace MoTool
