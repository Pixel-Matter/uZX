#include <JuceHeader.h>

#include "TrackerModule.h"

namespace MoTool::uZX {

class TrackerModuleTest : public juce::UnitTest {
public:
    TrackerModuleTest() : UnitTest("TrackerModule", "MoTool") {}

    void runTest() override {
        beginTest("Each format plays as zxtune's own register dump");
        for (const auto* name : {"Agent-X_-_Film_Music_002.stc", "13.pt2", "hypergy.pt3", ";-).pt3", "3-EYE.stp",
                                 "zx-sos.asc", "sol.psm", "little+girl+with+big+yellow+bow.txt"}) {
            const auto module = readTrackerModule(load(name));
            if (! expectSingleChip(module, name))
                continue;
            expectWithinAbsoluteError(module->chips[0].getFrameRate(), 50.0, 1e-9, name);
            expectSameFrames(module->chips[0].frames, readDebugAY(sample(name + String(".debugay"))), name);
        }

        beginTest("A module names its title, note table and rows");
        {
            using T = BuiltinTuningEnum;
            struct Expected {
                const char* file;
                const char* title;
                std::optional<T::Enum> table;  // none: a table uZX has no preset for
                std::vector<size_t> rowFrames;  // repeating from the first row
            };
            const Expected samples[] {
                {"Agent-X_-_Film_Music_002.stc", "Composed by AGENT-", T::CustomST, {5}},
                {"13.pt2", "", T::CustomPT_1_ST, {5}},
                {"hypergy.pt3", "hypergy #2", T::CustomPT_2_ASM, {5}},
                {"3-EYE.stp", "THRIRD EYE    MIC/MPS/96.", T::CustomST, {5}},
                {"zx-sos.asc", "ZX-SOS", std::nullopt, {5}},  // ASC's own "ASM" table
                {"sol.psm", "", std::nullopt, {6}},
                // speed commands B..2 and B..6 on alternate rows
                {"little+girl+with+big+yellow+bow.txt", "little girl with big yellow bow", T::CustomPT_2_ASM, {2, 6}},
            };
            for (const auto& expected : samples) {
                const auto module = readTrackerModule(load(expected.file));
                if (! expectSingleChip(module, expected.file))
                    continue;
                expectEquals(module->title, String(expected.title), expected.file);
                expect(module->table.has_value() == expected.table.has_value(), String(expected.file) + ": table");
                if (module->table && expected.table)
                    expectEquals(String(module->table->getLabel().data()),
                                 String(BuiltinTuningType(*expected.table).getLabel().data()), expected.file);
                expectRows(module->rowStarts, expected.rowFrames, expected.file);
            }
        }

        beginTest("A Turbo Sound pair plays both chips for the first module's length");
        {
            const auto first = load("hypergy.pt3");
            const auto second = load(";-).pt3");
            MemoryBlock pair(first);
            pair.append(second.getData(), second.getSize());
            pair.append(turboSoundFooter(first.getSize(), second.getSize()).getData(), 16);

            const auto module = readTrackerModule(pair);
            expect(module.has_value() && module->chips.size() == 2, "two chips");
            if (module && module->chips.size() == 2) {
                const auto alone = readTrackerModule(first);
                const auto& chips = module->chips;
                expectSameFrames(chips[0].frames, alone->chips[0].frames, "chip 1");
                const auto secondAlone = readTrackerModule(second)->chips[0].frames;
                expectEquals((int) chips[1].frames.size(), (int) chips[0].frames.size(), "chip 2 follows chip 1");
                const auto overlap = (long) std::min(secondAlone.size(), chips[1].frames.size());
                expectSameFrames({chips[1].frames.begin(), chips[1].frames.begin() + overlap}, secondAlone,
                                 "chip 2 until its own loop");
                expect(module->rowStarts == alone->rowStarts, "the first module's rows");
                expectEquals(module->title, String("hypergy #2/:-)"), "both titles, as they differ");
                expect(module->table.has_value(), "both on PT #2: one table");
            }
        }

        beginTest("Other data is not a module");
        expect(! readTrackerModule({}).has_value());
        expect(! readTrackerModule(MemoryBlock("not a tracker module at all", 27)).has_value());
        const uint8 psg[] {'P', 'S', 'G', 0x1A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0x07, 0x38};
        expect(! readTrackerModule(MemoryBlock(psg, sizeof(psg))).has_value(), "PSG files have their own reader");
    }

private:
    static File sample(const String& name) {
        return File(UZX_ZXTUNE_REGRESSION_DIR).getChildFile(name);
    }

