#include "NotesToPsgMapper.h"

#include <JuceHeader.h>

#include <atomic>
#include <thread>

#include "../../../models/PsgCC.h"
#include "../../../models/tuning/TuningRegistry.h"
#include "../../../models/tuning/TuningSystemBase.h"

namespace MoTool::Tests {

using namespace MoTool::uZX;
using namespace juce;

//==============================================================================
/** Frames in, the chip's parameters out: no MIDI. */
class ChipMixerTests : public UnitTest
{
public:
    ChipMixerTests() : UnitTest("ChipMixer", "MoTool") {}

    void runTest() override {
        const std::shared_ptr<TuningSystem> tuning = makeBuiltinTuning(BuiltinTuningType::EqualTemperament);
        using P = PsgParamType;
        const auto value = [](const PsgParamFrameData& params, PsgParamType param) {
            return params.isSet(param) ? int(params.getRaw(param)) : -1;
        };

        beginTest("A voice gives its channel a volume, a period and its switches; the other channels are silenced");
        {
            NoteFrame frame;
            frame.add({.channel = 2, .key = 60, .pitch = 60.0, .level = 1.0f});
            const auto params = mixVoices(frame, *tuning, 1);

            expectEquals(value(params, P::VolumeB), 15);
            expectEquals(value(params, P::TonePeriodB), tuning->midiNoteToPeriod(60.0));
            expectEquals(value(params, P::ToneIsOnB), 1);
            expectEquals(value(params, P::NoiseIsOnB), 0);
            expectEquals(value(params, P::EnvelopeIsOnB), 0);

            expectEquals(value(params, P::VolumeA), 0);
            expectEquals(value(params, P::EnvelopeIsOnA), 0);
            expect(!params.isSet(P::ToneIsOnA) && !params.isSet(P::TonePeriodA), "a silent channel keeps the rest");
            expect(!params.isSet(P::EnvelopePeriod) && !params.isSet(P::EnvelopeShape), "no envelope voice");
        }

        beginTest("A voice's pitch, not its key, sets the period");
        {
            NoteFrame frame;
            frame.add({.channel = 1, .key = 60, .pitch = 61.5, .level = 0.5f});
            expectEquals(value(mixVoices(frame, *tuning, 1), P::TonePeriodA), tuning->midiNoteToPeriod(61.5));
        }

        beginTest("Timbre becomes the switches");
        {
            NoteFrame frame;
            frame.add({.channel = 1,
                       .key = 60,
                       .pitch = 60.0,
                       .level = 1.0f,
                       .timbre = {.tone = false, .noise = true, .envelope = true}});
            const auto params = mixVoices(frame, *tuning, 1);
            expectEquals(value(params, P::ToneIsOnA), 0);
            expectEquals(value(params, P::NoiseIsOnA), 1);
            expectEquals(value(params, P::EnvelopeIsOnA), 1);
        }

        beginTest("The last voice to start on a channel takes it");
        {
            NoteFrame frame;
            frame.add({.channel = 1, .key = 60, .pitch = 60.0, .level = 1.0f});
            frame.add({.channel = 1, .key = 64, .pitch = 64.0, .level = 0.6f});
            const auto params = mixVoices(frame, *tuning, 1);
            expectEquals(value(params, P::TonePeriodA), tuning->midiNoteToPeriod(64.0));
            expectEquals(value(params, P::VolumeA), 9);
        }

        beginTest("The voice on the fourth channel plays the envelope, and restarts it only when asked");
        {
            NoteFrame frame;
            frame.add({.channel = 8,
                       .key = 36,
                       .pitch = 36.0,
                       .level = 1.0f,
                       .attack = true,
                       .timbre = {.envelopeShape = 10, .noisePeriod = 7}});
            auto params = mixVoices(frame, *tuning, 5);
            expectEquals(value(params, P::EnvelopePeriod), tuning->midiNoteToPeriod(36.0, TuningSystem::Envelope));
            expectEquals(value(params, P::NoisePeriod), 7);
            expectEquals(value(params, P::VolumeA), 0, "it has no channel of its own");
            expect(!params.isSet(P::EnvelopeShape), "a note starting lets the envelope run");

            frame.begin()->attack = false;
            frame.begin()->restartEnvelope = true;
            expectEquals(value(mixVoices(frame, *tuning, 5), P::EnvelopeShape), 10,
                         "a restart, inside a lasting note too, writes the shape");
        }
    }
};

static ChipMixerTests chipMixerTests;

//==============================================================================
/** The mixer as a MIDI effect: the chip-note stream in, PSG controllers out. */
class MidiToPsgConverterTests : public UnitTest
{
public:
    MidiToPsgConverterTests() : UnitTest("NotesToPsgMapper", "MoTool") {}

private:
    std::unique_ptr<TuningSystem> createTestTuning() {
        TuningOptions options{.builtinTable = BuiltinTuningEnum::EqualTemperament};
        return makeBuiltinTuning(options);
    }

