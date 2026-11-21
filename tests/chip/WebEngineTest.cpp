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

/** The page's pluck preset: a noise transient, then a tone that decays to silence. */
MoTool::uZX::NoteSound pluck() {
    MoTool::uZX::NoteSound sound;
    sound.noise = 1.0f;
    sound.noiseHold = 0.04f;
    sound.noisePeriod = 5.0f;
    sound.decay = 0.14f;
    sound.sustain = 0.0f;
    return sound;
}

/** Renders whole frames at 48 kHz and returns the trace of the last one. */
const MoTool::uZX::TraceSnapshot& renderFrames(std::uint32_t frames) {
    uzx_trace_clear();
    for (std::uint32_t i = 0; i < frames; ++i) {
        uzx_trace_clear();
        renderAndInspect(960);
    }
    return trace(0);
}

bool near(float a, float b) {
    return std::abs(a - b) < 1.0e-4f;
}

void testPluckTraceAndReset() {
    check(uzx_init(48000, 0) == 1, "engine should initialize for pluck test");
    *uzx_note_sound() = pluck();
    check(uzx_note_on(21, 1, 60.0f, 0.8f) == 1, "pluck note should be accepted");
    const auto stats = renderAndInspect(2048);
    checkStereoAudio(stats);
    check(uzx_trace_count() == 3, "pluck render should produce three frames");
    const auto& attack = trace(0);
    check(attack.sampleLow == 0 && attack.count == 1, "pluck attack starts at sample zero");
    check(attack.noise == 21 && near(attack.voices[0].level, 0.8f), "pluck attack owns the noise at full level");
    const auto& decayed = trace(2);
    check(decayed.sampleLow == 1920 && decayed.noise == 0 && decayed.voices[0].level < attack.voices[0].level,
          "pluck noise transient ends while the decaying tone remains");
    check(renderFrames(5).count == 0, "a note that decays to silence ends by itself");

    uzx_reset();
    check(uzx_trace_count() == 0, "reset clears trace history");
    check(uzx_dropped_note_count() == 0, "reset clears rejected-note count");
    check(uzx_note_on(21, 0, 45.0f, 0.8f) == 1, "reset clears the old gate ID for reuse");
    uzx_reset();
    uzx_render(1);
    check(uzx_trace_count() == 1 && trace(0).sampleLow == 0 && trace(0).count == 0,
          "reset removes sounding notes and restarts the trace clock");
}

void testSoundIsUsedOnce() {
    check(uzx_init(48000, 0) == 1, "engine should initialize");
    uzx_note_sound()->noise = 1.0f;
    check(uzx_note_on(1, 0, 60.0f, 0.8f) == 1, "noisy note accepted");
    check(uzx_note_on(2, 0, 64.0f, 0.8f) == 1, "following note accepted");
    uzx_render(1);
    check(trace(0).noise == 1, "the first note sounds as the record said, the next as a plain tone");

    uzx_note_sound()->decay = std::nanf("");
    check(uzx_note_on(3, 0, 60.0f, 0.8f) == 0, "a sound that is not finite is rejected");
}

void testLevelEnvelope() {
    check(uzx_init(48000, 0) == 1, "engine should initialize");
    auto& sound = *uzx_note_sound();
    sound.attack = 0.04f;   // 2 frames
    sound.decay = 0.04f;    // 2 frames
    sound.sustain = 0.5f;
    sound.release = 0.08f;  // 4 frames
    check(uzx_note_on(1, 0, 60.0f, 1.0f) == 1, "note accepted");
    constexpr float held[]{0.0f, 0.5f, 1.0f, 0.75f, 0.5f, 0.5f};
    for (const auto level : held)
        check(near(renderFrames(1).voices[0].level, level), "attack, decay and sustain shape the level");
    uzx_note_off(1);
    constexpr float released[]{0.5f, 0.375f, 0.25f, 0.125f};
    for (const auto level : released)
        check(near(renderFrames(1).voices[0].level, level), "the release falls from the level at the gate's end");
    check(renderFrames(1).count == 0, "the note ends when its release does");
}