    MemoryBlock load(const String& name) {
        MemoryBlock bytes;
        expect(sample(name).loadFileAsData(bytes), "missing sample " + name);
        return bytes;
    }

    /** zxtune's .debugay: a register header line, then a line per frame with each written register as
        two hex digits at its column and blanks elsewhere; "=" for a frame without writes. */
    static std::vector<PsgRegsFrame> readDebugAY(const File& file) {
        StringArray lines;
        file.readLines(lines);
        if (lines.size() > 0 && lines[lines.size() - 1].isEmpty())
            lines.remove(lines.size() - 1);

        std::vector<PsgRegsFrame> frames;
        for (int i = 1; i < lines.size(); ++i) {
            PsgRegsFrame frame {};
            for (size_t reg = 0; reg < frame.registers.size(); ++reg) {
                const auto hex = lines[i].substring((int) reg * 2, (int) reg * 2 + 2).trim();
                if (hex.length() == 2 && hex.containsOnly("0123456789ABCDEFabcdef")) {
                    frame.registers[reg] = (uint8) hex.getHexValue32();
                    frame.mask[reg] = true;
                }
            }
            frames.push_back(frame);
        }
        return frames;
    }

    /** "PT3!", first module's size, "PT3!", second's size (16-bit little endian), "02TS". */
    static MemoryBlock turboSoundFooter(size_t firstSize, size_t secondSize) {
        MemoryOutputStream footer;
        footer.write("PT3!", 4);
        footer.writeShort((short) firstSize);
        footer.write("PT3!", 4);
        footer.writeShort((short) secondSize);
        footer.write("02TS", 4);
        return footer.getMemoryBlock();
    }

    bool expectSingleChip(const std::optional<TrackerModule>& module, const String& name) {
        expect(module.has_value(), name + " reads as a module");
        if (! module)
            return false;
        expectEquals((int) module->chips.size(), 1, name);
        return module->chips.size() == 1;
    }

    /** Rows start at frame 0 and last rowFrames[i % size] frames each. */
    void expectRows(const std::vector<size_t>& starts, const std::vector<size_t>& rowFrames, const String& name) {
        expect(starts.size() > 1 && starts.front() == 0, name + ": rows from frame 0");
        for (size_t i = 1; i < starts.size(); ++i) {
            if (starts[i] - starts[i - 1] != rowFrames[(i - 1) % rowFrames.size()]) {
                expect(false, name + ": row " + String((int) i - 1) + " lasts " + String((int) (starts[i] - starts[i - 1])));
                return;
            }
        }
    }

    static bool sameFrame(const PsgRegsFrame& a, const PsgRegsFrame& b) {
        for (size_t reg = 0; reg < a.registers.size(); ++reg)
            if (a.mask[reg] != b.mask[reg] || (a.mask[reg] && a.registers[reg] != b.registers[reg]))
                return false;
        return true;
    }

    void expectSameFrames(const std::vector<PsgRegsFrame>& actual, const std::vector<PsgRegsFrame>& expected,
                          const String& what) {
        expectEquals((int) actual.size(), (int) expected.size(), what + ": frame count");
        for (size_t i = 0; i < std::min(actual.size(), expected.size()); ++i) {
            if (! sameFrame(actual[i], expected[i])) {
                expect(false, what + ": frame " + String((int) i) + " differs");
                return;
            }
        }
    }
};

static TrackerModuleTest trackerModuleTest;

}  // namespace MoTool::uZX
