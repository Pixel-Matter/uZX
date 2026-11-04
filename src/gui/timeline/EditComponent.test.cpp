#include <JuceHeader.h>

#include "EditComponent.h"
#include "../devices/TrackDevicesPanel.h"
#include "../../plugins/uZX/aychip/AYPluginEditor.h"
#include "PsgClipComponent.h"
#include "../../util/Helpers.h"
#include "../../util/TestHelpers.h"

namespace MoTool::Tests {

using namespace tracktion::literals;

class EditComponentTests : public juce::UnitTest {
public:
    EditComponentTests() : UnitTest("EditComponent", "MoTool") {}

    void runTest() override {
        auto& engine = *te::Engine::getEngines()[0];
        runLayoutTests(engine);
        runMinimumHeightTests(engine);
        runPlayerOverlayTests(engine);

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
        beginTest("Player layout stays out of undo, ends track rows in their devices, and leaves Studio's headers alone");
        auto edit = te::Edit::createSingleTrackEdit(engine);
        te::SelectionManager selectionManager(engine);
        edit->getUndoManager().clearUndoHistory();
        {
            EditViewState playerView(*edit, selectionManager);
            playerView.showHeaders = false;  // as the Player does
            playerView.showFooters = true;
            EditComponent player(*edit, playerView, {.showDetailsPanel = false, .autoFitTrackHeights = true});
            player.setSize(1000, 300);
            player.setSize(1000, 400);  // auto-fit resizes the tracks
            TestHelpers::flushMessageQueue();

            // the timeline stops before the devices at the end of each track row
            const int timelineRight = 1000 - EditViewState::footerWidth;
            expectEquals(findChild<RulerComponent>(player)->getRight(), timelineRight, "ruler");
            auto* row = findDescendant<TrackRowComponent>(player);
            expect(row != nullptr && row->footer != nullptr, "track row with devices");
            if (row != nullptr && row->footer != nullptr) {
                expectEquals(row->footer->getX(), timelineRight, "devices start");
                expectEquals(row->footer->getRight(), row->getWidth(), "devices end");
                expectEquals(row->body.getRight(), timelineRight, "clips end");
            }
        }
        expect(!edit->getUndoManager().canUndo());
        EditViewState studioView(*edit, selectionManager);
        expectEquals(studioView.getTrackHeaderWidth(), EditViewState::minHeadersWidth);
    }

    void runMinimumHeightTests(te::Engine& engine) {
        beginTest("Fitted rows share the height in proportion, none below its minimum");
        {
            using Heights = Array<int>;
            const auto fit = TracksContainerComponent::fitHeights;
            expect(fit({100, 300}, {50, 50}, 800) == Heights({200, 600}), "in proportion");
            expect(fit({100, 300}, {250, 250}, 600) == Heights({250, 350}), "held at the minimum");
            expect(fit({0, 0, 0}, {10, 10, 10}, 100) == Heights({33, 33, 34}), "the last takes the rounding");
        }

        beginTest("Player rows keep the AY panel's minimum height and scroll when the view is shorter");
        auto edit = te::Edit::createSingleTrackEdit(engine);
        edit->ensureNumberOfAudioTracks(2);
        for (auto* track : te::getAudioTracks(*edit))
            Helpers::ensureAYPluginOnTrack(*track);
        te::SelectionManager selectionManager(engine);
        EditViewState playerView(*edit, selectionManager);
        playerView.showHeaders = false;
        playerView.showFooters = true;
        EditComponent player(*edit, playerView, {.showDetailsPanel = false, .autoFitTrackHeights = true});
        player.setSize(1000, 1000);
        TestHelpers::flushMessageQueue();

        auto* row = findDescendant<TrackRowComponent>(player);
        auto* devices = row != nullptr ? row->footer.get() : nullptr;
        expect(devices != nullptr);
        if (devices == nullptr)
            return;
        auto* chip = findDescendant<uZX::AYPluginUI>(*devices);
        expect(chip != nullptr && chip->getMinimumHeight() > 0);
        const int minimum = row->getMinimumHeight();
        expectEquals(minimum, chip->getMinimumHeight() + TitleBar::height + 2 * ClipComponent::ringWidth
                                  + TrackRowComponent::resizerHeight);

        player.setSize(1000, 300);  // two rows can't fit
        TestHelpers::flushMessageQueue();
        auto* tracks = findChild<Viewport>(player);
        expect(tracks->getVerticalScrollBar().isVisible(), "tracks scroll");
        expect(row->getHeight() >= minimum, "rows keep their minimum");
        expectEquals(findChild<RulerComponent>(player)->getRight(),
                     1000 - EditViewState::footerWidth - player.getLookAndFeel().getDefaultScrollbarWidth(),
                     "the scrollbar takes the right edge from the timeline");

        player.setSize(1000, 1000);
        TestHelpers::flushMessageQueue();
        expect(! tracks->getVerticalScrollBar().isVisible(), "tracks fit again");
    }

    void runPlayerOverlayTests(te::Engine& engine) {
        beginTest("Without clip headers the notes start at the top, with one legend and the tune's key");
        auto edit = te::Edit::createSingleTrackEdit(engine);
        edit->ensureNumberOfAudioTracks(2);
        Array<PsgClip*> clips;
        for (auto* track : te::getAudioTracks(*edit)) {
            const uZX::PsgRegsFrame tone {{uZX::PsgRegType::Mixer, 0x3e}, {uZX::PsgRegType::TonePeriodFineA, 0xfc},
                                          {uZX::PsgRegType::VolumeA, 15}};
            uZX::PsgData data {std::vector<uZX::PsgRegsFrame>(50, tone), {50.0, 1}};
            clips.add(PsgClip::insertTo(*track, data, {{0_tp, 1_td}, {}}, "PSG").get());
        }
        te::SelectionManager selectionManager(engine);
        EditViewState playerView(*edit, selectionManager);
        playerView.showHeaders = false;
        playerView.showFooters = true;
        playerView.showClipHeaders = false;
        EditComponent player(*edit, playerView, {.showDetailsPanel = false, .autoFitTrackHeights = true});
        player.setSize(1000, 1000);
        TestHelpers::flushMessageQueue();

        expect(findDescendant<ClipHeader>(player) == nullptr, "no clip headers");
        auto* notes = findDescendant<PsgClipComponent>(player);
        expect(notes != nullptr && notes->getY() == ClipComponent::ringWidth, "notes at the top of the track");
        int legends = 0;
        for (auto* child : player.getChildren())
            legends += dynamic_cast<PsgLegendComponent*>(child) != nullptr ? 1 : 0;
        expectEquals(legends, 1);

        auto* setInfo = findChild<PsgSetInfoComponent>(player);
        expect(setInfo != nullptr);
        const auto together = detectTuningTogether(clips);
        expect(together.hasNotes);
        if (setInfo != nullptr && together.hasNotes) {
            expect(setInfo->getText().startsWith(together.getTableLabel()), setInfo->getText());
            expect(setInfo->getText().contains(together.keys.front().getName()), setInfo->getText());
            expectEquals(setInfo->getX(), findChild<RulerComponent>(player)->getRight(), "in the corner above the devices");
        }
    }

    template <typename T>
    static T* findDescendant(Component& parent) {
        for (auto* child : parent.getChildren()) {
            if (auto* c = dynamic_cast<T*>(child))
                return c;
            if (auto* c = findDescendant<T>(*child))
                return c;
        }
        return nullptr;
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
