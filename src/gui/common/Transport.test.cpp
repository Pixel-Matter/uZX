#include "Transport.h"

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
