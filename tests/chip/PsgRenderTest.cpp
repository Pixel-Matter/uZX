#include "../../src/chip/ChipAllocator.h"
#include "../../src/chip/NoteFrameClock.h"
#include "../../src/chip/PsgRegisterWriter.h"
#include "../../src/chip/PsgRenderer.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using namespace MoTool::uZX;

namespace {

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "PsgRenderTest: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void check(bool condition, std::string_view message) {
    if (!condition)
        fail(message);
}

NoteVoice voice(int channel, int key, double pitch, float level, int priority, bool tone, bool noise,
                int noisePeriod) {
    NoteVoice value{.channel = channel, .key = key, .pitch = pitch, .level = level, .priority = priority};
    value.timbre.tone = tone;
    value.timbre.noise = noise;
    value.timbre.noisePeriod = noisePeriod;
    return value;
}

NoteFrame fullFrame(bool retrigger) {
    NoteFrame frame;
    frame.add(voice(1, 60, 60.0, 1.0f, 1, true, true, 3));
    frame.add(voice(2, 62, 62.0, 0.5f, 8, true, true, 9));
    frame.add(voice(3, 64, 64.0, 1.0f, 4, true, false, 0));
    frame.add(voice(4, 65, 65.0, 1.0f, 3, true, false, 0));
    auto driver = voice(9, 36, 36.0, 1.0f, 10, false, false, 0);
    driver.timbre.drivesEnvelope = true;
    driver.timbre.envelopeShape = EnvelopeShapeEnum::TriangleDown;
    driver.restartEnvelope = retrigger;
    frame.add(driver);
    return frame;
}

std::vector<NoteFrame> fixtureFrames() {
    std::vector<NoteFrame> frames;
    frames.push_back(fullFrame(true));
    frames.push_back(fullFrame(true));  // Repeated trigger forces shape register write again.
    frames.push_back(fullFrame(false)); // Held shape remains unchanged and writes nothing.
    frames.emplace_back();             // Silence releases all channel volumes.
    NoteFrame driverOnly;
    auto driver = voice(9, 36, 36.0, 1.0f, 10, false, false, 0);
    driver.timbre.drivesEnvelope = true;
    driver.timbre.envelopeShape = EnvelopeShapeEnum::TriangleDown;
    driverOnly.add(driver);
    frames.push_back(driverOnly); // The envelope can continue without taking a tone channel.
    return frames;
}

int periodFor(double pitch, PsgPeriodMode mode) {
    constexpr double clock = 1773400.0;
    const double frequency = 440.0 * std::pow(2.0, (pitch - 69.0) / 12.0);
    const double divider = mode == PsgPeriodMode::Tone ? 16.0
        : mode == PsgPeriodMode::Envelope ? 256.0
                                          : 512.0;
    return static_cast<int>(std::floor(clock / (divider * frequency) + 0.5));
}

PsgRegisterFrame runFrame(ChipAllocator& allocator, PsgRegisterWriter& writer, const NoteFrame& frame) {
    const auto voices = allocator.allocate(frame);
    const auto patch = renderPsg(voices, periodFor);
    return writer.write(patch);
}

void expectWrites(const PsgRegisterFrame& frame, std::initializer_list<std::pair<int, int>> writes,
                  std::string_view label) {
    std::array<bool, 14> expectedMask{};
    std::array<std::uint8_t, 14> expectedValues{};
    for (const auto& [reg, value] : writes) {
        expectedMask[static_cast<std::size_t>(reg)] = true;
        expectedValues[static_cast<std::size_t>(reg)] = static_cast<std::uint8_t>(value);
    }
    for (std::size_t reg = 0; reg < expectedMask.size(); ++reg) {
        if (frame.isSet(reg) != expectedMask[reg])
            fail(std::string(label) + ": wrong write mask at register " + std::to_string(reg));
        if (expectedMask[reg] && frame.getRaw(reg) != expectedValues[reg])
            fail(std::string(label) + ": wrong value at register " + std::to_string(reg));
    }
}

struct Event
{
    std::uint64_t sample;
    PsgRegisterFrame writes;
};

