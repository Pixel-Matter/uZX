#include "PsgRowEvidence.h"

#include <JuceHeader.h>

#include "formats/psg/PsgData.h"

namespace MoTool::Tests {

using namespace MoTool::uZX;

//==============================================================================
class PsgRowEvidenceTests : public UnitTest
{
public:
    PsgRowEvidenceTests() : UnitTest("PsgRowEvidence", "MoTool") {}

    /** A channel cell's parts, as cellsOf packs them. */
    struct Parts
    {
        int volume, pitch, mixer, envelope;
    };

    static Parts partsOf(uint16_t cell) { return {cell / 243, cell / 27 % 9, cell / 3 % 9, cell % 3}; }

    /** Tone on every channel and no noise; channel A plays a note from frame 2, jumps to another
        at frame 10 and again at frame 11, and falls silent at frame 12. */
    static PsgData makeNote() {
        PsgData data;
        data.options = {50.0, 1};
        data.frames.resize(20);
        data.frames[0].setMixer(0x38);
        data.frames[2].setTonePeriod(0, 400);
        data.frames[2].setVolume(0, 15);
        data.frames[10].setTonePeriod(0, 300);
        data.frames[11].setTonePeriod(0, 200);
        data.frames[12].setVolume(0, 0);
        return data;
    }

    void runTest() override {
        beginTest("A channel's cell tells a note from silence, a held pitch that jumps, an ornament");
        {
            const auto cells = PsgRowEvidence::cellsOf(makeNote());
            expectEquals((int) cells.size(), 20);
            expectEquals((int) cells[0].channels[0], 0, "the first frame has nothing before it");
            expectEquals(partsOf(cells[2].channels[0]).volume, 4, "up by 4 or more from silence");
            expectEquals(partsOf(cells[2].channels[0]).pitch, 8, "a tone appears");
            expectEquals(partsOf(cells[5].channels[0]).pitch, 1, "the pitch holds");
            expectEquals(partsOf(cells[10].channels[0]).pitch, 7, "a jump after 5 or more held frames");
            expectEquals(partsOf(cells[11].channels[0]).pitch, 4, "a jump the frame after another");
            expectEquals(partsOf(cells[12].channels[0]).volume, 7, "fell silent");
            expectEquals((int) cells[5].channels[1], 0, "a silent channel has no event");
            expectEquals((int) cells[5].chip, 0, "nor has the chip");
        }

        beginTest("A channel's cell tells its noise, tone and envelope bit switching");
        {
            auto data = makeNote();
            data.frames[4].setMixer(0x30);  // noise on A
            data.frames[6].setVolumeAndEnvMod(0, 15, true);
            data.frames[8].setMixer(0x31);  // tone off A
            const auto cells = PsgRowEvidence::cellsOf(data);
            expectEquals(partsOf(cells[4].channels[0]).mixer, 3, "noise switched on");
            expectEquals(partsOf(cells[6].channels[0]).envelope, 1, "envelope bit set");
            expectEquals(partsOf(cells[8].channels[0]).mixer, 2, "tone switched off");
            expectEquals(partsOf(cells[4].channels[1]).mixer, 0, "a silent channel's mixer bits say nothing");
        }

        beginTest("The chip's cell tells an envelope restart, a repeated one, and one no channel uses");
        {
            auto data = makeNote();
            data.frames[3].setEnvelopeShape(8);  // no channel is on the envelope yet
            data.frames[6].setVolumeAndEnvMod(0, 15, true);
            data.frames[6].setEnvelopeShape(8);
            data.frames[7].setEnvelopeShape(8);
            data.frames[9].setEnvelopePeriod(400);
            const auto cells = PsgRowEvidence::cellsOf(data);
            expectEquals(cells[3].chip / 6, 3, "a restart no channel uses");
            expectEquals(cells[6].chip / 6, 1, "a restart");
            expectEquals(cells[7].chip / 6, 2, "a restart the frame after another");
            expectEquals(cells[9].chip / 2 % 3, 2, "the envelope period jumped");
        }

        beginTest("Evidence is never negative, and a channel that falls silent adds none");
        {
            const auto evidence = PsgRowEvidence::of(makeNote());
            expectEquals((int) evidence.size(), 20);
            for (auto value : evidence)
                expect(value >= 0.0);
            expectWithinAbsoluteError(evidence[12], 0.0, 1e-12, "the frame the note ends on");
            expectWithinAbsoluteError(evidence[5], 0.0, 1e-12, "a frame where nothing happens");
        }
    }
};

static PsgRowEvidenceTests psgRowEvidenceTests;

}  // namespace MoTool::Tests
