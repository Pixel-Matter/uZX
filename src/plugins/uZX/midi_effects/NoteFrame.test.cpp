#include "NoteFrame.h"

#include <JuceHeader.h>

namespace MoTool::Tests {

using namespace MoTool::uZX;
using namespace juce;

//==============================================================================
/** The chip-note stream read back into frames, and frames that make the trip through MIDI. */
class NoteDecoderTests : public UnitTest
{
public:
    NoteDecoderTests() : UnitTest("NoteDecoder", "MoTool") {}

    void runTest() override {
        beginTest("A note sounds from its note on to its note off, and starts once");
        {
            NoteDecoder decoder;
            expect(decoder.decode(MidiMessage::noteOn(3, 60, (uint8) 127)));
            auto frame = decoder.takeFrame();
            expectEquals(describe(frame), String("3:60 60.00 1.0 attack"));
            expectEquals(describe(decoder.takeFrame()), String("3:60 60.00 1.0"), "held, not started again");

            decoder.decode(MidiMessage::noteOff(3, 60));
            expect(decoder.takeFrame().isEmpty());
        }

        beginTest("Aftertouch is a note's level, the pitch wheel bends its channel");
        {
            NoteDecoder decoder;
            decoder.decode(MidiMessage::noteOn(1, 60, (uint8) 127));
            decoder.decode(MidiMessage::noteOn(2, 64, (uint8) 127));
            decoder.takeFrame();
            decoder.decode(MidiMessage::aftertouchChange(1, 60, 0));
            decoder.decode(MidiMessage::pitchWheel(2, 16383));
            expectEquals(describe(decoder.takeFrame()), String("1:60 60.00 0.0; 2:64 66.00 1.0"));
        }

        beginTest("A key played again while it sounds starts again");
        {
            NoteDecoder decoder;
            decoder.decode(MidiMessage::noteOn(1, 60, (uint8) 127));
            decoder.takeFrame();
            decoder.decode(MidiMessage::noteOn(1, 60, (uint8) 127));
            expectEquals(describe(decoder.takeFrame()), String("1:60 60.00 1.0 attack"));
        }

        beginTest("Timbre controllers set the timbre of the notes on their channel, sounding or yet to come");
        {
            NoteDecoder decoder;
            expect(decoder.decode(MidiMessage::controllerEvent(1, 80, 0)));
            decoder.decode(MidiMessage::noteOn(1, 60, (uint8) 127));
            decoder.decode(MidiMessage::noteOn(2, 60, (uint8) 127));
            decoder.decode(MidiMessage::controllerEvent(1, 81, 127));
            decoder.decode(MidiMessage::controllerEvent(1, 82, 1));
            decoder.decode(MidiMessage::controllerEvent(1, 70, 12));
            decoder.decode(MidiMessage::controllerEvent(1, 2, 5));
            decoder.decode(MidiMessage::controllerEvent(1, 85, 127));

            const auto frame = decoder.takeFrame();
            const auto& first = frame.begin()[0].timbre;
            expect(!first.tone && first.noise && first.envelope && first.drivesEnvelope);
            expectEquals(first.envelopeShape, 12);
            expectEquals(first.noisePeriod, 5);
            const auto& second = frame.begin()[1].timbre;
            expect(second.tone && !second.noise && !second.envelope && !second.drivesEnvelope,
                   "another channel keeps the default timbre");

            expect(!decoder.decode(MidiMessage::controllerEvent(1, 64, 127)),
                   "the sustain pedal is no part of the stream");
        }

        beginTest("A voice's priority is its channel's number until controller 86 sets one");
        {
            NoteDecoder decoder;
            decoder.decode(MidiMessage::noteOn(3, 60, (uint8) 127));
            decoder.decode(MidiMessage::noteOn(9, 60, (uint8) 127));
            expectEquals(decoder.takeFrame().begin()[0].priority, 3);
            expectEquals(decoder.takeFrame().begin()[1].priority, 9);

            decoder.decode(MidiMessage::controllerEvent(3, 86, 100));
            expectEquals(decoder.takeFrame().begin()[0].priority, 100, "of a sounding voice too");
            decoder.decode(MidiMessage::controllerEvent(3, 86, 0));
            expectEquals(decoder.takeFrame().begin()[0].priority, 0);
        }

        beginTest("A voice asks for no place until controller 10 pans its channel");
        {
            NoteDecoder decoder;
            decoder.decode(MidiMessage::noteOn(3, 60, (uint8) 127));
            expect(!decoder.takeFrame().begin()->pan.has_value());
            decoder.decode(MidiMessage::controllerEvent(3, 10, 127));
            expectEquals(decoder.takeFrame().begin()->pan.value_or(-1.0f), 1.0f);
            decoder.decode(MidiMessage::controllerEvent(3, 10, 0));
            expectEquals(decoder.takeFrame().begin()->pan.value_or(-1.0f), 0.0f);
        }

        beginTest("The envelope restarts on its controller, not on a note or another shape");
        {
            NoteDecoder decoder;
            const auto restarts = [&] {
                String text;
                for (const auto& v : decoder.takeFrame())
                    text << (v.restartEnvelope ? "R" : "-");
                return text;
            };
            decoder.decode(MidiMessage::noteOn(4, 36, (uint8) 100));
            expectEquals(restarts(), String("-"), "a note starting lets the envelope run");
            decoder.decode(MidiMessage::controllerEvent(4, 83, 127));
            expectEquals(restarts(), String("R"), "inside a lasting note");
            expectEquals(restarts(), String("-"), "once");
            decoder.decode(MidiMessage::controllerEvent(4, 83, 0));
            expectEquals(restarts(), String("-"), "the release does nothing");
            decoder.decode(MidiMessage::controllerEvent(4, 70, 10));
            expectEquals(restarts(), String("-"), "another shape is a value; the chip restarts when it gets one");

            decoder.decode(MidiMessage::noteOff(4, 36));
            decoder.decode(MidiMessage::controllerEvent(4, 83, 1));
            expectEquals(restarts(), String(), "no voice on the channel yet");
            decoder.decode(MidiMessage::noteOn(4, 38, (uint8) 100));
            decoder.decode(MidiMessage::noteOn(5, 38, (uint8) 100));
            expectEquals(restarts(), String("R-"), "it waits for a voice of its channel");
        }

        beginTest("An all-notes-off controller ends its channel's notes; a reset ends all and forgets the controllers");
        {
            NoteDecoder decoder;
            decoder.decode(MidiMessage::controllerEvent(2, 80, 0));
            decoder.decode(MidiMessage::pitchWheel(2, 0));
            decoder.decode(MidiMessage::noteOn(1, 60, (uint8) 127));
            decoder.decode(MidiMessage::noteOn(2, 64, (uint8) 127));
            decoder.decode(MidiMessage::allNotesOff(1));
            expectEquals(describe(decoder.takeFrame()), String("2:64 62.00 1.0 attack"));

            decoder.reset();
            expect(decoder.takeFrame().isEmpty());
            decoder.decode(MidiMessage::noteOn(2, 64, (uint8) 127));
            const auto frame = decoder.takeFrame();
            expectEquals(describe(frame), String("2:64 64.00 1.0 attack"), "the wheel is back in the middle");
            expect(frame.begin()->timbre.tone, "and the tone back on");
        }

        beginTest("A reset of all controllers returns a channel's timbre, pan, priority and wheel");
        {
            NoteDecoder decoder;
            for (const auto& m : {MidiMessage::controllerEvent(3, 81, 127), MidiMessage::controllerEvent(3, 10, 0),
                                  MidiMessage::controllerEvent(3, 86, 100), MidiMessage::controllerEvent(3, 83, 1),
                                  MidiMessage::pitchWheel(3, 0), MidiMessage::noteOn(3, 60, (uint8) 127),
                                  MidiMessage::allControllersOff(3)})
                decoder.decode(m);
            const auto frame = decoder.takeFrame();
            const auto& voice = *frame.begin();
            expectEquals(describe(frame), String("3:60 60.00 1.0 attack"), "the note sounds on");
            expect(voice.timbre.tone && !voice.timbre.noise && !voice.pan.has_value() && !voice.restartEnvelope);
            expectEquals(voice.priority, 3);
        }

        beginTest("Frames survive the trip through MIDI, each voice on a channel of its own from 2 up");
        {
            NoteEncoder encoder;
            NoteDecoder decoder;
            const auto trip = [&](const NoteFrame& frame) {
                tracktion::MidiMessageArray midi;
                encoder.encode(frame, midi, 0.0);
                for (const auto& m : midi)
                    decoder.decode(m);
                return decoder.takeFrame();
            };

            NoteFrame frame;
            frame.add({.channel = 1, .key = 60, .pitch = 60.5, .level = 1.0f, .attack = true});
            frame.add({.channel = 1, .key = 48, .pitch = 47.0, .level = 0.0f, .attack = true});
            expectEquals(describe(trip(frame)),
                         String("2:60 60.50 1.0 attack; 3:48 47.00 0.0 attack"),
                         "two notes of one channel bend apart; one that starts at level 0 is still a voice");

            NoteFrame next;
            next.add({.channel = 1, .key = 48, .pitch = 48.25, .level = 1.0f});
            next.add({.channel = 1, .key = 60, .pitch = 60.0, .level = 1.0f, .attack = true});
            expectEquals(describe(trip(next)), String("3:48 48.25 1.0; 4:60 60.00 1.0 attack"),
                         "one voice moves on its channel, the other ends and starts again on the next free one");

            expectEquals(describe(trip({})), String());
        }

        beginTest("A voice's timbre, pan, priority and restart make the trip, and change while it sounds");
        {
            NoteEncoder encoder;
            NoteDecoder decoder;
            const auto trip = [&](const NoteFrame& frame) {
                tracktion::MidiMessageArray midi;
                encoder.encode(frame, midi, 0.0);
                for (const auto& m : midi)
                    decoder.decode(m);
                return decoder.takeFrame();
            };
            // "tone noise envelope driver shape noisePeriod priority pan restart" of each voice
            const auto controls = [](const NoteFrame& frame) {
                StringArray voices;
                for (const auto& v : frame)
                    voices.add(String(int(v.timbre.tone)) + String(int(v.timbre.noise)) + String(int(v.timbre.envelope))
                               + String(int(v.timbre.drivesEnvelope)) + " " + String(v.timbre.envelopeShape) + " "
                               + String(v.timbre.noisePeriod) + " " + String(v.priority) + " "
                               + (v.pan ? String(*v.pan, 1) : String("-")) + (v.restartEnvelope ? " R" : ""));
                return voices.joinIntoString("; ");
            };

            NoteVoice plain{.channel = 7, .key = 60, .pitch = 60.0, .level = 1.0f, .attack = true, .priority = 7};
            NoteVoice buzz{.channel = 7,
                           .key = 36,
                           .pitch = 36.0,
                           .level = 1.0f,
                           .attack = true,
                           .restartEnvelope = true,
                           .priority = 100,
                           .pan = 1.0f,
                           .timbre = {.tone = false, .noise = true, .envelope = true, .drivesEnvelope = true,
                                      .envelopeShape = 10, .noisePeriod = 7}};
            NoteFrame frame;
            frame.add(plain);
            frame.add(buzz);
            expectEquals(controls(trip(frame)), String("1000 0 0 7 -; 0111 10 7 100 1.0 R"),
                         "the priority a voice had on the channel it came from goes with it");

            plain.attack = buzz.attack = false;
            buzz.restartEnvelope = false;
            buzz.timbre.noise = false;
            buzz.priority = 3;
            plain.pan = 0.0f;
            NoteFrame next;
            next.add(plain);
            next.add(buzz);
            expectEquals(controls(trip(next)), String("1000 0 0 7 0.0; 0011 10 7 3 1.0"));
        }

        beginTest("A channel used again starts from a reset, not with what the voice before left on it");
        {
            NoteEncoder encoder;
            NoteDecoder decoder;
            const auto trip = [&](std::initializer_list<NoteVoice> voices) {
                NoteFrame frame;
                for (const auto& v : voices)
                    frame.add(v);
                tracktion::MidiMessageArray midi;
                encoder.encode(frame, midi, 0.0);
                for (const auto& m : midi)
                    decoder.decode(m);
                return decoder.takeFrame();
            };
            trip({{.channel = 1, .key = 60, .pitch = 61.0, .level = 1.0f, .attack = true, .pan = 1.0f,
                   .timbre = {.noise = true}}});
            // Fourteen more notes, one after another, take the other channels in turn
            for (int key = 1; key < NoteEncoder::maxVoices; ++key)
                trip({{.channel = 1, .key = key, .pitch = double(key), .level = 1.0f, .attack = true}});

            const auto frame = trip({{.channel = 1, .key = 72, .pitch = 72.0, .level = 1.0f, .attack = true}});
            const auto& voice = *frame.begin();
            expectEquals(describe(frame), String("2:72 72.00 1.0 attack"), "back on the first channel, the wheel in the middle");
            expect(!voice.timbre.noise && !voice.pan.has_value());
        }

        beginTest("A sixteenth voice at once is not sent");
        {
            NoteEncoder encoder;
            NoteFrame frame;
            for (int key = 0; key < 16; ++key)
                frame.add({.channel = 1, .key = key, .pitch = double(key), .level = 1.0f, .attack = true});
            tracktion::MidiMessageArray midi;
            encoder.encode(frame, midi, 0.0);
            int notes = 0;
            for (const auto& m : midi)
                notes += m.isNoteOn() ? 1 : 0;
            expectEquals(notes, NoteEncoder::maxVoices);
        }
    }

private:
    /** "channel:key pitch level [attack]; ..." */
    static String describe(const NoteFrame& frame) {
        StringArray voices;
        for (const auto& v : frame)
            voices.add(String(v.channel) + ":" + String(v.key) + " " + String(v.pitch, 2) + " " + String(v.level, 1)
                       + (v.attack ? " attack" : ""));
        return voices.joinIntoString("; ");
    }
};

static NoteDecoderTests noteDecoderTests;

}  // namespace MoTool::Tests
