#include "TuningCsv.h"

#include <JuceHeader.h>

#include "../../utils/StringLiterals.h"

namespace MoTool {

class TuningCsvTest : public juce::UnitTest
{
public:
    TuningCsvTest() : UnitTest("TuningCsv", "MoTool") {}

    void runTest() override {
        beginTest("Rows spell note names in ASCII, padded to three characters");
        {
            expectEquals(TuningCsv::format({{60, "C4", 425}, {61, "C♯4"_u, 401}, {63, "E♭4"_u, 357}}),
                         String("MIDI,Note,Period\n60,C-4,425\n61,C#4,401\n63,Eb4,357\n"));
            expectEquals(TuningCsv::format({}), String("MIDI,Note,Period\n"));
        }

        beginTest("File names lose the characters file systems reject");
        {
            expectEquals(TuningCsv::fileName("A/B\\C:D*E?F<G>H|I#J\"K\""), String("A-B-C-D-E-F-G-H-I-J'K'.csv"));
            expectEquals(TuningCsv::fileName("Just  Intonation C#/Db  Major"),
                         String("Just Intonation C-Db Major.csv"));
        }
    }
};

static TuningCsvTest tuningCsvTest;

}  // namespace MoTool
