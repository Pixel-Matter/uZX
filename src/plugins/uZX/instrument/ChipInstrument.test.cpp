#include "ChipInstrument.h"

#include <JuceHeader.h>

#include <utility>
#include <vector>

namespace MoTool::Tests {

using namespace MoTool::uZX;

//==============================================================================
/** The MIDI the chip instrument sends for the notes it is played, each on a channel of its own: a note on
    at the level its envelope starts at, the level after that as polyphonic aftertouch, the pitch as the
    pitch wheel, what changed once a 50 Hz frame. */
class ChipInstrumentFxTests : public juce::UnitTest
{
public:
    ChipInstrumentFxTests() : UnitTest("ChipInstrumentFx", "MoTool") {}

    void runTest() override {
        const auto on = [](int key, int channel = 1) { return MidiMessage::noteOn(channel, key, (uint8) 100); };
        const auto off = [](int key, int channel = 1) { return MidiMessage::noteOff(channel, key); };

        // What a note starts with on its own channel: the channel's controllers reset, then its priority,
        // which is the number of the channel it was played on
        const auto start = [](int sample, int channel, int priority = 1) {
            const auto at = String(sample) + " Controller ";
            const auto on = " Channel " + String(channel) + "; ";
            return at + "All Controllers Off: 0" + on + at + "86: " + String(priority) + on;
        };

        beginTest("A note starts on a channel of its own with a reset, its priority, the pitch wheel and a note on");
        {
            Rig rig;
            rig.play(2, {{0, on(60)}});
            expectEquals(rig.take(), start(0, 2) + "0 Pitch wheel 8192 Channel 2; 0 Note on C3 Velocity 100 Channel 2");
            rig.play(2, {{100, off(60)}});
            expectEquals(rig.take(), String("2880 Note off C3 Velocity 0 Channel 2"));
        }

        beginTest("The amp envelope goes out as aftertouch, a message a frame while the level changes");
        {
            Rig rig;
            rig.fx.oscParams.ampAttack.setStoredValue(0.04f);
            rig.fx.oscParams.ampDecay.setStoredValue(0.04f);
            rig.fx.oscParams.ampSustain.setStoredValue(50.0f);
            rig.fx.oscParams.ampRelease.setStoredValue(0.04f);
            rig.play(6, {{0, on(60)}});
            expectEquals(rig.take(), start(0, 2) + "0 Pitch wheel 8192 Channel 2; 0 Note on C3 Velocity 50 Channel 2; "
                                                   "960 Aftertouch C3: 100 Channel 2; 1920 Aftertouch C3: 50 Channel 2");
            rig.play(4, {{0, off(60)}});
            expectEquals(rig.take(), String("5760 Note off C3 Velocity 0 Channel 2"), "one note off, in a block of four frames");
        }

        beginTest("The pitch envelope goes out as the pitch wheel");
        {
            Rig rig;
            rig.fx.oscParams.pitchDecay.setStoredValue(0.04f);
            rig.fx.oscParams.pitchDepth.setStoredValue(1.0f);
            rig.play(4, {{0, on(60)}});
            expectEquals(rig.take(), start(0, 2) + "0 Pitch wheel 10240 Channel 2; 0 Note on C3 Velocity 100 Channel 2; "
                                                   "960 Pitch wheel 8192 Channel 2");
        }

        beginTest("A note that starts between frames goes out at the next frame");
        {
            Rig rig;
            rig.play(2, {{500, on(60)}});
            expectEquals(rig.take(), start(960, 2) + "960 Pitch wheel 8192 Channel 2; 960 Note on C3 Velocity 100 Channel 2");
        }

        beginTest("The notes of a chord on one channel go out on a channel each, with a pitch wheel each");
        {
            Rig rig;
            rig.fx.oscParams.pitchDecay.setStoredValue(0.04f);
            rig.fx.oscParams.pitchDepth.setStoredValue(1.0f);
            rig.play(1, {{0, on(60)}});
            rig.take();
            rig.play(1, {{0, on(64)}});
            expectEquals(rig.take(), String("960 Pitch wheel 8192 Channel 2; ") + start(960, 3)
                                         + "960 Pitch wheel 10240 Channel 3; 960 Note on E3 Velocity 100 Channel 3",
                         "the first note's pitch settles while the second one's starts");
            rig.play(1, {{0, off(60)}});
            expectEquals(rig.take(), String("1920 Note off C3 Velocity 0 Channel 2; 1920 Pitch wheel 8192 Channel 3"));
        }

        beginTest("A note's priority is the channel it is played on, and that channel's controllers go with the note");
        {
            Rig rig;
            rig.play(1, {{0, MidiMessage::controllerEvent(5, 81, 127)}, {0, MidiMessage::controllerEvent(5, 10, 0)},
                         {0, on(60, 5)}, {0, on(64, 1)}});
            expectEquals(rig.take(),
                         String("0 Controller All Controllers Off: 0 Channel 2; "
                                "0 Controller General Purpose Button 2 (on/off): 127 Channel 2; "
                                "0 Controller 86: 5 Channel 2; 0 Controller Pan position (coarse): 0 Channel 2; "
                                "0 Pitch wheel 8192 Channel 2; 0 Note on C3 Velocity 100 Channel 2; ")
                             + start(0, 3, 1) + "0 Pitch wheel 8192 Channel 3; 0 Note on E3 Velocity 100 Channel 3",
                         "the note of channel 5 has the noise on and a pan; the note of channel 1 has neither");
            rig.play(1, {{0, MidiMessage::controllerEvent(5, 80, 0)}, {0, MidiMessage::controllerEvent(5, 83, 127)}});
            expectEquals(rig.take(),
                         String("960 Controller General Purpose Button 1 (on/off): 0 Channel 2; "
                                "960 Controller General Purpose Button 4 (on/off): 127 Channel 2; "
                                "960 Controller General Purpose Button 4 (on/off): 0 Channel 2"),
                         "the tone off and an envelope restart reach the note that sounds, and only it");
        }

        beginTest("A fourth note takes a voice from a held one");
        {
            Rig rig;
            rig.play(1, {{0, on(60)}, {0, on(64)}, {0, on(67)}});
            rig.take();
            rig.play(1, {{0, on(72)}});
            expectEquals(rig.take(), String("960 Note off E3 Velocity 0 Channel 3; ") + start(960, 5)
                                         + "960 Pitch wheel 8192 Channel 5; 960 Note on C4 Velocity 100 Channel 5",
                         "the note that lost its voice gets a note off, and the new one the channel free the longest");
        }

        beginTest("A key played again while its note is released starts the note again");
        {
            Rig rig;
            rig.fx.oscParams.ampRelease.setStoredValue(0.1f);
            rig.play(1, {{0, on(60)}});
            rig.play(1, {{0, off(60)}});
            rig.take();
            rig.play(1, {{0, on(60)}});
            expectEquals(rig.take(), String("1920 Note off C3 Velocity 0 Channel 2; ") + start(1920, 3)
                                         + "1920 Pitch wheel 8192 Channel 3; 1920 Note on C3 Velocity 100 Channel 3",
                         "a note off between the two note ons");
        }

        beginTest("All notes off ends the held notes at the next frame");
        {
            Rig rig;
            rig.fx.oscParams.ampRelease.setStoredValue(0.1f);
            rig.play(1, {{0, on(60)}, {0, on(64)}});
            rig.take();
            rig.play(1, {}, true);
            expectEquals(rig.take(), String("960 Note off C3 Velocity 0 Channel 2; 960 Note off E3 Velocity 0 Channel 3"));
            expect(rig.allNotesOffSent, "what comes after the instrument hears the all-notes-off too");
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
        bool allNotesOffSent = false;  // by the last block played

        /** Plays the next `frames` frames with the events at their sample offsets, and collects what goes out. */
        void play(int frames, std::vector<std::pair<int, MidiMessage>> events = {}, bool allNotesOff = false) {
            te::MidiMessageArray buffer;
            buffer.isAllNotesOff = allNotesOff;
            for (auto& [sample, m] : events)
                buffer.addMidiMessage(m.withTimeStamp(sample / sampleRate), {});
            MidiBufferContext c {buffer, 0, frames * frame, te::TimePosition::fromSeconds(position / sampleRate), sampleRate};
            fx(c);
            allNotesOffSent = buffer.isAllNotesOff;
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
