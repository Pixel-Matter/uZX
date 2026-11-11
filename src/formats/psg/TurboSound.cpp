#include "TurboSound.h"
#include "PsgFile.h"

#include <cstdlib>

namespace MoTool::uZX {

static const String separators {"_-. "};

Array<File> findTurboSoundSet(const File& file) {
    const auto name = file.getFileNameWithoutExtension();
    const auto length = name.length();
    if (length < 2 || ! separators.containsChar(name[length - 2]))
        return {};

    const auto chip = static_cast<int>(name.getLastCharacter()) - '0';
    if (chip < 1 || chip > 3)
        return {};

    PsgFile opened(file);
    if (! opened.ensureRead())
        return {};

    // An emulator stopped mid-frame can leave one chip's file a frame longer ("200%" by Abstract 64 et al.).
    const auto matches = [&opened] (PsgFile& other) {
        const auto& a = opened.getHeader();
        const auto& b = other.getHeader();
        const auto frames = static_cast<long>(opened.getData().frames.size());
        return other.getFile().existsAsFile() && other.ensureRead()
            && std::abs(static_cast<long>(other.getData().frames.size()) - frames) <= 1
            && a.version == b.version && a.interruptRate == b.interruptRate;
    };

    Array<File> set;
    for (int i = 1; i <= 3; ++i) {
        if (i == chip) {
            set.add(file);
            continue;
        }
        const auto sibling = file.getSiblingFile(name.dropLastCharacters(1) + String(i) + file.getFileExtension());
        PsgFile psg(sibling);
        if (matches(psg))
            set.add(sibling);
    }
    return set.size() > 1 ? set : Array<File>();
}

String getSetName(const Array<File>& files) {
    const auto first = files.getFirst().getFileNameWithoutExtension();
    auto common = first;
    for (const auto& f : files) {
        const auto name = f.getFileNameWithoutExtension();
        int i = 0;
        while (i < common.length() && i < name.length() && common[i] == name[i])
            ++i;
        common = common.substring(0, i);
    }
    if (common == first)
        return first;

    common = common.trimCharactersAtEnd(separators);
    return common.isNotEmpty() ? common : first;
}

}  // namespace MoTool::uZX
