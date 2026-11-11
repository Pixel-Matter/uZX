#include <JuceHeader.h>

#include "TurboSound.h"

namespace MoTool::uZX {

class TurboSoundTest : public juce::UnitTest {
public:
    TurboSoundTest() : UnitTest("TurboSound", "MoTool") {}

    void runTest() override {
        beginTest("Siblings with equal frame counts form a set, in chip order");
        {
            Dir dir;
            const auto s1 = dir.psg("song_1.psg", 3);
            const auto s2 = dir.psg("song_2.psg", 3);
            expect(findTurboSoundSet(s1) == Array<File>({s1, s2}));
            expect(findTurboSoundSet(s2) == Array<File>({s1, s2}));
        }

        beginTest("Each separator works");
        for (auto sep : {".", "-", " ", "_"}) {
            Dir dir;
            const auto a = dir.psg("song" + String(sep) + "1.psg", 2);
            const auto b = dir.psg("song" + String(sep) + "2.psg", 2);
            expect(findTurboSoundSet(b) == Array<File>({a, b}), "separator '" + String(sep) + "'");
        }

        beginTest("Three chips, and a gap");
        {
            Dir dir;
            const auto s1 = dir.psg("song_1.psg", 2);
            const auto s2 = dir.psg("song_2.psg", 2);
            const auto s3 = dir.psg("song_3.psg", 2);
            expect(findTurboSoundSet(s2) == Array<File>({s1, s2, s3}));
        }
        {
            Dir dir;
            const auto s1 = dir.psg("song_1.psg", 2);
            const auto s3 = dir.psg("song_3.psg", 2);
            expect(findTurboSoundSet(s1) == Array<File>({s1, s3}));
        }

        beginTest("A sibling a frame longer or shorter still belongs: an emulator stopped mid-frame");
        {
            Dir dir;
            const auto s1 = dir.psg("song.1.psg", 4);
            const auto s2 = dir.psg("song.2.psg", 3);
            expect(findTurboSoundSet(s1) == Array<File>({s1, s2}));
            expect(findTurboSoundSet(s2) == Array<File>({s1, s2}));
        }
        {
            const auto dir = File(UZX_FIXTURES_DIR).getChildFile("psg");
            const auto s1 = dir.getChildFile("200_percent_partyfinal.1.psg");
            const auto s2 = dir.getChildFile("200_percent_partyfinal.2.psg");
            expect(findTurboSoundSet(s2) == Array<File>({s1, s2}), "200%: 37,156 and 37,155 frames");
        }

        beginTest("Siblings with another frame count are left out");
        {
            Dir dir;
            const auto s1 = dir.psg("song_1.psg", 2);
            const auto s2 = dir.psg("song_2.psg", 2);
            dir.psg("song_3.psg", 5);
            expect(findTurboSoundSet(s1) == Array<File>({s1, s2}));
        }
        {
            Dir dir;
            const auto s1 = dir.psg("song_1.psg", 2);
            dir.psg("song_2.psg", 5);
            expect(findTurboSoundSet(s1).isEmpty());
        }

        beginTest("Siblings with another header are left out");
        {
            Dir dir;
            const auto s1 = dir.psg("song_1.psg", 2);
            const auto s2 = dir.psg("song_2.psg", 2);
            dir.psg("song_3.psg", 2, 0, 25);
            expect(findTurboSoundSet(s1) == Array<File>({s1, s2}), "interrupt rate");
        }
        {
            Dir dir;
            const auto s1 = dir.psg("song_1.psg", 2);
            const auto s2 = dir.psg("song_2.psg", 2);
            dir.psg("song_3.psg", 2, 1);
            expect(findTurboSoundSet(s1) == Array<File>({s1, s2}), "version");
        }

        beginTest("Unreadable siblings are left out");
        {
            Dir dir;
            const auto s1 = dir.psg("song_1.psg", 2);
            dir.root.getChildFile("song_2.psg").replaceWithText("definitely not a PSG file");
            expect(findTurboSoundSet(s1).isEmpty());
        }

        beginTest("Names that do not follow the pattern give no set");
        {
            Dir dir;
            const auto a = dir.psg("song1.psg", 2);
            dir.psg("song2.psg", 2);
            expect(findTurboSoundSet(a).isEmpty(), "no separator");
            const auto b = dir.psg("song_4.psg", 2);
            dir.psg("song_1.psg", 2);
            expect(findTurboSoundSet(b).isEmpty(), "chip 4");
            const auto z = dir.psg("song_0.psg", 2);
            expect(findTurboSoundSet(z).isEmpty(), "chip 0");
            const auto n = dir.psg("tune_a.psg", 2);
            dir.psg("tune_b.psg", 2);
            expect(findTurboSoundSet(n).isEmpty(), "no trailing digit");
        }

        beginTest("Set names");
        expectEquals(getSetName({File("/x/song_1.psg"), File("/x/song_2.psg")}), String("song"));
        expectEquals(getSetName({File("/x/song.1.psg"), File("/x/song.2.psg")}), String("song"));
        expectEquals(getSetName({File("/x/song 1.psg"), File("/x/song 2.psg")}), String("song"));
        expectEquals(getSetName({File("/x/a.psg"), File("/x/b.psg")}), String("a"));
        expectEquals(getSetName({File("/x/song_1.psg")}), String("song_1"));
    }

private:
    /** A fresh temp directory that deletes itself. */
    struct Dir {
        File root = File::getSpecialLocation(File::tempDirectory).getNonexistentChildFile("uzx-ts", "", false);

        Dir() { root.createDirectory(); }
        ~Dir() { root.deleteRecursively(); }

        /** Writes a PSG with `frames` one-register frames. */
        File psg(const String& name, int frames, uint8 version = 0, uint8 rate = 0) const {
            std::vector<uint8> bytes {'P', 'S', 'G', 0x1A, version, rate, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            for (int i = 0; i < frames; ++i)
                bytes.insert(bytes.end(), {0xFF, 0x08, 0x0F});
            auto file = root.getChildFile(name);
            file.replaceWithData(bytes.data(), bytes.size());
            return file;
        }
    };
};

static TurboSoundTest turboSoundTest;

}  // namespace MoTool::uZX
