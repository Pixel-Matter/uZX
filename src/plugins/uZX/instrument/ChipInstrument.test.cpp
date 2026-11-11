#include "ChipInstrument.h"

#include <JuceHeader.h>

#include <utility>
#include <vector>

namespace MoTool::Tests {

using namespace MoTool::uZX;

//==============================================================================
/** The MIDI the chip instrument sends for the notes it is played: a note on at the level its envelope
    starts at, the level after that as polyphonic aftertouch, the pitch as the pitch wheel, one message
    a 50 Hz frame at most. */
class ChipInstrumentFxTests : public juce::UnitTest
{
public:
    ChipInstrumentFxTests() : UnitTest("ChipInstrumentFx", "MoTool") {}

    void runTest() override {
        const auto on = [](int key, int channel = 1) { return MidiMessage::noteOn(channel, key, (uint8) 100); };
        const auto off = [](int key, int channel = 1) { return MidiMessage::noteOff(channel, key); };

        beginTest("A note without envelopes is a pitch wheel and a note on, then a note off");
        {
            Rig rig;
            rig.play(2, {{0, on(60)}});
            expectEquals(rig.take(), String("0 Pitch wheel 8192 Channel 1; 0 Note on C3 Velocity 100 Channel 1"));
            rig.play(2, {{100, off(60)}});
            expectEquals(rig.take(), String("2880 Note off C3 Velocity 0 Channel 1"));
        }

        beginTest("The amp envelope goes out as aftertouch, a message a frame while the level changes");
        {
            Rig rig;
            rig.fx.oscParams.ampAttack.setStoredValue(0.04f);
            rig.fx.oscParams.ampDecay.setStoredValue(0.04f);
            rig.fx.oscParams.ampSustain.setStoredValue(50.0f);
            rig.fx.oscParams.ampRelease.setStoredValue(0.04f);
            rig.play(6, {{0, on(60)}});
            expectEquals(rig.take(), String("0 Pitch wheel 8192 Channel 1; 0 Note on C3 Velocity 50 Channel 1; "
                                            "960 Aftertouch C3: 100 Channel 1; 1920 Aftertouch C3: 50 Channel 1"));
            rig.play(4, {{0, off(60)}});
            expectEquals(rig.take(), String("5760 Note off C3 Velocity 0 Channel 1"), "one note off, in a block of four frames");
        }

        beginTest("The pitch envelope goes out as the pitch wheel");
        {
            Rig rig;
            rig.fx.oscParams.pitchDecay.setStoredValue(0.04f);
            rig.fx.oscParams.pitchDepth.setStoredValue(1.0f);
            rig.play(4, {{0, on(60)}});
            expectEquals(rig.take(), String("0 Pitch wheel 10240 Channel 1; 0 Note on C3 Velocity 100 Channel 1; "
                                            "960 Pitch wheel 8192 Channel 1"));
        }

        beginTest("A note that starts between frames goes out at the next frame");
        {
            Rig rig;
            rig.play(2, {{500, on(60)}});
            expectEquals(rig.take(), String("960 Pitch wheel 8192 Channel 1; 960 Note on C3 Velocity 100 Channel 1"));
        }

        beginTest("Notes of a chord on one channel each send their own messages");
        {
            Rig rig;
            rig.play(1, {{0, on(60)}, {0, on(64)}});
            expectEquals(rig.take(), String("0 Pitch wheel 8192 Channel 1; 0 Note on C3 Velocity 100 Channel 1; "
                                            "0 Pitch wheel 8192 Channel 1; 0 Note on E3 Velocity 100 Channel 1"));
            rig.play(1, {{0, off(60)}});
            expectEquals(rig.take(), String("960 Note off C3 Velocity 0 Channel 1"));
        }

        beginTest("A fourth note takes a voice from a held one");
        {
            Rig rig;
            rig.play(1, {{0, on(60)}, {0, on(64)}, {0, on(67)}});
            rig.take();
            rig.play(1, {{0, on(72)}});
            expectEquals(rig.take(), String("960 Note off E3 Velocity 0 Channel 1; "
                                            "960 Pitch wheel 8192 Channel 1; 960 Note on C4 Velocity 100 Channel 1"),
                         "the note that lost its voice gets a note off");
        }

        beginTest("A key played again while its note is released starts the note again");
        {
            Rig rig;
            rig.fx.oscParams.ampRelease.setStoredValue(0.1f);
            rig.play(1, {{0, on(60)}});
            rig.play(1, {{0, off(60)}});
            rig.take();
            rig.play(1, {{0, on(60)}});
            expectEquals(rig.take(), String("1920 Note off C3 Velocity 0 Channel 1; "
                                            "1920 Pitch wheel 8192 Channel 1; 1920 Note on C3 Velocity 100 Channel 1"),
                         "a note off between the two note ons");
        }

        beginTest("All notes off ends the held notes at the next frame");
        {
            Rig rig;
            rig.fx.oscParams.ampRelease.setStoredValue(0.1f);
            rig.play(1, {{0, on(60)}, {0, on(64)}});
            rig.take();
            rig.play(1, {}, true);
            expectEquals(rig.take(), String("960 Note off C3 Velocity 0 Channel 1; 960 Note off E3 Velocity 0 Channel 1"));
        }
    }

private:
    static constexpr double sampleRate = 48000.0;
    static constexpr int frame = 960;  // samples in a 50 Hz frame

    struct Rig {
        juce::ValueTree state {"instrument"};
        ChipInstrumentFx fx {state};
        int position = 0;
        juce::StringArray sent;

        /** Plays the next `frames` frames with the events at their sample offsets, and collects what goes out. */
        void play(int frames, std::vector<std::pair<int, MidiMessage>> events = {}, bool allNotesOff = false) {
            te::MidiMessageArray buffer;
            buffer.isAllNotesOff = allNotesOff;
            for (auto& [sample, m] : events)
                buffer.addMidiMessage(m.withTimeStamp(sample / sampleRate), {});
            MidiBufferContext c {buffer, 0, frames * frame, te::TimePosition::fromSeconds(position / sampleRate), sampleRate};
            fx(c);
            for (auto& m : buffer)
                sent.add(String(position + roundToInt(m.getTimeStamp() * sampleRate)) + " " + m.getDescription());
            position += frames * frame;
        }

        /** What went out since the last call, as "sample message; ...". */
        String take() {
            const auto all = sent.joinIntoString("; ");
            sent.clear();
            return all;
        }
    };
};

static ChipInstrumentFxTests chipInstrumentFxTests;

}  // namespace MoTool::Tests
