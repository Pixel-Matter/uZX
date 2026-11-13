#include "MPEInstrumentFx.h"

#include <JuceHeader.h>

#include <utility>
#include <vector>

namespace MoTool::uZX {

namespace {

struct FrameFx;

/** A voice that sounds at its note's pitch, at a level that changes every frame, so every frame it renders
    sends a message at the frame's time. It also counts its note changes. */
struct FrameVoice : MPEEffectVoice<FrameVoice, FrameFx>
{
    using MPEEffectVoice::MPEEffectVoice;

    void noteStarted() {}
    void noteStopped(bool) { clearCurrentNote(); }
    void notePressureChanged() { ++changes; }
    void notePitchbendChanged() { ++changes; }
    void noteTimbreChanged() { ++changes; }
    void setCurrentPlayRate(double) {}

    void renderFrame(NoteFrame& frame) {
        level = level > 0.5f ? 0.25f : 0.75f;
        frame.add({.channel = currentlyPlayingNote.midiChannel,
                   .key = currentlyPlayingNote.initialNote,
                   .pitch = currentlyPlayingNote.initialNote + currentlyPlayingNote.totalPitchbendInSemitones,
                   .level = level,
                   .attack = false});
    }

    int changes = 0;
    float level = 0.25f;
};

struct FrameFx : MPEInstrumentFx<FrameVoice, FrameFx>
{
    FrameFx() : MPEInstrumentFx(*this) {}

    bool isPlaying() { return voices.getVoice(0)->isActive(); }
    int changesOf(int voice) { return voices.getVoice(voice)->changes; }
};

constexpr double sampleRate = 16.0 * noteFrameRate;  // a frame is 16 samples

MidiMessage at(MidiMessage m, int sample) {
    return m.withTimeStamp(sample / sampleRate);
}

/** Processes the range and returns the samples its output messages are at: all of them, or the pitch wheels. */
std::vector<int> process(FrameFx& fx, int start, int length, std::vector<MidiMessage> events, bool pitchWheels = false) {
    te::MidiMessageArray buffer;
    for (auto& m : events)
        buffer.addMidiMessage(m, {});
    MidiBufferContext c{buffer, start, length, {}, sampleRate};
    fx(c);

    std::vector<int> samples;
    for (auto& m : buffer) {
        const int sample = roundToInt(m.getTimeStamp() * sampleRate);
        if ((! pitchWheels || m.isPitchWheel()) && (samples.empty() || samples.back() != sample))
            samples.push_back(sample);
    }
    return samples;
}

}  // namespace

class MPEInstrumentFxTests : public UnitTest
{
public:
    MPEInstrumentFxTests() : UnitTest("MPEInstrumentFx", "MoTool") {}

    void runTest() override {
        using Samples = std::vector<int>;
        const auto noteOn = at(MidiMessage::noteOn(1, 60, (uint8) 100), 0);

        // Each case holds a note started in an earlier block, so its voice renders every frame of the range
        auto framesOf = [&](int start, int length) {
            FrameFx fx;
            process(fx, 0, 1, {noteOn});
            return process(fx, start, length, {});
        };
        auto bendsOf = [&](int start, int length, int bendSample) {
            FrameFx fx;
            process(fx, 0, 1, {noteOn});
            return process(fx, start, length, {at(MidiMessage::pitchWheel(1, 9000), bendSample)}, true);
        };

        beginTest("A range renders the frames that start in it");
        {
            expect(framesOf(0, 64) == Samples{0, 16, 32, 48});
            expect(framesOf(64, 64) == Samples{64, 80, 96, 112});
            expect(framesOf(60, 8) == Samples{64});
            expect(framesOf(65, 15) == Samples{});
        }

        beginTest("An event is heard from the first frame at or after it");
        {
            expect(bendsOf(64, 64, 96) == Samples{96});
            expect(bendsOf(64, 64, 97) == Samples{112});
            expect(bendsOf(64, 64, 64) == Samples{64}, "an event at the start of the range");
        }

        beginTest("Events from the end of the range are left out");
        {
            FrameFx fx;
            process(fx, 0, 1, {noteOn});
            process(fx, 64, 64, {at(MidiMessage::noteOff(1, 60), 128)});
            expect(fx.isPlaying(), "a note off at the end belongs to the next range");

            process(fx, 64, 64, {at(MidiMessage::noteOff(1, 60), 127)});
            expect(!fx.isPlaying(), "a note off on the last sample is in the range");
        }

        beginTest("Pitchbend, pressure and timbre reach only the voice playing the note");
        {
            FrameFx fx;
            process(fx,
                    0,
                    64,
                    {at(MidiMessage::noteOn(1, 60, (uint8) 100), 0),
                     at(MidiMessage::pitchWheel(1, 9000), 1),
                     at(MidiMessage::channelPressureChange(1, 90), 2),
                     at(MidiMessage::controllerEvent(1, 74, 90), 3)});
            expectEquals(fx.changesOf(0), 3);
            expectEquals(fx.changesOf(1), 0, "an idle voice is not told");
            expectEquals(fx.changesOf(2), 0, "an idle voice is not told");
        }
    }
};

static MPEInstrumentFxTests mpeInstrumentFxTests;

}  // namespace MoTool::uZX
