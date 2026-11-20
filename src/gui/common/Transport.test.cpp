#include "Transport.h"
#include "../../models/PsgFrameRetimer.h"

#include <JuceHeader.h>

namespace MoTool {

class TransportBarTests : public UnitTest
{
public:
    TransportBarTests() : UnitTest("TransportBar", "MoTool") {}

    void runTest() override {
        auto& engine = *tracktion::Engine::getEngines()[0];

        beginTest("The Loop button is square and lit while the transport loops, however looping is turned on");
        {
            auto edit = tracktion::Edit::createSingleTrackEdit(engine);
            auto& transport = edit->getTransport();
            transport.looping = true;
            tracktion::SelectionManager selectionManager(engine);
            EditViewState viewState(*edit, selectionManager);
            TransportBar bar(viewState, {.showRecord = false, .showAutomation = false});
            bar.setSize(1200, TransportBar::height);

            Button* loop = nullptr;
            for (auto* child : bar.getChildren())
                if (child->getName() == "Loop")
                    loop = dynamic_cast<Button*>(child);
            expect(loop != nullptr);
            if (loop == nullptr)
                return;

            expectEquals(loop->getWidth(), TransportBar::buttonSize);
            expectEquals(loop->getHeight(), TransportBar::buttonSize);
            expect(loop->getToggleState(), "looping when the bar is made");
            transport.looping = false;  // as Cmd+L or a double-click on the ruler's loop strip
            expect(!loop->getToggleState());
            transport.looping = true;
            expect(loop->getToggleState());
        }

        beginTest("Without a groove, as at a prime fpb, the groove readout shows the beat as one group");
        {
            auto edit = tracktion::Edit::createSingleTrackEdit(engine);
            PsgTiming::setFramesPerBeat(*edit, 23, false);  // no default groove: 23 splits into no equal groups
            tracktion::SelectionManager selectionManager(engine);
            EditViewState viewState(*edit, selectionManager);
            TransportBar bar(viewState, {.showRecord = false, .showAutomation = false});
            bar.setSize(1200, TransportBar::height);

            // The readout is the box just left of its "groove" label
            Label* unit = nullptr;
            for (auto* child : bar.getChildren())
                if (auto* label = dynamic_cast<Label*>(child); label != nullptr && label->getText() == "groove")
                    unit = label;
            expect(unit != nullptr);
            if (unit == nullptr)
                return;
            Label* groove = nullptr;
            for (auto* child : bar.getChildren())
                if (auto* label = dynamic_cast<Label*>(child); label != nullptr && label->getRight() <= unit->getX()
                    && (groove == nullptr || label->getRight() > groove->getRight()))
                    groove = label;
            expect(groove != nullptr);
            if (groove != nullptr)
                expectEquals(groove->getText(), String("23"));
        }

        beginTest("Each readout's value and unit say in a tooltip how to change it, and a screen reader what it is");
        {
            auto edit = tracktion::Edit::createSingleTrackEdit(engine);
            tracktion::SelectionManager selectionManager(engine);
            EditViewState viewState(*edit, selectionManager);
            TransportBar bar(viewState, {.showRecord = false, .showAutomation = false});

            const StringArray unitNames {"bpm", "fps", "fpb", "groove", "sig", "pos"};
            int units = 0, editable = 0;
            for (auto* child : bar.getChildren()) {
                auto* label = dynamic_cast<Label*>(child);
                if (label == nullptr)
                    continue;
                if (unitNames.contains(label->getText())) {
                    ++units;
                    expect(label->getTooltip().contains("click to"), label->getText());
                }
                if (label->isEditableOnSingleClick()) {
                    ++editable;
                    expect(label->getTooltip().contains("click to type"), label->getText());
                    expect(label->getDescription().isNotEmpty(), "a screen reader says what the value is");
                }
            }
            expectEquals(units, unitNames.size());
            expectEquals(editable, 5, "bpm, fpb, groove, sig and pos");
            for (auto* child : bar.getChildren())
                if (auto* knob = dynamic_cast<Slider*>(child))
                    expectEquals(knob->getTitle(), String("Master volume"));
        }

        beginTest("No button, knob or menu in the bar takes the keys: after a click they still play and step");
        {
            auto edit = tracktion::Edit::createSingleTrackEdit(engine);
            tracktion::SelectionManager selectionManager(engine);
            EditViewState viewState(*edit, selectionManager);
            TransportBar bar(viewState);  // Studio's, with the record and automation buttons
            int controls = 0;
            for (auto* c : bar.getChildren())
                if (dynamic_cast<Button*>(c) != nullptr || dynamic_cast<Slider*>(c) != nullptr
                    || dynamic_cast<MenuReadout*>(c) != nullptr) {
                    ++controls;
                    expect(! c->getWantsKeyboardFocus(), c->getName());
                }
            expect(controls >= 9, "rewind, play, record, loop, follow, read, write, volume and fps");
        }

        beginTest("The Player shows no readouts before a tune is open, only values a tune has given");
        {
            const auto bpmShown = [](TransportBar& bar) {
                for (auto* child : bar.getChildren())
                    if (auto* label = dynamic_cast<Label*>(child); label != nullptr && label->getText() == "bpm")
                        return label->isVisible();
                return false;
            };
            tracktion::SelectionManager selectionManager(engine);

            auto empty = tracktion::Edit::createSingleTrackEdit(engine);
            EditViewState emptyView(*empty, selectionManager);
            TransportBar player(emptyView, {.showRecord = false, .showAutomation = false, .readoutsNeedClips = true});
            expect(! bpmShown(player), "no 120 bpm that no tune gave it");
            TransportBar studio(emptyView, {.showRecord = false, .showAutomation = false});
            expect(bpmShown(studio), "Studio sets a tempo before there are clips");

            auto withTune = tracktion::Edit::createSingleTrackEdit(engine);
            auto* track = tracktion::getAudioTracks(*withTune).getFirst();
            track->insertMIDIClip({tracktion::TimePosition(), tracktion::TimePosition::fromSeconds(1.0)}, nullptr);
            EditViewState tuneView(*withTune, selectionManager);
            TransportBar playing(tuneView, {.showRecord = false, .showAutomation = false, .readoutsNeedClips = true});
            expect(bpmShown(playing));
        }

        beginTest("The transport buttons sit in the middle of the bar; in a narrow one, clear of the output section");
        {
            auto edit = tracktion::Edit::createSingleTrackEdit(engine);
            tracktion::SelectionManager selectionManager(engine);
            EditViewState viewState(*edit, selectionManager);
            TransportBar bar(viewState);  // Studio's, the widest

            const auto find = [&](const String& name) -> Component* {
                for (auto* child : bar.getChildren())
                    if (child->getName() == name)
                        return child;
                return nullptr;
            };
            auto* rewind = find("Rewind");
            auto* follow = find("Follow");
            expect(rewind != nullptr && follow != nullptr);
            if (rewind == nullptr || follow == nullptr)
                return;

            bar.setSize(1600, TransportBar::height);
            expectWithinAbsoluteError((rewind->getX() + follow->getRight()) / 2, 800, 1);

            bar.setSize(800, TransportBar::height);  // the narrowest window
            int outputLeft = bar.getWidth();
            for (auto* child : bar.getChildren())
                if (dynamic_cast<CpuLoadMeter*>(child) != nullptr)
                    outputLeft = child->getX();
            for (auto* child : bar.getChildren())
                if (child->isVisible() && child->getX() < outputLeft)
                    expect(child->getRight() <= outputLeft, "nothing runs under the CPU meter");
        }
    }
};

static TransportBarTests transportBarTests;

}  // namespace MoTool
