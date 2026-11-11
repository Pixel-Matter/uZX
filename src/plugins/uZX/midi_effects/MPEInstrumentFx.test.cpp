#include "MPEInstrumentFx.h"

#include <JuceHeader.h>

#include <utility>
#include <vector>

namespace MoTool::uZX {

namespace {

struct SpanFx;

/** A voice that records the sample range of every sub-block it renders and counts its note changes. */
struct SpanVoice : MPEEffectVoice<SpanVoice, SpanFx>
{
    using MPEEffectVoice::MPEEffectVoice;

    void noteStarted() {}
    void noteStopped(bool) { clearCurrentNote(); }
    void notePressureChanged() { ++changes; }
    void notePitchbendChanged() { ++changes; }
    void noteTimbreChanged() { ++changes; }
    void setCurrentPlayRate(double) {}
    void renderNextBlock(MidiBufferContext& c);

    int changes = 0;
};

struct SpanFx : MPEInstrumentFx<SpanVoice, SpanFx>
{
    SpanFx() : MPEInstrumentFx(*this) {}

    bool isPlaying() { return voices.getVoice(0)->isActive(); }
    int changesOf(int voice) { return voices.getVoice(voice)->changes; }

    std::vector<std::pair<int, int>> spans;  // start, length
};

void SpanVoice::renderNextBlock(MidiBufferContext& c) {
    midiFx.spans.emplace_back(c.start, c.length);
}

constexpr double sampleRate = 48000.0;

MidiMessage at(MidiMessage m, int sample) {
    return m.withTimeStamp(sample / sampleRate);
}

void process(SpanFx& fx, int start, int length, std::vector<MidiMessage> events) {
    te::MidiMessageArray buffer;
    for (auto& m : events)
        buffer.addMidiMessage(m, {});
    MidiBufferContext c{buffer, start, length, {}, sampleRate};
    fx(c);
}

}  // namespace

class MPEInstrumentFxTests : public UnitTest
{
public:
    MPEInstrumentFxTests() : UnitTest("MPEInstrumentFx", "MoTool") {}

    void runTest() override {
        using Spans = std::vector<std::pair<int, int>>;

        // Each case holds a note started in an earlier block, so its voice renders the whole range
        auto spansOf = [](int start, int length, std::vector<MidiMessage> events) {
            SpanFx fx;
            process(fx, 0, 1, {at(MidiMessage::noteOn(1, 60, (uint8) 100), 0)});
            fx.spans.clear();
            process(fx, start, length, std::move(events));
            return fx.spans;
        };

        beginTest("A range without events renders as one sub-block");
        {
            expect(spansOf(0, 64, {}) == Spans{{0, 64}});
            expect(spansOf(64, 64, {}) == Spans{{64, 64}});
        }

        beginTest("An event inside a range that starts past zero splits it there");
        {
            const auto bend = at(MidiMessage::pitchWheel(1, 9000), 96);
            expect(spansOf(64, 64, {bend}) == Spans{{64, 32}, {96, 32}});
        }

        beginTest("Events at the start do not split the range, events from its end are left out");
        {
            const auto bend = at(MidiMessage::pitchWheel(1, 9000), 64);
            expect(spansOf(64, 64, {bend}) == Spans{{64, 64}});

            SpanFx fx;
            process(fx, 0, 1, {at(MidiMessage::noteOn(1, 60, (uint8) 100), 0)});
            process(fx, 64, 64, {at(MidiMessage::noteOff(1, 60), 128)});
            expect(fx.isPlaying(), "a note off at the end belongs to the next range");

            process(fx, 64, 64, {at(MidiMessage::noteOff(1, 60), 127)});
            expect(!fx.isPlaying(), "a note off on the last sample is in the range");
        }

        beginTest("Pitchbend, pressure and timbre reach only the voice playing the note");
        {
            SpanFx fx;
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