void testPitchShaping() {
    check(uzx_init(48000, 0) == 1, "engine should initialize");
    auto& sound = *uzx_note_sound();
    sound.ornamentLength = 3.0f;
    sound.ornament = {0.0f, 3.0f, 7.0f};
    sound.ornamentStep = 0.04f;  // 2 frames
    check(uzx_note_on(1, 0, 60.0f, 1.0f) == 1, "ornament note accepted");
    constexpr float ornament[]{60.0f, 60.0f, 63.0f, 63.0f, 67.0f, 67.0f, 60.0f};
    for (const auto pitch : ornament)
        check(near(renderFrames(1).voices[0].pitch, pitch), "the ornament loops its offsets, each for its step");

    uzx_reset();
    sound.pitchPeak = 12.0f;
    sound.pitchDecay = 0.08f;  // 4 frames
    check(uzx_note_on(1, 0, 60.0f, 1.0f) == 1, "pitch envelope note accepted");
    constexpr float falling[]{72.0f, 69.0f, 66.0f, 63.0f, 60.0f};
    for (const auto pitch : falling)
        check(near(renderFrames(1).voices[0].pitch, pitch), "the pitch falls from the peak to the note");

    uzx_reset();
    sound.vibratoRate = 12.5f;  // a quarter turn per frame
    sound.vibratoDepth = 0.5f;
    sound.vibratoDelay = 0.04f;
    check(uzx_note_on(1, 0, 60.0f, 1.0f) == 1, "vibrato note accepted");
    constexpr float vibrato[]{60.0f, 60.0f, 60.0f, 60.5f, 60.0f, 59.5f};
    for (const auto pitch : vibrato)
        check(near(renderFrames(1).voices[0].pitch, pitch), "the vibrato starts after its delay");
}

void testEnvelopeDriverTakesNoChannel() {
    check(uzx_init(48000, 0) == 1, "engine should initialize");
    auto& sound = *uzx_note_sound();
    sound.tone = 0.0f;
    sound.drivesEnvelope = 1.0f;
    sound.restartEnvelope = 1.0f;
    check(uzx_note_on(1, 0, 36.0f, 1.0f) == 1, "envelope driver accepted");
    sound.envelope = 1.0f;
    sound.pan = 0.5f;
    check(uzx_note_on(2, 0, 43.0f, 1.0f) == 1, "tone through the envelope accepted");
    const auto first = renderFrames(1);
    check(first.envelope == 1 && first.retrigger == 1, "the driver owns the envelope and restarts it once");
    check(first.channels[0] == 0 && first.channels[1] == 2 && first.channels[2] == 0,
          "only the tone takes a channel, the one its pan asks for");
    check(renderFrames(1).retrigger == 0, "the envelope restarts only as the note starts");
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
    *uzx_note_sound() = pluck();
    check(uzx_note_on(42, 1, 67.0f, 0.8f) == 1, "pluck note accepted");
    check(uzx_note_on(41, 0, 60.0f, 0.5f) == 0, "duplicate active ID rejected");
    check(uzx_note_on(0, 0, 60.0f, 0.5f) == 0, "reserved zero ID rejected");
    check(uzx_note_on(43, 0, 128.0f, 0.5f) == 0, "pitch above the MIDI range rejected");
    check(uzx_dropped_note_count() == 3, "rejected note-ons are counted");
    uzx_render(1);
    const auto& snapshot = trace(0);
    check(snapshot.count == 2, "trace contains both active notes");
    check(std::find(snapshot.channels.begin(), snapshot.channels.end(), 41u) != snapshot.channels.end(),
          "bass retains channel ownership");
    check(std::find(snapshot.channels.begin(), snapshot.channels.end(), 42u) != snapshot.channels.end(),
          "pluck receives an available channel");
    check(snapshot.noise == 42 && snapshot.voices[1].instrument == 1,
          "pluck transient owns the noise generator and keeps its instrument tag");
}

}  // namespace

int main() {
    testBassAtRate(44100, 882);
    testBassAtRate(48000, 960);
    testPluckTraceAndReset();
    testSoundIsUsedOnce();
    testLevelEnvelope();
    testPitchShaping();
    testEnvelopeDriverTakesNoChannel();
    testNoteOffAndShortGate();
    testRejectedNotesAndTraceOwnership();
    std::cout << "WebEngineTest: all checks passed\n";
    return EXIT_SUCCESS;
}
