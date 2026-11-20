#include "../../src/chip/web/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "WebEngineTest: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void check(bool condition, std::string_view message) {
    if (!condition)
        fail(message);
}

const MoTool::uZX::TraceSnapshot& trace(std::uint32_t index) {
    check(index < uzx_trace_count(), "trace index must exist");
    return uzx_trace_data()[index];
}

struct AudioStats
{
    bool leftNonzero = false;
    bool rightNonzero = false;
    bool finite = true;
    bool stereoDifference = false;
    float maxLeft = 0.0f;
    float maxRight = 0.0f;
};

AudioStats renderAndInspect(std::uint32_t totalSamples) {
    AudioStats stats;
    std::uint32_t rendered = 0;
    while (rendered < totalSamples) {
        const auto count = std::min<std::uint32_t>(128, totalSamples - rendered);
        uzx_render(count);
        const auto* left = uzx_audio_left();
        const auto* right = uzx_audio_right();
        for (std::uint32_t i = 0; i < count; ++i) {
            stats.finite &= std::isfinite(left[i]) && std::isfinite(right[i]);
            stats.leftNonzero |= std::abs(left[i]) > 1.0e-7f;
            stats.rightNonzero |= std::abs(right[i]) > 1.0e-7f;
            stats.stereoDifference |= std::abs(left[i] - right[i]) > 1.0e-7f;
            stats.maxLeft = std::max(stats.maxLeft, std::abs(left[i]));
            stats.maxRight = std::max(stats.maxRight, std::abs(right[i]));
        }
        rendered += count;
    }
    return stats;
}

void checkStereoAudio(const AudioStats& stats) {
    check(stats.finite, "rendered stereo PCM must be finite");
    check(stats.leftNonzero && stats.rightNonzero, "both stereo channels must contain audible samples");
    check(stats.stereoDifference, "panned chip output should differ between left and right");
}

void testBassAtRate(std::uint32_t sampleRate, std::uint32_t secondTick) {
    check(uzx_init(sampleRate, 0) == 1, "AY engine should initialize at supported sample rate");
    check(uzx_note_on(11, 0, 45.0f, 0.8f) == 1, "bass note should be accepted");
    const auto stats = renderAndInspect(1024);
    check(uzx_trace_count() == 2, "render should produce two note frames");
    const auto& first = trace(0);
    const auto& second = trace(1);
    checkStereoAudio(stats);
    check(first.sampleLow == 0 && first.sampleHigh == 0, "first tick starts at sample zero");
    check(second.sampleLow == secondTick, "second tick lands at the expected audio sample");
    check(first.count == 1 && first.voices[0].id == 11 && first.voices[0].instrument == 0,
          "bass note should appear in the ownership trace");
    check(std::find(first.channels.begin(), first.channels.end(), 11u) != first.channels.end(),
          "bass note should own a chip tone channel");
}

void testPluckTraceAndReset() {
    check(uzx_init(48000, 0) == 1, "engine should initialize for pluck test");
    check(uzx_note_on(21, 1, 60.0f, 0.8f) == 1, "pluck note should be accepted");
    const auto stats = renderAndInspect(2048);
    checkStereoAudio(stats);
    check(uzx_trace_count() == 3, "pluck render should produce three frames");
    const auto& attack = trace(0);
    check(attack.sampleLow == 0 && attack.count == 1, "pluck attack starts at sample zero");
    check(attack.envelope == 21 && attack.noise == 21 && attack.retrigger == 1,
          "pluck attack owns its transient envelope and noise");
    const auto& held = trace(1);
    check(held.sampleLow == 960 && held.envelope == 0 && held.retrigger == 0,
          "pluck decay releases the envelope after its attack");
    const auto& decayed = trace(2);
    check(decayed.sampleLow == 1920 && decayed.noise == 0,
          "pluck noise transient ends while the decaying tone remains");

    uzx_reset();
    check(uzx_trace_count() == 0, "reset clears trace history");
    check(uzx_dropped_note_count() == 0, "reset clears rejected-note count");
    check(uzx_note_on(21, 0, 45.0f, 0.8f) == 1, "reset clears the old gate ID for reuse");
    uzx_reset();
    uzx_render(1);
    check(uzx_trace_count() == 1 && trace(0).sampleLow == 0 && trace(0).count == 0,
          "reset removes sounding notes and restarts the trace clock");
}

void testNoteOffAndShortGate() {
    check(uzx_init(44100, 1) == 1, "YM engine should initialize");
    check(uzx_note_on(31, 0, 48.0f, 0.7f) == 1, "short bass gate should be accepted");
    uzx_note_off(31);
    const auto firstBlock = renderAndInspect(128);
    check(uzx_trace_count() == 1 && trace(0).count == 1,
          "a gate closed before its first tick still sounds its first frame");

    const auto restOfGate = renderAndInspect(896);
    AudioStats wholeGate;
    wholeGate.finite = firstBlock.finite && restOfGate.finite;
    wholeGate.leftNonzero = firstBlock.leftNonzero || restOfGate.leftNonzero;
    wholeGate.rightNonzero = firstBlock.rightNonzero || restOfGate.rightNonzero;
    wholeGate.stereoDifference = firstBlock.stereoDifference || restOfGate.stereoDifference;
    checkStereoAudio(wholeGate);
    check(uzx_trace_count() == 2, "next frame should be traced after the note-off");
    check(trace(1).sampleLow == 882 && trace(1).count == 0,
          "released gate is removed at the next frame tick");
}

void testRejectedNotesAndTraceOwnership() {
    check(uzx_init(48000, 0) == 1, "engine should initialize for ownership test");
    check(uzx_note_on(41, 0, 50.0f, 0.9f) == 1, "bass note accepted");
    check(uzx_note_on(42, 1, 67.0f, 0.8f) == 1, "pluck note accepted");
    check(uzx_note_on(41, 0, 60.0f, 0.5f) == 0, "duplicate active ID rejected");
    check(uzx_note_on(0, 0, 60.0f, 0.5f) == 0, "reserved zero ID rejected");
    check(uzx_note_on(43, 2, 60.0f, 0.5f) == 0, "unknown instrument rejected");
    check(uzx_dropped_note_count() == 3, "rejected note-ons are counted");
    uzx_render(1);
    const auto& snapshot = trace(0);
    check(snapshot.count == 2, "trace contains both active notes");
    check(std::find(snapshot.channels.begin(), snapshot.channels.end(), 41u) != snapshot.channels.end(),
          "bass retains channel ownership");
    check(std::find(snapshot.channels.begin(), snapshot.channels.end(), 42u) != snapshot.channels.end(),
          "pluck receives an available channel");
    check(snapshot.noise == 42 && snapshot.envelope == 42,
          "pluck transient owns both noise and envelope generators");
}

}  // namespace

int main() {
    testBassAtRate(44100, 882);
    testBassAtRate(48000, 960);
    testPluckTraceAndReset();
    testNoteOffAndShortGate();
    testRejectedNotesAndTraceOwnership();
    std::cout << "WebEngineTest: all checks passed\n";
    return EXIT_SUCCESS;
}