    /** Runs a block through the mapper and returns what it wrote. */
    static tracktion::MidiMessageArray
    play(NotesToPsgMapper& mapper, std::initializer_list<MidiMessage> messages, bool allNotesOff = false) {
        tracktion::MidiMessageArray buffer;
        buffer.isAllNotesOff = allNotesOff;
        for (const auto& m : messages)
            buffer.addMidiMessage(m, {});
        MidiBufferContext context{buffer, 0, 480, {}, 48000.0};
        mapper(context);
        return buffer;
    }

    /** The values a controller is sent with on a MIDI channel, in order, as "v v v". */
    static String valuesOf(const tracktion::MidiMessageArray& messages, int channel, MidiCCType controller) {
        StringArray values;
        for (const auto& m : messages)
            if (m.isController() && m.getChannel() == channel
                && m.getControllerNumber() == static_cast<int>(controller))
                values.add(String(m.getControllerValue()));
        return values.joinIntoString(" ");
    }

    static int periodOf(const tracktion::MidiMessageArray& messages, int channel) {
        return (valuesOf(messages, channel, MidiCCType::CC20PeriodCoarse).getIntValue() << 7)
               | valuesOf(messages, channel, MidiCCType::CC52PeriodFine).getIntValue();
    }

public:
    void runTest() override {
        beginTest("A note sets its channel's volume, period and switches; its end only the volume");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());

            auto messages = play(mapper, {MidiMessage::noteOn(1, 60, (uint8) 100)});
            expectEquals(valuesOf(messages, 1, MidiCCType::Volume), String("12"));
            expectEquals(periodOf(messages, 1), mapper.getTuningSystem().midiNoteToPeriod(60.0));
            expectEquals(valuesOf(messages, 1, MidiCCType::GPB1ToneSwitch), String("1"));
            expectEquals(valuesOf(messages, 1, MidiCCType::GPB2NoiseSwitch), String("0"));
            expectEquals(valuesOf(messages, 1, MidiCCType::GPB3EnvSwitch), String("0"));
            expectEquals(valuesOf(messages, 2, MidiCCType::Volume), String("0"), "the first frame silences the others");
            expectEquals(valuesOf(messages, 3, MidiCCType::Volume), String("0"));

            messages = play(mapper, {MidiMessage::noteOff(1, 60)});
            expectEquals(messages.size(), 1);
            expectEquals(valuesOf(messages, 1, MidiCCType::Volume), String("0"));
        }

