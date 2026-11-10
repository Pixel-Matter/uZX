#include <JuceHeader.h>

#include "EditComponent.h"
#include "../devices/TrackDevicesPanel.h"
#include "../devices/LevelMeterUI.h"
#include "../../plugins/uZX/aychip/AYPluginEditor.h"
#include "PlayheadComponent.h"
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
        runVizModeTests(engine);

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

    void runVizModeTests(te::Engine& engine) {
        beginTest("Viz mode: no ruler or scrollbar, and each track row ends in its chip's scopes");
        auto edit = te::Edit::createSingleTrackEdit(engine);
        auto* track = te::getAudioTracks(*edit)[0];
        Helpers::ensureAYPluginOnTrack(*track);
        track->insertMIDIClip({te::TimePosition(), te::TimeDuration::fromSeconds(1.0)}, nullptr);
        te::SelectionManager selectionManager(engine);
        EditViewState vizView(*edit, selectionManager);
        vizView.showAsPlayer(true, {});
        EditComponent viz(*edit, vizView, {.showDetailsPanel = false, .autoFitTrackHeights = true});
        viz.setSize(1000, 400);
        TestHelpers::flushMessageQueue();

        expect(! findChild<RulerComponent>(viz)->isVisible(), "no ruler");
        expect(! findChild<ScrollBar>(viz)->isVisible(), "no scrollbar");
        expect(findChild<Viewport>(viz)->getBounds() == viz.getLocalBounds(), "the tracks fill the view");
        auto* row = findDescendant<TrackRowComponent>(viz);
        expect(row != nullptr && dynamic_cast<uZX::AYScopes*>(row->footer.get()) != nullptr, "scopes");
        expect(findDescendant<uZX::AYPluginUI>(viz) == nullptr, "no chip controls");

        vizView.vizScopesWidth = 400;  // what dragging the gap before the scopes does
        TestHelpers::flushMessageQueue();
        if (row != nullptr && row->footer != nullptr) {
            expectEquals(row->body.getRight(), row->getWidth() - 400, "the notes end where the scopes' column starts");
            expectEquals(row->footer->getX(), row->getWidth() - 400 + TrackRowComponent::footerPadding.getLeft(),
                         "the scopes start after the gap");
        }

        beginTest("Viz mode leaves out tracks without clips, and shows one once it gets a clip");
        {
            auto* empty = edit->insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(*edit), nullptr).get();
            TestHelpers::flushMessageQueue();
            expectEquals(countDescendants<TrackRowComponent>(viz), 1, "the empty track is left out");
            EditViewState studioView(*edit, selectionManager);
            EditComponent studio(*edit, studioView, {});
            TestHelpers::flushMessageQueue();
            expectEquals(countDescendants<TrackRowComponent>(studio), 2, "outside Viz mode it is shown");
            empty->insertMIDIClip({te::TimePosition(), te::TimeDuration::fromSeconds(1.0)}, nullptr);
            TestHelpers::flushMessageQueue();
            expectEquals(countDescendants<TrackRowComponent>(viz), 2, "with a clip it is shown");
            edit->deleteTrack(empty);
            TestHelpers::flushMessageQueue();
        }

        beginTest("Viz mode showing the notes alone: no scopes and no gap, the notes fill the row");
        {
            EditViewState notesView(*edit, selectionManager);
            notesView.showAsPlayer(true, {VizOptions::Layout::notes});
            EditComponent notes(*edit, notesView, {.showDetailsPanel = false, .autoFitTrackHeights = true});
            notes.setSize(1000, 400);
            TestHelpers::flushMessageQueue();
            auto* notesRow = findDescendant<TrackRowComponent>(notes);
            expect(notesRow != nullptr && notesRow->footer == nullptr, "no scopes");
            if (notesRow != nullptr)
                expectEquals(notesRow->body.getRight(), notesRow->getWidth(), "the notes reach the right edge");
            expect(findDescendant<TrackFooterOverlayComponent>(notes) == nullptr, "no gap to drag");
        }

        beginTest("Viz mode showing the scopes alone: they fill the row, with no notes, playhead or legend");
        {
            EditViewState scopesView(*edit, selectionManager);
            scopesView.showAsPlayer(true, {VizOptions::Layout::scopes});
            EditComponent scopes(*edit, scopesView, {.showDetailsPanel = false, .autoFitTrackHeights = true});
            scopes.setSize(1000, 400);
            TestHelpers::flushMessageQueue();
            auto* scopesRow = findDescendant<TrackRowComponent>(scopes);
            expect(scopesRow != nullptr && scopesRow->footer != nullptr && ! scopesRow->body.isVisible(),
                   "scopes only");
            if (scopesRow != nullptr && scopesRow->footer != nullptr) {
                const auto& pad = TrackRowComponent::footerPadding;
                expectEquals(scopesRow->footer->getX(), pad.getLeft(), "from the left");
                expectEquals(scopesRow->footer->getRight(), scopesRow->getWidth() - pad.getRight(), "to the right");
            }
            expect(findDescendant<TrackFooterOverlayComponent>(scopes) == nullptr, "no gap to drag");
            expect(! findChild<PlayheadComponent>(scopes)->isVisible(), "no playhead");
            expect(findChild<PsgLegendComponent>(scopes) == nullptr, "no legend");
        }

        beginTest("Scopes side by side: under each row's notes, A | B | C, the notes across the whole width");
        {
            EditViewState bandView(*edit, selectionManager);
            bandView.showAsPlayer(true, {.scopes = VizOptions::Scopes::sideBySide});
            bandView.vizScopesShare = 40;
            EditComponent band(*edit, bandView, {.showDetailsPanel = false, .autoFitTrackHeights = true});
            band.setSize(1000, 400);
            TestHelpers::flushMessageQueue();
            auto* bandRow = findDescendant<TrackRowComponent>(band);
            expect(bandRow != nullptr && bandRow->footer != nullptr, "scopes");
            if (bandRow != nullptr && bandRow->footer != nullptr) {
                expectEquals(bandRow->body.getRight(), bandRow->getWidth(), "the notes reach the right edge");
                expect(bandRow->footer->getY() > bandRow->body.getBottom(), "the scopes under the notes");
                expectEquals(bandRow->footer->getRight(), bandRow->getWidth() - 8, "apart from the window edge");
                const auto& scopes = bandRow->footer->getChildren();
                expect(scopes.size() == 3 && scopes[0]->getY() == scopes[2]->getY()
                           && scopes[0]->getRight() < scopes[1]->getX(), "A | B | C");

                const int scopesTop = bandRow->footer->getY();
                bandView.vizScopesShare = 60;  // what dragging the gap above the scopes does
                expect(bandRow->footer->getY() < scopesTop, "a bigger share raises the scopes");
            }
            expect(findDescendant<TrackFooterOverlayComponent>(band) == nullptr, "no column gap");
            expect(findChild<PlayheadComponent>(band)->getCoveredAreas != nullptr, "the playhead keeps off the scopes");
        }

        beginTest("Classic colours: the scopes are drawn on black");
        {
            EditViewState classicView(*edit, selectionManager);
            classicView.showAsPlayer(true, {VizOptions::Layout::scopes, VizOptions::Colours::classic});
            EditComponent classic(*edit, classicView, {.showDetailsPanel = false, .autoFitTrackHeights = true});
            classic.setSize(1000, 400);
            TestHelpers::flushMessageQueue();
            auto* scope = findDescendant<uZX::WaveformDisplay>(classic);
            expect(scope != nullptr, "a scope");
            if (scope != nullptr) {
                const auto image = scope->createComponentSnapshot(scope->getLocalBounds(), true, 1.0f);
                const auto pixel = image.getPixelAt(image.getWidth() - 1, image.getHeight() - 1);
                expect(pixel == Colours::black, pixel.toDisplayString(true));
            }
        }
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

        beginTest("A wider Player footer gives its width to the AY chip, and is never narrower than its minimum");
        playerView.footersWidth = 400;  // what dragging the gap before the devices does
        TestHelpers::flushMessageQueue();
        expectEquals(row->footer->getWidth(), 400);
        constexpr int padding = 8, spacing = 8, meter = 16;
        // found again: the devices may have been rebuilt meanwhile
        auto chipWidth = [&] { return findDescendant<uZX::AYPluginUI>(*row->footer)->getWidth(); };
        expect(findDescendant<LevelMeterUI>(*row->footer) != nullptr,
               "the level meter has its own UI: every device UI is linked in, on Linux too");
        expectEquals(chipWidth(), 400 - 2 * padding - spacing - meter, "the chip takes the rest");

        playerView.footersWidth = 100;
        TestHelpers::flushMessageQueue();
        expectEquals(row->footer->getWidth(), EditViewState::footerWidth);
        expectEquals(chipWidth(), EditViewState::footerWidth - 2 * padding - spacing - meter);
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
    static int countDescendants(Component& parent) {
        int count = 0;
        for (auto* child : parent.getChildren())
            count += (dynamic_cast<T*>(child) != nullptr ? 1 : 0) + countDescendants<T>(*child);
        return count;
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
