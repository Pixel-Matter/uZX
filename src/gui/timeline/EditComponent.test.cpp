#include <JuceHeader.h>

#include "EditComponent.h"
#include "../../util/TestHelpers.h"

namespace MoTool::Tests {

using namespace tracktion::literals;

class EditComponentTests : public juce::UnitTest {
public:
    EditComponentTests() : UnitTest("EditComponent", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        runLayoutTests(engine);

        auto edit = te::Edit::createSingleTrackEdit(engine);
        te::getAudioTracks(*edit)[0]->insertMIDIClip({0_tp, 10_tp}, nullptr);

        te::SelectionManager selectionManager(engine);
        EditViewState editViewState(*edit, selectionManager);
        auto& zoom = editViewState.zoom;
        // as in the Player: tracks fit the height and never scroll vertically
        EditComponent component(*edit, editViewState, {.showDetailsPanel = false, .autoFitTrackHeights = true});
        component.setSize(editViewState.getTrackHeaderWidth() + 1000, 300);
        zoom.setRange({0_tp, 2_td});

        auto* scrollBar = findChild<ScrollBar>(component);
        auto* tracks = findChild<Viewport>(component);

        beginTest("Time scrollbar spans the clips and shows the visible range");
        {
            expect(scrollBar != nullptr);
            expectEquals(scrollBar->getMaximumRangeLimit(), 10.0);
            expectWithinAbsoluteError(scrollBar->getCurrentRangeStart(), 0.0, 1e-9);
            expectWithinAbsoluteError(scrollBar->getCurrentRangeSize(), 2.0, 1e-9);
        }

        beginTest("Dragging the time scrollbar scrolls the timeline");
        {
            scrollBar->setCurrentRangeStart(3.0, sendNotificationSync);
            expectWithinAbsoluteError(zoom.getStart().inSeconds(), 3.0, 1e-9);
            expectWithinAbsoluteError(zoom.getViewSpan().inSeconds(), 2.0, 1e-9);
        }

        beginTest("Scrolling the timeline moves the time scrollbar");
        {
            zoom.setStart(5_tp);
            expectWithinAbsoluteError(scrollBar->getCurrentRangeStart(), 5.0, 1e-9);
        }

        beginTest("Time scrollbar grows when a clip is added past the end");
        {
            te::getAudioTracks(*edit)[0]->insertMIDIClip({20_tp, 30_tp}, nullptr);
            TestHelpers::flushMessageQueue();
            expectEquals(scrollBar->getMaximumRangeLimit(), 30.0);
        }

        beginTest("Playback follows again after a view drag, whatever the buttons at release");
        {
            auto& transport = edit->getTransport();
            const auto followsAfter = [&](auto releaseDrag) {
                transport.setPosition(0_tp);  // so moving to 1 s below is a change
                releaseDrag();
                transport.play(false);
                zoom.setRange({100_tp, 2_td});
                transport.setPosition(1_tp);
                transport.stop(false, false);
                return zoom.getStart() < 1_tp;
            };
            const ModifierKeys left {ModifierKeys::leftButtonModifier}, right {ModifierKeys::rightButtonModifier},
                middle {ModifierKeys::middleButtonModifier};

            auto* ruler = findChild<RulerComponent>(component);
            expect(followsAfter([&] {
                ruler->mouseDown(mouseEventFor(*ruler, {100, 10}, {100, 10}, left));
                ruler->mouseDrag(mouseEventFor(*ruler, {200, 10}, {100, 10}, left));
                ruler->mouseUp(mouseEventFor(*ruler, {200, 10}, {100, 10}, right));  // popup-menu modifier
            }), "ruler drag");

            Component& tracksArea = component;  // gets the tracks' mouse events
            expect(followsAfter([&] {
                tracksArea.mouseDown(mouseEventFor(*tracks, {100, 10}, {100, 10}, middle));
                tracksArea.mouseDrag(mouseEventFor(*tracks, {200, 10}, {100, 10}, middle));
                tracksArea.mouseUp(mouseEventFor(*tracks, {200, 10}, {100, 10}, left));
            }), "middle-button drag over the tracks");
        }

        beginTest("Wheel over the time scrollbar scrolls the timeline, as on the ruler");
        {
            zoom.setStart(5_tp);
            scrollBar->mouseWheelMove(mouseEventFor(*scrollBar), wheel(0.0f, -0.5f));
            expectGreaterThan(zoom.getStart().inSeconds(), 5.0);
        }

        beginTest("Wheel over tracks that never scroll vertically scrolls the timeline");
        {
            zoom.setStart(5_tp);
            tracks->mouseWheelMove(mouseEventFor(*tracks), wheel(0.0f, -0.5f));
            expectGreaterThan(zoom.getStart().inSeconds(), 5.0);
        }

        beginTest("Over scrollable tracks, wheel stays vertical and a sideways swipe scrolls the timeline");
        {
            EditComponent studio(*edit, editViewState, {.showDetailsPanel = false});
            studio.setSize(editViewState.getTrackHeaderWidth() + 1000, 300);
            auto* studioTracks = findChild<Viewport>(studio);

            zoom.setStart(5_tp);
            studioTracks->mouseWheelMove(mouseEventFor(*studioTracks), wheel(0.0f, -0.5f));
            expectWithinAbsoluteError(zoom.getStart().inSeconds(), 5.0, 1e-9);

            studioTracks->mouseWheelMove(mouseEventFor(*studioTracks), wheel(-0.5f, 0.1f));
            expectGreaterThan(zoom.getStart().inSeconds(), 5.0);
        }
    }

private:
    void runLayoutTests(te::Engine& engine) {
        beginTest("Player layout stays out of undo and leaves Studio's track headers alone");
        auto edit = te::Edit::createSingleTrackEdit(engine);
        te::SelectionManager selectionManager(engine);
        edit->getUndoManager().clearUndoHistory();
        {
            EditViewState playerView(*edit, selectionManager);
            playerView.showHeaders = false;  // as the Player does
            EditComponent player(*edit, playerView, {.showDetailsPanel = false, .autoFitTrackHeights = true});
            player.setSize(1000, 300);
            player.setSize(1000, 400);  // auto-fit resizes the tracks
            TestHelpers::flushMessageQueue();
        }
        expect(!edit->getUndoManager().canUndo());
        EditViewState studioView(*edit, selectionManager);
        expectEquals(studioView.getTrackHeaderWidth(), EditViewState::minHeadersWidth);
    }

    template <typename T>
    static T* findChild(Component& parent) {
        for (auto* child : parent.getChildren())
            if (auto* c = dynamic_cast<T*>(child))
                return c;
        return nullptr;
    }

    static MouseEvent mouseEventFor(Component& c, Point<float> pos = {10.0f, 10.0f},
                                    Point<float> downPos = {10.0f, 10.0f}, ModifierKeys mods = {}) {
        const auto now = Time::getCurrentTime();
        return {Desktop::getInstance().getMainMouseSource(), pos, mods,
                MouseInputSource::defaultPressure, MouseInputSource::defaultOrientation,
                MouseInputSource::defaultRotation, MouseInputSource::defaultTiltX, MouseInputSource::defaultTiltY,
                &c, &c, now, downPos, now, 1, pos != downPos};
    }

    static MouseWheelDetails wheel(float deltaX, float deltaY) {
        MouseWheelDetails w {};
        w.deltaX = deltaX;
        w.deltaY = deltaY;
        return w;
    }
};

static EditComponentTests editComponentTests;

}  // namespace MoTool::Tests
