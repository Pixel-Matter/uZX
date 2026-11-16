#include "Ruler.h"

#include <JuceHeader.h>

#include "../../controllers/PlayWholeEdit.h"
#include "../../models/EditUtilities.h"
#include "PlayheadComponent.h"

namespace MoTool::Tests {

using namespace tracktion::literals;

class RulerLoopDragTests : public juce::UnitTest
{
public:
    RulerLoopDragTests() : UnitTest("RulerLoopDrag", "MoTool") {}

    void runTest() override {
        using Drag = RulerComponent::LoopDrag;
        const te::TimeRange loop{2_tp, 4_tp};
        const auto drag = [&](Drag d, te::TimePosition from, te::TimePosition to) {
            return RulerComponent::draggedLoop(d, loop, from, to);
        };

        beginTest("A drag on an empty part of the strip makes a range either way");
        expect(drag(Drag::create, 5_tp, 7_tp) == te::TimeRange(5_tp, 7_tp));
        expect(drag(Drag::create, 7_tp, 5_tp) == te::TimeRange(5_tp, 7_tp));

        beginTest("An edge moves alone, and crossing the other edge swaps them");
        expect(drag(Drag::start, 2_tp, 1_tp) == te::TimeRange(1_tp, 4_tp));
        expect(drag(Drag::end, 4_tp, 6_tp) == te::TimeRange(2_tp, 6_tp));
        expect(drag(Drag::start, 2_tp, 5_tp) == te::TimeRange(4_tp, 5_tp));

        beginTest("The range moves whole and stops at zero");
        expect(drag(Drag::move, 3_tp, 4_tp) == te::TimeRange(3_tp, 5_tp));
        expect(drag(Drag::move, 3_tp, 0_tp) == te::TimeRange(0_tp, 2_tp));

        // In a strip 800 pixels wide
        const auto at = [](float x, float x1, float x2, bool wholeProject = false) {
            return RulerComponent::loopDragAt(x, x1, x2, 800.0f, wholeProject);
        };

        beginTest("A drag inside a range moves it only while both its ends are in view");
        expect(at(300, 200, 400) == Drag::move);
        expect(at(300, 0, 800) == Drag::move, "a range that just fills the view");
        expect(at(300, -0.3f, 800.3f) == Drag::move, "give or take rounding");
        expect(at(300, -50, 400) == Drag::create, "its start is out of view");
        expect(at(300, 200, 900) == Drag::create, "its end is out of view");
        expect(at(300, -5000, 5000) == Drag::create, "zoomed in on a part of it");
        expect(at(600, 200, 400) == Drag::create, "outside it");

        beginTest("A drag on a range that is the whole project draws a new one");
        expect(at(300, 0, 800, true) == Drag::create, "zoomed to the project");
        expect(at(300, 0, 500, true) == Drag::create, "zoomed out past its end");

        beginTest("An edge is grabbed within a few pixels of it, whatever the range");
        expect(at(203, 200, 400) == Drag::start);
        expect(at(397, -50, 400) == Drag::end);
        expect(at(3, 0, 800, true) == Drag::start);
    }
};

static RulerLoopDragTests rulerLoopDragTests;

//==============================================================================
/** The loop strip under the mouse: a ruler 800 pixels wide over a project that ends off the grid. */
class RulerLoopStripTests : public juce::UnitTest
{
public:
    RulerLoopStripTests() : UnitTest("RulerLoopStrip", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        auto edit = te::Edit::createSingleTrackEdit(engine);
        edit->tempoSequence.getTempo(0)->setBpm(120.0);
        Helpers::setProjectFps(*edit, 50.0);
        const auto projectEnd = te::TimePosition::fromSeconds(7.913);
        te::getAudioTracks(*edit)[0]->insertMIDIClip({0_tp, projectEnd}, nullptr);

        te::SelectionManager selection(engine);
        EditViewState view(*edit, selection);
        view.zoom.setViewWidthPx(800);
        TimelineGrid grid(view);
        RulerComponent ruler(*edit, view, grid);
        ruler.setBounds(0, 0, 800, 24);

        auto& transport = edit->getTransport();
        const auto setUp = [&](te::TimeRange loop, te::TimeRange inView) {
            transport.setLoopRange(loop);
            transport.looping = false;
            view.zoom.setRange(inView);
        };
        const auto loopIs = [&](double start, double end) {
            const auto loop = transport.getLoopRange();
            return std::abs(loop.getStart().inSeconds() - start) < 1.0e-6
                   && std::abs(loop.getEnd().inSeconds() - end) < 1.0e-6;
        };
        const float strip = 18.0f, upperHalf = 6.0f;
        const te::TimeRange wholeProject{0_tp, projectEnd};

        beginTest("A double-click on the strip loops what is in view, between grid lines");
        setUp(wholeProject, {2_tp, 4_tp});
        ruler.mouseDoubleClick(mouseAt(ruler, 400, strip, 400, 2));
        expect(loopIs(2.0, 4.0), "grid lines on the view's borders are in view");
        expect(transport.looping);

        setUp(wholeProject, {te::TimePosition::fromSeconds(2.013), te::TimePosition::fromSeconds(3.987)});
        ruler.mouseDoubleClick(mouseAt(ruler, 400, strip, 400, 2));
        {
            const auto loop = transport.getLoopRange(), inView = view.zoom.getRange();
            expect(loop.getStart() >= inView.getStart() && loop.getEnd() <= inView.getEnd() && !loop.isEmpty());
            expect(grid.snapTimeToVisibleGrid(loop.getStart()) == loop.getStart());
            expect(grid.snapTimeToVisibleGrid(loop.getEnd()) == loop.getEnd());
        }

        beginTest("With the project's end in view the loop runs to it, though it is off the grid");
        setUp({2_tp, 4_tp}, wholeProject);
        ruler.mouseDoubleClick(mouseAt(ruler, 700, strip, 700, 2));  // beside the range
        expect(PlayWholeEdit::isWholeEdit(transport.getLoopRange(), projectEnd), "zoomed to the project: all of it");
        expect(transport.looping);

        setUp({2_tp, 4_tp}, {5_tp, 9_tp});
        ruler.mouseDoubleClick(mouseAt(ruler, 400, strip, 400, 2));
        expect(loopIs(5.0, 7.913));

        setUp({2_tp, 4_tp}, {8_tp, 9_tp});
        ruler.mouseDoubleClick(mouseAt(ruler, 400, strip, 400, 2));
        expect(loopIs(2.0, 4.0) && !transport.looping, "nothing of the project is in view");

        beginTest("A double-click on a range that could be moved, on an edge or in the upper half changes nothing");
        setUp({2_tp, 4_tp}, {0_tp, 8_tp});  // 100 pixels a second: the range spans 200 to 400
        ruler.mouseDoubleClick(mouseAt(ruler, 300, strip, 300, 2));
        ruler.mouseDoubleClick(mouseAt(ruler, 200, strip, 200, 2));
        ruler.mouseDoubleClick(mouseAt(ruler, 600, upperHalf, 600, 2));
        expect(loopIs(2.0, 4.0) && !transport.looping);

        beginTest("Drawing a range turns looping on; moving one doesn't");
        const auto drag = [&](float from, float to) {
            ruler.mouseDown(mouseAt(ruler, from, strip, from));
            ruler.mouseDrag(mouseAt(ruler, to, strip, from));
            ruler.mouseUp(mouseAt(ruler, to, strip, from));
        };
        drag(300, 350);
        expect(loopIs(2.5, 4.5) && !transport.looping);
        drag(500, 600);
        expect(loopIs(5.0, 6.0) && transport.looping);

        beginTest("Clear Loop in the menu puts the range back over the whole project and turns looping off");
        const auto clearLoop = [&] {
            const auto menu = ruler.createMenu();
            for (PopupMenu::MenuItemIterator items(menu); items.next();)
                if (items.getItem().text == "Clear Loop")
                    return items.getItem();
            return PopupMenu::Item();
        };
        expect(clearLoop().isEnabled, "a range is set and looped");
        clearLoop().action();
        expect(PlayWholeEdit::isWholeEdit(transport.getLoopRange(), projectEnd) && !transport.looping);
        expect(!clearLoop().isEnabled, "nothing left to clear");
        transport.looping = true;
        expect(clearLoop().isEnabled, "the whole project, looped");

        beginTest("A double-click reaches the strip through the playhead line its first click put there");
        PlayheadComponent playhead(*edit, view, grid);
        Component timeline;
        timeline.setBounds(0, 0, 800, 200);
        timeline.addAndMakeVisible(ruler);
        timeline.addAndMakeVisible(playhead);
        playhead.setBounds(timeline.getLocalBounds());
        setUp(wholeProject, {2_tp, 4_tp});
        playhead.mouseDoubleClick(mouseAt(playhead, 400, strip, 400, 2));
        expect(loopIs(2.0, 4.0) && transport.looping);
    }

private:
    /** A left-button event at (x, y) of a press at (downX, y). */
    static MouseEvent mouseAt(Component& c, float x, float y, float downX, int clicks = 1) {
        const auto now = Time::getCurrentTime();
        return {Desktop::getInstance().getMainMouseSource(),
                {x, y},
                ModifierKeys(ModifierKeys::leftButtonModifier),
                MouseInputSource::defaultPressure,
                MouseInputSource::defaultOrientation,
                MouseInputSource::defaultRotation,
                MouseInputSource::defaultTiltX,
                MouseInputSource::defaultTiltY,
                &c,
                &c,
                now,
                {downX, y},
                now,
                clicks,
                ! juce::approximatelyEqual(x, downX)};
    }
};

static RulerLoopStripTests rulerLoopStripTests;

}  // namespace MoTool::Tests
