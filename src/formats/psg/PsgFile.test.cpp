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
    }
};

static PsgFileTest psgFileTest;

}  // namespace MoTool::uZX
