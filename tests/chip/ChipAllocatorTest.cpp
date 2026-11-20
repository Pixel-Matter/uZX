#include "chip/ChipAllocator.h"

#include <array>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <string_view>

using namespace MoTool::uZX;

namespace {

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "ChipAllocatorTest: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void check(bool condition, std::string_view message) {
    if (!condition)
        fail(message);
}

NoteVoice voice(int channel, int key, int priority = 0) {
    return {.channel = channel, .key = key, .pitch = double(key), .level = 1.0f, .priority = priority};
}

NoteFrame frameOf(std::initializer_list<NoteVoice> items) {
    NoteFrame frame;
    for (const auto& item : items)
        frame.add(item);
    return frame;
}

using Ids = std::array<int, ChipAllocator::numToneChannels>;

Ids channelKeys(const ChipAllocator::Voices& voices) {
    Ids keys{};
    for (std::size_t i = 0; i < keys.size(); ++i)
        keys[i] = voices.channels[i] != nullptr ? voices.channels[i]->key : -1;
    return keys;
}

void expectChannels(const ChipAllocator::Voices& voices, Ids expected, std::string_view frame) {
    if (channelKeys(voices) != expected) {
        std::cerr << "ChipAllocatorTest: wrong channel assignment in " << frame << ": got [";
        const auto actual = channelKeys(voices);
        for (std::size_t i = 0; i < actual.size(); ++i)
            std::cerr << (i == 0 ? "" : ", ") << actual[i];
        std::cerr << "]\n";
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main() {
    {
        ChipAllocator allocator;
        auto frame = frameOf({voice(1, 60), voice(1, 62), voice(1, 64)});
        const auto voices = allocator.allocate(frame);
        expectChannels(voices, {64, 62, 60}, "three equal voices");
    }
    {
        ChipAllocator allocator;
        auto frame = frameOf({voice(1, 60), voice(1, 62), voice(1, 64), voice(1, 65)});
        const auto voices = allocator.allocate(frame);
        expectChannels(voices, {65, 64, 62}, "later tie wins");
    }
    {
        ChipAllocator allocator;
        auto frame = frameOf({voice(1, 60, 9), voice(1, 62), voice(1, 64), voice(1, 65)});
        const auto voices = allocator.allocate(frame);
        expectChannels(voices, {60, 65, 64}, "higher priority wins");
    }

    {
        ChipAllocator allocator;
        auto driver = voice(9, 36, 10);
        driver.timbre.tone = false;
        driver.timbre.drivesEnvelope = true;
        auto frame = frameOf({voice(1, 60, 3), voice(2, 62, 2), voice(3, 64, 1), driver});
        const auto voices = allocator.allocate(frame);
        expectChannels(voices, {60, 62, 64}, "envelope driver outside channel pool");
        check(voices.envelope != nullptr && voices.envelope->channel == 9,
              "highest-priority envelope driver should be independent of channels");
    }

    {
        ChipAllocator allocator;
        auto lower = voice(1, 60, 1);
        lower.timbre.noise = true;
        auto better = voice(2, 62, 8);
        better.timbre.noise = true;
        auto frame = frameOf({lower, better, voice(3, 64, 2), voice(4, 65, 3)});
        const auto voices = allocator.allocate(frame);
        expectChannels(voices, {62, 65, 64}, "noise voice selection");
        check(voices.noise != nullptr && voices.noise->channel == 2,
              "best selected voice with noise should drive noise");
    }

    {
        ChipAllocator allocator;
        auto right = voice(1, 60);
        right.pan = 1.0f;
        auto frame = frameOf({right});
        auto voices = allocator.allocate(frame);
        expectChannels(voices, {-1, -1, 60}, "default right pan");
        allocator.setChannelPans({0.0, 1.0, 0.5});
        frame = frameOf({right});
        voices = allocator.allocate(frame);
        expectChannels(voices, {-1, 60, -1}, "configured right pan");
    }

    {
        ChipAllocator allocator;
        auto frame = frameOf({voice(1, 60)});
        auto voices = allocator.allocate(frame);
        expectChannels(voices, {60, -1, -1}, "first frame");
        frame = frameOf({voice(1, 60), voice(2, 62)});
        voices = allocator.allocate(frame);
        expectChannels(voices, {60, 62, -1}, "held placement");

        auto panned = voice(1, 60);
        panned.pan = 1.0f;
        frame = frameOf({panned});
        voices = allocator.allocate(frame);
        expectChannels(voices, {-1, -1, 60}, "pan moves held voice");
        frame = NoteFrame{};
        voices = allocator.allocate(frame);
        expectChannels(voices, {-1, -1, -1}, "silence");
        frame = frameOf({voice(1, 60)});
        voices = allocator.allocate(frame);
        expectChannels(voices, {60, -1, -1}, "silence releases the previous slot for the same voice");
    }

    std::cout << "ChipAllocatorTest: all checks passed\n";
    return EXIT_SUCCESS;
}
