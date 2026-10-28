#include "EditState.h"

namespace MoTool {

class TrackViewStateTest : public UnitTest
{
public:
    TrackViewStateTest() : UnitTest("TrackViewState", "MoTool") {}

    void runTest() override {
        TrackViewState view(ValueTree(te::IDs::TRACK), nullptr);
        Component container;
        Component row;
        container.setSize(800, 1000);
        container.addChildComponent(row);
        row.setBounds(0, 100, 800, 160);

        auto resizeHeight = [&](int height) {
            view.getConstrainer().setBoundsForComponent(&row, row.getBounds().withHeight(height),
                                                       false, false, true, false);
        };

        beginTest("Track can grow beyond 600 pixels within its container");
        resizeHeight(700);
        expectEquals(row.getHeight(), 700);

        beginTest("Bottom-edge drag stops at the container bottom without moving the top");
        resizeHeight(2000);
        expectEquals(row.getY(), 100);
        expectEquals(row.getBottom(), container.getHeight());

        beginTest("Resize limit follows container size changes");
        container.setSize(800, 1200);
        resizeHeight(2000);
        expectEquals(row.getHeight(), 1100);

        beginTest("Scrolled rows keep their top position and bottom edge visible");
        row.setTopLeftPosition(0, -200);
        resizeHeight(2000);
        expectEquals(row.getY(), -200);
        expectEquals(row.getBottom(), container.getHeight());

        beginTest("Minimum track height remains enforced");
        resizeHeight(1);
        expectEquals(row.getHeight(), 56);
    }
};

static TrackViewStateTest trackViewStateTest;

}  // namespace MoTool
