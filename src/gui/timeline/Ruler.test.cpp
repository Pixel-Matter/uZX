#include "Ruler.h"

#include <JuceHeader.h>

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
    }
};

static RulerLoopDragTests rulerLoopDragTests;

}  // namespace MoTool::Tests
