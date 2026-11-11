#include "TuningCsv.h"

#include "../../utils/StringLiterals.h"

namespace MoTool::TuningCsv {

static String asciiNoteName(const String& name) {
    auto ascii = name.replace("♯"_u, "#").replace("♭"_u, "b");
    if (ascii.length() == 2)
        return ascii.substring(0, 1) + "-" + ascii.substring(1);
    return ascii;
}

String format(const std::vector<Row>& rows) {
    String csv = "MIDI,Note,Period\n";
    for (const auto& row : rows)
        csv += String(row.midiNote) + "," + asciiNoteName(row.noteName) + "," + String(row.period) + "\n";
    return csv;
}

String fileName(const String& description) {
    auto name = description.replaceCharacters("/\\:*?<>|#", "---------").replaceCharacter('"', '\'');
    while (name.contains("  "))
        name = name.replace("  ", " ");
    while (name.contains("--"))
        name = name.replace("--", "-");
    return name + ".csv";
}

}  // namespace MoTool::TuningCsv
