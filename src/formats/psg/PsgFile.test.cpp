#include <JuceHeader.h>

#include "PsgFile.h"

namespace MoTool::uZX {

class PsgFileTest : public juce::UnitTest {
public:
    PsgFileTest() : UnitTest("PsgFile", "MoTool") {}

    void runTest() override {
        const std::vector<uint8> header {'P', 'S', 'G', 0x1A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        const auto readable = [] (std::vector<uint8> bytes) {
            TemporaryFile temp(".psg");
            temp.getFile().replaceWithData(bytes.data(), bytes.size());
            PsgFile file(temp.getFile());
            return file.ensureRead();
        };
        const auto withHeader = [&header] (std::initializer_list<uint8> data) {
            auto bytes = header;
            bytes.insert(bytes.end(), data);
            return bytes;
        };

        beginTest("Files with nothing playable are unreadable");
        expect(! PsgFile(File::getSpecialLocation(File::tempDirectory).getChildFile("missing.psg")).ensureRead());
        expect(! readable({'n', 'o', 't', ' ', 'a', ' ', 'P', 'S', 'G', ' ', 'f', 'i', 'l', 'e', '!', '!', '!'}));
        expect(! readable(header), "Header without frames");

        beginTest("Frames are readable, and survive a damaged tail");
        expect(readable(withHeader({0xFF, 0x07, 0x38, 0xFF, 0x08, 0x0F})));
        expect(readable(withHeader({0xFF, 0x07, 0x38, 0xFD})), "0xFD end marker keeps the frames before it");

        beginTest("I/O port registers 14 and 15 are skipped");
        {
            TemporaryFile temp(".psg");
            const auto bytes = withHeader({0xFF, 0x0E, 0xAA, 0x0F, 0xBB, 0x07, 0x38});
            temp.getFile().replaceWithData(bytes.data(), bytes.size());
            PsgFile file(temp.getFile());
            expect(file.ensureRead());
            const auto& frame = file.getData().frames[0];
            expect(! frame.isSet(0) && ! frame.isSet(1), "R14/R15 must not leak into other registers");
            expect(frame.isSet(7));
        }

        beginTest("A pause adds exactly 4 * n empty frames and no extra frame before the next marker");
        {
            TemporaryFile temp(".psg");
            // frame, pause of 2 (8 empty frames), frame
            const auto bytes = withHeader({0xFF, 0x08, 0x0F, 0xFE, 0x02, 0xFF, 0x08, 0x00});
            temp.getFile().replaceWithData(bytes.data(), bytes.size());
            PsgFile file(temp.getFile());
            expect(file.ensureRead());
            const auto& frames = file.getData().frames;
            expectEquals((int) frames.size(), 10);
            expect(frames[0].isSet(8) && frames[9].isSet(8));
            for (size_t i = 1; i < 9; ++i)
                expect(frames[i].isEmpty(), "pause frames must be empty");
        }

        beginTest("Register pairs right after a pause land in the last paused frame");
        {
            TemporaryFile temp(".psg");
            const auto bytes = withHeader({0xFF, 0x08, 0x0F, 0xFE, 0x01, 0x0A, 0x04, 0xFF, 0x08, 0x00});
            temp.getFile().replaceWithData(bytes.data(), bytes.size());
            PsgFile file(temp.getFile());
            expect(file.ensureRead());
            const auto& frames = file.getData().frames;
            expectEquals((int) frames.size(), 6);
            expect(frames[4].isSet(10) && frames[4].getRaw(10) == 4);
            expect(frames[5].isSet(8));
        }

        beginTest("Pause runs stop at the frame limit, keeping the frames before it");
        {
            TemporaryFile temp(".psg");
            auto bytes = withHeader({0xFF, 0x08, 0x0F});
            for (size_t i = 0; i <= PsgFile::maxFrames / 1020; ++i)
                bytes.insert(bytes.end(), {0xFE, 0xFF});  // 1,020 empty frames each
            temp.getFile().replaceWithData(bytes.data(), bytes.size());
            PsgFile file(temp.getFile());
            expect(file.ensureRead());
            expectEquals(file.getData().frames.size(), PsgFile::maxFrames);
            expect(file.getData().frames[0].isSet(8));
        }

        beginTest("A 0xFF version byte means frames start right after the magic");
        {
            TemporaryFile temp(".psg");
            const std::vector<uint8> bytes{'P', 'S', 'G', 0x1A, 0xFF, 0x08, 0x0F, 0xFF, 0x08, 0x00};
            temp.getFile().replaceWithData(bytes.data(), bytes.size());
            PsgFile file(temp.getFile());
            expect(file.ensureRead());
            expectEquals((int) file.getData().frames.size(), 2);
        }
    }
};

static PsgFileTest psgFileTest;

}  // namespace MoTool::uZX
