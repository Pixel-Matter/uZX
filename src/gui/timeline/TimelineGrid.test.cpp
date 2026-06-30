#include <JuceHeader.h>

#include "TimelineGrid.h"
#include "../../models/EditUtilities.h"

namespace MoTool {

class TimelineGridTest : public UnitTest {
public:
    TimelineGridTest() : UnitTest("TimelineGrid", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];

        beginTest("Bars beats frames grid uses frame subdivisions when zoomed in below a beat");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);

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
            expect(labels.contains("1.1.05"), "Expected ruler labels to use 50 fps frame counts.");
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
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);

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

        beginTest("Bars beats frames coarse frame labels stay clear of beat edges");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);

            editViewState.zoom.setViewWidthPx(625);
            editViewState.zoom.setRange({te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(1.0)});

            TimelineGrid grid(editViewState);
            StringArray labels;

            for (const auto& tick : grid.getTicks()) {
                if (tick.label.isNotEmpty())
                    labels.add(tick.label);
            }

            expect(labels.contains("1.1.10"), "Expected middle-of-beat frame labels to remain visible.");
            expect(! labels.contains("1.1.05"), "Coarse frame labels should not crowd the beat start.");
            expect(! labels.contains("1.1.20"), "Coarse frame labels should not crowd the next beat.");
            expect(labels.contains("1.2"), "Expected native beat labels to remain visible.");
        }

        beginTest("Extended frames-only grid snaps use selected FPS");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::fps50);

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

        beginTest("Tempo changes invalidate grid ticks");
        {
            auto edit = te::Edit::createSingleTrackEdit(engine);
            edit->tempoSequence.getTempo(0)->setBpm(120.0);

            te::SelectionManager selectionManager(engine);
            EditViewState editViewState(*edit, selectionManager);
            Helpers::setEditTimecodeFormat(*edit, TimecodeTypeExt::barsBeatsFps50);

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