        beginTest("Only what changes is sent");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());
            play(mapper, {MidiMessage::noteOn(1, 60, (uint8) 100)});

            auto messages = play(mapper, {MidiMessage::noteOff(1, 60), MidiMessage::noteOn(1, 62, (uint8) 100)});
            expectEquals(messages.size(), 2, "a new note at the same level: its period alone");
            expectEquals(periodOf(messages, 1), mapper.getTuningSystem().midiNoteToPeriod(62.0));

            messages = play(mapper, {MidiMessage::aftertouchChange(1, 62, 127)});
            expectEquals(messages.size(), 1);
            expectEquals(valuesOf(messages, 1, MidiCCType::Volume), String("15"));

            messages = play(mapper, {MidiMessage::pitchWheel(1, 16383)});
            expectEquals(messages.size(), 2);
            expectEquals(periodOf(messages, 1), mapper.getTuningSystem().midiNoteToPeriod(62.0 + notePitchBendRange));
        }

        beginTest("The last note played on a channel sounds, and an older held one returns when it ends");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());
            play(mapper, {MidiMessage::noteOn(1, 60, (uint8) 100)});

            auto messages = play(mapper, {MidiMessage::noteOn(1, 64, (uint8) 100)});
            expectEquals(periodOf(messages, 1), mapper.getTuningSystem().midiNoteToPeriod(64.0));
            expectEquals(valuesOf(messages, 1, MidiCCType::GPB1ToneSwitch), String(), "the tone is on already");

            messages = play(mapper, {MidiMessage::noteOff(1, 64)});
            expectEquals(periodOf(messages, 1), mapper.getTuningSystem().midiNoteToPeriod(60.0));
            expectEquals(valuesOf(messages, 1, MidiCCType::Volume), String(), "at the same level");
        }

        beginTest("Timbre controllers set the note's switches and are not passed on");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());
            auto messages = play(mapper,
                                 {MidiMessage::controllerEvent(1, 80, 0),
                                  MidiMessage::controllerEvent(1, 82, 127),
                                  MidiMessage::controllerEvent(1, 64, 100)});
            expectEquals(valuesOf(messages, 1, MidiCCType::GPB1ToneSwitch), String(), "no note yet");
            expectEquals(
                valuesOf(messages, 1, MidiCCType::SustainOnOff), String(), "and what is not timbre stops here");

            messages = play(mapper, {MidiMessage::noteOn(1, 60, (uint8) 100)});
            expectEquals(valuesOf(messages, 1, MidiCCType::GPB1ToneSwitch), String("0"));
            expectEquals(valuesOf(messages, 1, MidiCCType::GPB3EnvSwitch), String("1"));
        }

        beginTest("A note on the fourth channel plays the envelope; controller 83 or another shape restarts it");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());
            auto& tuning = mapper.getTuningSystem();
            auto messages =
                play(mapper, {MidiMessage::controllerEvent(4, 70, 10), MidiMessage::noteOn(4, 36, (uint8) 100)});
            expectEquals(periodOf(messages, 4), tuning.midiNoteToPeriod(36.0, TuningSystem::Envelope));
            expectEquals(valuesOf(messages, 4, MidiCCType::SoundVariation), String("10"), "another shape is written");
            expectEquals(valuesOf(messages, 4, MidiCCType::GPB4RetriggerSwitch), String("1 0"));

            messages = play(mapper, {MidiMessage::noteOff(4, 36), MidiMessage::noteOn(4, 38, (uint8) 100)});
            expectEquals(periodOf(messages, 4), tuning.midiNoteToPeriod(38.0, TuningSystem::Envelope));
            expectEquals(messages.size(), 2, "another note moves the envelope's pitch and lets it run");

            messages = play(mapper, {MidiMessage::controllerEvent(4, 83, 127)});
            expectEquals(messages.size(), 3, "a restart inside the lasting note writes the shape, and nothing else");
            expectEquals(valuesOf(messages, 4, MidiCCType::SoundVariation), String("10"));
            expectEquals(valuesOf(messages, 4, MidiCCType::GPB4RetriggerSwitch), String("1 0"));

            messages = play(mapper, {MidiMessage::controllerEvent(4, 83, 0), MidiMessage::controllerEvent(4, 70, 10)});
            expectEquals(messages.size(), 0, "the release and the same shape again do nothing");
        }

        beginTest("Other channels pass through and are not played");
        {
            NotesToPsgMapper mapper;
            mapper.setBaseChannel(2);
            mapper.setTuningSystem(createTestTuning());

            auto messages =
                play(mapper, {MidiMessage::noteOn(1, 60, (uint8) 100), MidiMessage::noteOn(6, 60, (uint8) 100)});
            expectEquals(messages.size(), 2);
            expect(messages[0].isNoteOn() && messages[1].isNoteOn(), "channels 1 and 6 are outside 2 to 5");

            messages = play(mapper, {MidiMessage::noteOn(2, 60, (uint8) 100)});
            expectEquals(
                valuesOf(messages, 2, MidiCCType::Volume), String("12"), "channel 2 is the first chip channel");
        }

        beginTest("A block's output keeps each message's time, passed-through ones too");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());
            auto messages = play(mapper,
                                 {MidiMessage::noteOn(1, 60, (uint8) 100).withTimeStamp(0.1),
                                  MidiMessage::noteOn(9, 60, (uint8) 100).withTimeStamp(0.25),
                                  MidiMessage::noteOff(1, 60).withTimeStamp(0.5)});
            int checked = 0;
            for (const auto& m : messages) {
                if (m.getChannel() == 9) {
                    expectWithinAbsoluteError(m.getTimeStamp(), 0.25, 1e-9, "passed through at its own time");
                    ++checked;
                } else if (m.getChannel() == 1 && m.isController()
                           && m.getControllerNumber() == (int) MidiCCType::Volume) {
                    expectWithinAbsoluteError(m.getTimeStamp(),
                                              m.getControllerValue() > 0 ? 0.1 : 0.5,
                                              1e-9,
                                              "the note's volume at the note on and off");
                    ++checked;
                }
            }
            expectEquals(checked, 3);
        }

        beginTest("Moving the channel range silences a held note on the channels it leaves");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());
            play(mapper, {MidiMessage::noteOn(1, 60, (uint8) 100)});

            mapper.setBaseChannel(5);
            const auto messages = play(mapper, {});
            expectEquals(valuesOf(messages, 1, MidiCCType::Volume), String("0"));
            expectEquals(valuesOf(messages, 5, MidiCCType::Volume), String(), "nothing on the new ones yet");
        }

        beginTest("A reset between blocks silences the held note in the next block, even an empty one");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());
            play(mapper, {MidiMessage::noteOn(1, 60, (uint8) 100)});

            mapper.reset();  // as a MIDI panic does, on the message thread
            expectEquals(valuesOf(play(mapper, {}), 1, MidiCCType::Volume), String("0"));
            expectEquals(play(mapper, {}).size(), 0, "once");
        }

        beginTest("After an all-notes-off the chip is silenced, told so, and sent the next note whole");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());
            play(mapper, {MidiMessage::noteOn(1, 60, (uint8) 100)});

            auto messages = play(mapper, {}, true);
            expect(messages.isAllNotesOff, "the chip after the mapper hears the all-notes-off too");
            expectEquals(valuesOf(messages, 1, MidiCCType::Volume), String("0"));

            // The chip may be another one by now, as after a stop: it needs the period and the switches again
            messages = play(mapper, {MidiMessage::noteOn(1, 60, (uint8) 100)});
            expectEquals(valuesOf(messages, 1, MidiCCType::Volume), String("12"));
            expectEquals(periodOf(messages, 1), mapper.getTuningSystem().midiNoteToPeriod(60.0));
            expectEquals(valuesOf(messages, 1, MidiCCType::GPB1ToneSwitch), String("1"));
        }

        beginTest("Retuning while the audio thread renders");
        {
            NotesToPsgMapper mapper;
            std::atomic<bool> done{false};
            std::thread audio([&] {
                while (!done) {
                    play(mapper,
                         {MidiMessage::noteOn(1, 60, (uint8) 100),
                          MidiMessage::pitchWheel(1, 9000).withTimeStamp(0.001),
                          MidiMessage::noteOff(1, 60).withTimeStamp(0.002)});
                }
            });
            for (int i = 0; i < 50000; ++i)
                mapper.setTuningSystem(createTestTuning());
            done = true;
            audio.join();
            expect(true, "no crash");
        }
    }
};

static MidiToPsgConverterTests midiToPsgmapperTests;

}  // namespace MoTool::Tests
