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
            bar.setSize(1200, 40);

            Button* loop = nullptr;
            for (auto* child : bar.getChildren())
                if (child->getName() == "Loop")
                    loop = dynamic_cast<Button*>(child);
            expect(loop != nullptr);
            if (loop == nullptr)
                return;

            expectEquals(loop->getWidth(), loop->getHeight());
            expect(loop->getToggleState(), "looping when the bar is made");
            transport.looping = false;  // as Cmd+L or a double-click on the ruler's loop strip
            expect(!loop->getToggleState());
            transport.looping = true;
            expect(loop->getToggleState());
        }
    }
};

static TransportBarTests transportBarTests;

}  // namespace MoTool
