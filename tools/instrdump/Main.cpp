/** instrdump: the samples and ornaments of a ZX Spectrum AY tracker module, and how its patterns
    use them, as JSON on stdout.

        instrdump MODULE

    Reads what zxtune's AY parsers read: PT3, PT2, PT1, STC, ST3, ASC, STP, PSM, GTR, SQT, PSC, FTC,
    Vortex Tracker text, and Turbo Sound pairs of these (two modules in the output). Exits with 1
    when the file is none of them. Model.h describes the output.
*/
#include "Formats.h"

#include "binary/container_factories.h"
#include "formats/chiptune/aym/turbosound.h"
#include "formats/chiptune/decoders.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <vector>

namespace MoTool::InstrDump {

namespace {

namespace FC = Formats::Chiptune;

struct Format {
    FC::Decoder::Ptr decoder;
    Reader read;
};

/** In zxtune's detection order (third_party/zxtune/uzx/ay_module.cpp). */
std::vector<Format> allFormats() {
    std::vector<Format> formats;
    formats.push_back({FC::CreateProTracker3Decoder(), &readProTracker3});
    formats.push_back({FC::CreateVortexTracker2Decoder(), &readVortexTracker2});
    formats.push_back({FC::CreateProTracker2Decoder(), &readProTracker2});
    formats.push_back({FC::CreateSoundTrackerCompiledDecoder(), &readSoundTrackerCompiled});
    formats.push_back({FC::CreateSoundTrackerDecoder(), &readSoundTracker});
    formats.push_back({FC::CreateSoundTracker3Decoder(), &readSoundTracker3});
    formats.push_back({FC::CreateASCSoundMaster0xDecoder(), &readASCSoundMaster0});
    formats.push_back({FC::CreateASCSoundMaster1xDecoder(), &readASCSoundMaster1});
    formats.push_back({FC::CreateSoundTrackerProCompiledDecoder(), &readSoundTrackerPro});
    formats.push_back({FC::CreateProSoundMakerCompiledDecoder(), &readProSoundMaker});
    formats.push_back({FC::CreateGlobalTrackerDecoder(), &readGlobalTracker});
    formats.push_back({FC::CreateProTracker1Decoder(), &readProTracker1});
    formats.push_back({FC::CreateSQTrackerDecoder(), &readSQTracker});
    formats.push_back({FC::CreateProSoundCreatorDecoder(), &readProSoundCreator});
    formats.push_back({FC::CreateFastTrackerDecoder(), &readFastTracker});
    return formats;
}

std::optional<Module> readModule(const std::vector<Format>& formats, const Binary::Container& data) {
    for (const auto& format : formats) {
        if (!format.decoder->Check(data)) {
            continue;
        }
        Module module;
        if (format.read(data, module)) {
            module.format = format.decoder->GetDescription();
            return module;
        }
    }
    return {};
}

/** The two modules of a Turbo Sound file, which ends in a "02TS" footer. */
class TurboSoundParts : public FC::TurboSound::Builder {
public:
    void SetFirstSubmoduleLocation(std::size_t offset, std::size_t size) override { parts[0] = {offset, size}; }
    void SetSecondSubmoduleLocation(std::size_t offset, std::size_t size) override { parts[1] = {offset, size}; }

    struct Part {
        std::size_t offset = 0;
        std::size_t size = 0;
    };
    Part parts[2];
};

std::vector<Module> readFile(const Binary::Container& data) {
    const auto formats = allFormats();
    std::vector<Module> modules;
    if (FC::CreateTurboSoundDecoder()->Check(data)) {
        TurboSoundParts turboSound;
        if (FC::TurboSound::Parse(data, turboSound)) {
            for (const auto& part : turboSound.parts) {
                const auto chip = data.GetSubcontainer(part.offset, part.size);
                if (auto module = chip ? readModule(formats, *chip) : std::nullopt) {
                    modules.push_back(std::move(*module));
                }
            }
            if (modules.size() == 2) {
                return modules;
            }
            modules.clear();
        }
    }
    if (auto module = readModule(formats, data)) {
        modules.push_back(std::move(*module));
    }
    return modules;
}

}  // namespace

}  // namespace MoTool::InstrDump

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: instrdump MODULE\n";
        return 2;
    }
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << argv[1] << ": cannot open\n";
        return 2;
    }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::vector<MoTool::InstrDump::Module> modules;
    try {
        if (const auto data = Binary::CreateContainer(Binary::View(bytes.data(), bytes.size()))) {
            modules = MoTool::InstrDump::readFile(*data);
        }
    } catch (...) {
        // zxtune throws Error (not a std::exception) on some malformed data: not a module either
    }
    if (modules.empty()) {
        std::cerr << argv[1] << ": not a supported tracker module\n";
        return 1;
    }
    MoTool::InstrDump::writeJson(std::cout, modules);
    return 0;
}
