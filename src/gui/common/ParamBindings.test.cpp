#include <JuceHeader.h>

#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>


namespace MoTool::Tests {

using namespace MoTool;
using namespace tracktion::literals;
using namespace tracktion;
using namespace juce;
using namespace std::literals;

//==============================================================================
//==============================================================================
class WidgetBindingTests  : public UnitTest {

public:
    WidgetBindingTests() : UnitTest("CustomClip", "MoTool") {}

    void runTest() override {
        auto& engine = *Engine::getEngines()[0];
        Clipboard clipboard;
        auto edit = Edit::createSingleTrackEdit(engine);

        beginTest("WidgetParamBinding creation");
        {
            expect(true, "TODO");
        }
    }
};

static WidgetBindingTests widgetBindingTests;

}  // namespace MoTool::Tests