std::vector<Event> runClocked(std::uint32_t sampleRate, std::uint32_t frameRate,
                              const std::vector<std::uint32_t>& blockSizes) {
    auto frames = fixtureFrames();
    ChipAllocator allocator;
    PsgRegisterWriter writer;
    NoteFrameClock clock(sampleRate, frameRate);
    std::vector<Event> events;
    std::size_t nextFrame = 0;
    std::size_t block = 0;
    while (nextFrame < frames.size()) {
        clock.advance(blockSizes[block++ % blockSizes.size()], [&](std::uint64_t sample) {
            if (nextFrame < frames.size())
                events.push_back({sample, runFrame(allocator, writer, frames[nextFrame++])});
        });
    }
    return events;
}

void expectInitialWrites(const PsgRegisterFrame& frame, std::string_view label) {
    expectWrites(frame,
                 {{0, 121}, {1, 1}, {2, 80}, {3, 1}, {4, 61}, {5, 1}, {6, 9}, {7, 48},
                  {8, 8}, {9, 15}, {10, 15}, {11, 53}, {12, 0}, {13, 10}},
                 label);
}

void expectGoldenEvents(const std::vector<Event>& events, const std::array<std::uint64_t, 5>& samples) {
    check(events.size() == samples.size(), "clock should produce exactly five fixture ticks");
    for (std::size_t i = 0; i < samples.size(); ++i)
        check(events[i].sample == samples[i], "tick must use the same absolute sample position across blocks");

    // Captured desktop oracle at clock 1773400, A4=440 and channel pans 0, .5, 1.
    expectInitialWrites(events[0].writes, "initial frame");
    expectWrites(events[1].writes, {{13, 10}}, "repeated retrigger");
    expectWrites(events[2].writes, {}, "held envelope shape");
    expectWrites(events[3].writes, {{8, 0}, {9, 0}, {10, 0}}, "silence");
    expectWrites(events[4].writes, {}, "independent envelope continuation");
}

}  // namespace

int main() {
    // The first three voices with sound take the channels by priority. The separate higher-priority
    // envelope driver remains the envelope source; only the allocated noise voice sets R6.
    expectGoldenEvents(runClocked(44100, 50, {128}), {0, 882, 1764, 2646, 3528});
    expectGoldenEvents(runClocked(48000, 50, {257}), {0, 960, 1920, 2880, 3840});
    expectGoldenEvents(runClocked(22050, 60, {127, 241, 64}), {0, 368, 735, 1103, 1470});

    // Splitting just before/after a tick cannot move it into a neighboring block or duplicate it.
    expectGoldenEvents(runClocked(44100, 50, {881, 1, 1}), {0, 882, 1764, 2646, 3528});

    {
        NoteFrameClock clock(100, 50);
        std::vector<std::uint64_t> ticks;
        clock.advance(2, [&](std::uint64_t sample) { ticks.push_back(sample); });  // [0, 2)
        check(ticks == std::vector<std::uint64_t>{0}, "tick at the exact block end belongs to the next block");
        clock.advance(1, [&](std::uint64_t sample) { ticks.push_back(sample); });  // [2, 3)
        check(ticks == (std::vector<std::uint64_t>{0, 2}), "next nonempty block receives the boundary tick");
    }

    {
        auto frames = fixtureFrames();
        ChipAllocator allocator;
        PsgRegisterWriter writer;
        auto first = runFrame(allocator, writer, frames.front());
        expectInitialWrites(first, "pre-reset frame");
        writer.reset();
        auto restarted = runFrame(allocator, writer, frames[2]);
        expectInitialWrites(restarted, "post-reset frame");
    }

    {
        PsgRegisterWriter writer;
        PsgPatch patch;
        patch.channels[0].volume = 5;
        patch.channels[0].tonePeriod = 0x0345;
        patch.channels[0].toneOn = true;
        patch.channels[0].noiseOn = false;
        expectWrites(writer.write(patch), {{0, 0x45}, {1, 0x03}, {7, 0x3e}, {8, 5}, {9, 0}, {10, 0}},
                     "sparse initial patch");

        patch.channels[0].tonePeriod = 0x03ab;
        expectWrites(writer.write(patch), {{0, 0xab}, {1, 0x03}},
                     "period change writes the unchanged coarse byte with the new fine byte");
    }

    std::cout << "PsgRenderTest: all checks passed\n";
    return EXIT_SUCCESS;
}
