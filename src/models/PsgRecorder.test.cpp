#include "PsgRecorder.h"

#include <JuceHeader.h>

namespace MoTool::Tests {

using namespace juce;

//==============================================================================
/** The PSG wire in, a PSG file's frames out. */
class PsgRecorderTests : public UnitTest
{
public:
    PsgRecorderTests() : UnitTest("PsgRecorder", "MoTool") {}

    void runTest() override {
        // "R8=12 R0=133" for each frame, "-" for one that writes nothing
        const auto describe = [](const uZX::PsgData& data) {
            StringArray frames;
            for (const auto& frame : data.frames) {
                StringArray writes;
                for (size_t reg = 0; reg < frame.size(); ++reg)
                    if (frame.mask[reg])
                        writes.add("R" + String(int(reg)) + "=" + String(int(frame.registers[reg])));
                frames.add(writes.isEmpty() ? String("-") : writes.joinIntoString(" "));
            }
            return frames.joinIntoString("; ");
        };
        constexpr int volume = 7, periodCoarse = 20, periodFine = 52, shape = 70, retrigger = 83;

        beginTest("A frame holds the registers written in it, and a write of what the chip holds is left out");
        {
            PsgRecorder recorder(50.0);
            recorder.add(0.0, 0, volume, 12);
            recorder.add(0.0, 0, periodCoarse, 3);
            recorder.add(0.0, 0, periodFine, 5);
            recorder.add(0.02, 0, volume, 12);
            recorder.add(0.02, 1, volume, 9);
            recorder.add(0.1, 0, volume, 0);
            const auto data = recorder.finish(0.16);
            expectEquals(describe(data), String("R0=133 R1=1 R8=12; R9=9; -; -; -; R8=0; -; -"));
            expectEquals(data.getFrameRate(), 50.0);
        }

        beginTest("The envelope shape is written when it is another, and again on a retrigger");
        {
            PsgRecorder recorder(50.0);
            recorder.add(0.0, 3, shape, 12);
            recorder.add(0.02, 3, shape, 12);
            recorder.add(0.04, 3, retrigger, 1);
            recorder.add(0.04, 3, retrigger, 0);
            expectEquals(describe(recorder.finish(0.06)), String("R13=12; -; R13=12"));
        }

        beginTest("A message a hair before a frame's time is on that frame, and several times share a frame");
        {
            PsgRecorder recorder(50.0);
            recorder.add(0.0199999999, 0, volume, 5);
            recorder.add(0.025, 1, volume, 6);
            recorder.add(0.039, 0, volume, 7);
            expectEquals(describe(recorder.finish(0.06)), String("-; R8=7 R9=6; -"));
        }

        beginTest("Registers written at the very end make one more frame: the end of a note is kept");
        {
            PsgRecorder recorder(50.0);
            recorder.add(0.0, 0, volume, 15);
            recorder.add(0.04, 0, volume, 0);
            expectEquals(describe(recorder.finish(0.04)), String("R8=15; -; R8=0"));
            expectEquals(describe(recorder.finish(0.04)), String("-; -"), "and the recorder starts over");
        }
    }
};

static PsgRecorderTests psgRecorderTests;

}  // namespace MoTool::Tests
