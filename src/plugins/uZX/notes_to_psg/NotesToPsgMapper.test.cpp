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

        // The voice on each chip channel as its tone period's key, "." for a silent one: "60 . 64"
        const auto keys = [&](const PsgParamFrameData& params) {
            StringArray text;
            for (int n = 0; n < ChipMixer::numToneChannels; ++n) {
                String key(".");
                for (int k = 0; k < 128; ++k)
                    if (value(params, P::VolumeA + n) > 0 && value(params, P::TonePeriodA + n) == tuning->midiNoteToPeriod(k))
                        key = String(k);
                text.add(key);
            }
            return text.joinIntoString(" ");
        };
        const auto voice = [](int channel, int key, int priority = 0) {
            return NoteVoice{.channel = channel, .key = key, .pitch = double(key), .level = 1.0f, .priority = priority};
        };
        const auto frameOf = [](std::initializer_list<NoteVoice> voices) {
            NoteFrame frame;
            for (const auto& v : voices)
                frame.add(v);
            return frame;
        };

        beginTest("A voice gives a chip channel a volume, a period and its switches; the other channels are silenced");
        {
            ChipMixer mixer;
            const auto params = mixer.mix(frameOf({voice(7, 60)}), *tuning);

            expectEquals(value(params, P::VolumeA), 15, "whatever its MIDI channel");
            expectEquals(value(params, P::TonePeriodA), tuning->midiNoteToPeriod(60.0));
            expectEquals(value(params, P::ToneIsOnA), 1);
            expectEquals(value(params, P::NoiseIsOnA), 0);
            expectEquals(value(params, P::EnvelopeIsOnA), 0);

            expectEquals(value(params, P::VolumeB), 0);
            expectEquals(value(params, P::EnvelopeIsOnB), 0);
            expect(!params.isSet(P::ToneIsOnB) && !params.isSet(P::TonePeriodB), "a silent channel keeps the rest");
            expect(!params.isSet(P::EnvelopePeriod) && !params.isSet(P::EnvelopeShape), "nothing drives the envelope");
            expect(!params.isSet(P::NoisePeriod), "and no voice has the noise on");
        }

        beginTest("A voice's pitch, not its key, sets the period");
        {
            ChipMixer mixer;
            NoteFrame frame;
            frame.add({.channel = 1, .key = 60, .pitch = 61.5, .level = 0.5f});
            const auto params = mixer.mix(frame, *tuning);
            expectEquals(value(params, P::TonePeriodA), tuning->midiNoteToPeriod(61.5));
            expectEquals(value(params, P::VolumeA), 8);
        }

        beginTest("Timbre becomes the switches");
        {
            ChipMixer mixer;
            NoteFrame frame;
            frame.add({.channel = 1,
                       .key = 60,
                       .pitch = 60.0,
                       .level = 1.0f,
                       .timbre = {.tone = false, .noise = true, .envelope = true, .noisePeriod = 7}});
            const auto params = mixer.mix(frame, *tuning);
            expectEquals(value(params, P::ToneIsOnA), 0);
            expectEquals(value(params, P::NoiseIsOnA), 1);
            expectEquals(value(params, P::EnvelopeIsOnA), 1);
            expectEquals(value(params, P::NoisePeriod), 7);
        }

        beginTest("Three voices play; of more, the higher priority, then the later in the frame");
        {
            expectEquals(keys(ChipMixer().mix(frameOf({voice(1, 60), voice(1, 62), voice(1, 64)}), *tuning)),
                         String("64 62 60"), "a chord on one MIDI channel, the best voice on the first chip channel");
            expectEquals(keys(ChipMixer().mix(frameOf({voice(1, 60), voice(1, 62), voice(1, 64), voice(1, 65)}), *tuning)),
                         String("65 64 62"), "the earliest of four equals waits");
            expectEquals(
                keys(ChipMixer().mix(frameOf({voice(1, 60, 9), voice(1, 62), voice(1, 64), voice(1, 65)}), *tuning)),
                String("60 65 64"), "unless its priority is higher");
        }

        beginTest("A voice keeps its chip channel while it plays, and one that waited plays when a channel frees");
        {
            ChipMixer mixer;
            expectEquals(keys(mixer.mix(frameOf({voice(1, 60)}), *tuning)), String("60 . ."));
            expectEquals(keys(mixer.mix(frameOf({voice(1, 60), voice(2, 62)}), *tuning)), String("60 62 ."),
                         "a better voice arrives and the first stays where it is");
            expectEquals(keys(mixer.mix(frameOf({voice(2, 62)}), *tuning)), String(". 62 ."));
            expectEquals(keys(mixer.mix(frameOf({voice(2, 62), voice(3, 64), voice(4, 65), voice(5, 67)}), *tuning)),
                         String("67 65 64"), "the voice that had a channel loses it to three better ones");
            expectEquals(keys(mixer.mix(frameOf({voice(2, 62), voice(3, 64), voice(4, 65)}), *tuning)),
                         String("62 65 64"), "and comes back on the one that frees");
        }

        beginTest("A panned voice takes the chip channel that sounds nearest, the better voice first");
        {
            ChipMixer mixer;  // A left, B centre, C right
            const auto panned = [&](int channel, int key, float pan, int priority = 0) {
                auto v = voice(channel, key, priority);
                v.pan = pan;
                return v;
            };
            expectEquals(keys(mixer.mix(frameOf({panned(1, 60, 1.0f)}), *tuning)), String(". . 60"));
            expectEquals(keys(mixer.mix(frameOf({panned(1, 60, 1.0f), voice(1, 62), panned(1, 64, 0.4f)}), *tuning)),
                         String("62 64 60"), "one without a pan takes what is left");
            expectEquals(keys(mixer.mix(frameOf({panned(1, 60, 1.0f), panned(1, 65, 0.9f, 5)}), *tuning)),
                         String("60 . 65"), "a better voice asks for the same place, and the other moves");

            mixer.setChannelPans({0.0, 1.0, 0.5});  // the chip's ACB layout
            expectEquals(keys(ChipMixer(mixer).mix(frameOf({panned(1, 60, 1.0f)}), *tuning)), String(". 60 ."));
        }

        beginTest("A voice whose pitch drives the envelope sets its period and shape, and restarts it only when asked");
        {
            ChipMixer mixer;
            auto driver = voice(8, 36);
            driver.attack = true;
            driver.timbre = {.tone = false, .drivesEnvelope = true, .envelopeShape = EnvelopeShape::TriangleDown, .noisePeriod = 7};
            auto params = mixer.mix(frameOf({driver}), *tuning);
            expectEquals(value(params, P::EnvelopePeriod), tuning->midiNoteToPeriod(36.0, TuningSystem::Envelope));
            expectEquals(value(params, P::EnvelopeShape), 10, "the shape is a value to hold, as the chip's register has it");
            expect(!params.isSet(P::RetriggerEnvelope), "a note starting lets the envelope run");
            expectEquals(value(params, P::VolumeA), 0, "with nothing to sound it takes no channel");
            expect(!params.isSet(P::NoisePeriod), "the noise period is not its to set");

            driver.attack = false;
            driver.restartEnvelope = true;
            expectEquals(value(mixer.mix(frameOf({driver}), *tuning), P::RetriggerEnvelope), 1,
                         "a restart, inside a lasting note too");

            driver.timbre.envelope = true;
            params = mixer.mix(frameOf({driver}), *tuning);
            expectEquals(value(params, P::EnvelopeIsOnA), 1, "a buzzer: one voice is the envelope and its channel");
            expectEquals(value(params, P::ToneIsOnA), 0);
        }

        beginTest("Of two voices that drive the envelope the better one does, with or without a channel");
        {
            ChipMixer mixer;
            auto low = voice(2, 36, 2);
            low.timbre = {.drivesEnvelope = true, .envelopeShape = EnvelopeShape::SawDown};
            auto high = voice(9, 43, 9);
            high.timbre = {.tone = false, .drivesEnvelope = true, .envelopeShape = EnvelopeShape::Saw};
            const auto params = mixer.mix(frameOf({high, low, voice(3, 60, 3), voice(4, 62, 4), voice(5, 64, 5)}), *tuning);
            expectEquals(value(params, P::EnvelopePeriod), tuning->midiNoteToPeriod(43.0, TuningSystem::Envelope));
            expectEquals(value(params, P::EnvelopeShape), 12);
            expectEquals(keys(params), String("64 62 60"), "the other one found no channel either");
        }

        beginTest("The noise period comes from the best voice with the noise on that got a channel");
        {
            ChipMixer mixer;
            auto waiting = voice(1, 60, 1);
            waiting.timbre = {.noise = true, .noisePeriod = 3};
            auto quiet = voice(1, 62, 5);
            quiet.timbre = {.noise = true, .noisePeriod = 9};
            auto loud = voice(1, 64, 6);
            loud.timbre = {.noise = true, .noisePeriod = 20};
            expectEquals(value(mixer.mix(frameOf({waiting, quiet, loud, voice(1, 65, 7), voice(1, 67, 8)}), *tuning),
                               P::NoisePeriod), 20);
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

        beginTest("Notes on one MIDI channel play together; a fourth takes the oldest one's place and gives it back");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());
            auto& tuning = mapper.getTuningSystem();
            play(mapper, {MidiMessage::noteOn(1, 60, (uint8) 100)});

            auto messages = play(mapper, {MidiMessage::noteOn(1, 64, (uint8) 100)});
            expectEquals(periodOf(messages, 2), tuning.midiNoteToPeriod(64.0), "on the next chip channel");
            expectEquals(valuesOf(messages, 2, MidiCCType::Volume), String("12"));
            expectEquals(valuesOf(messages, 1, MidiCCType::Volume), String(), "the first note plays on");

            play(mapper, {MidiMessage::noteOn(1, 67, (uint8) 100)});
            messages = play(mapper, {MidiMessage::noteOn(1, 72, (uint8) 100)});
            expectEquals(messages.size(), 2, "the period of the first chip channel alone");
            expectEquals(periodOf(messages, 1), tuning.midiNoteToPeriod(72.0));

            messages = play(mapper, {MidiMessage::noteOff(1, 72)});
            expectEquals(periodOf(messages, 1), tuning.midiNoteToPeriod(60.0), "the held note returns");
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

        beginTest("A note told to drive the envelope sets its pitch and shape; controller 83 restarts it");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());
            auto& tuning = mapper.getTuningSystem();
            // On MIDI channel 9: controller 85 makes its notes drive the envelope, and with the tone off they
            // take no chip channel. The envelope's PSG goes out on the fourth channel from the base
            auto messages = play(mapper,
                                 {MidiMessage::controllerEvent(9, 85, 127),
                                  MidiMessage::controllerEvent(9, 80, 0),
                                  MidiMessage::controllerEvent(9, 70, 36),  // the third shape of eight
                                  MidiMessage::noteOn(9, 36, (uint8) 100)});
            expectEquals(periodOf(messages, 4), tuning.midiNoteToPeriod(36.0, TuningSystem::Envelope));
            expectEquals(valuesOf(messages, 4, MidiCCType::SoundVariation), String("10"), "which is the chip's shape 10");
            expectEquals(valuesOf(messages, 4, MidiCCType::GPB4RetriggerSwitch), String(),
                         "the shape goes as a value: the chip restarts by itself if it held another");
            expectEquals(valuesOf(messages, 1, MidiCCType::Volume), String("0"), "no chip channel sounds");

            messages = play(mapper, {MidiMessage::noteOff(9, 36), MidiMessage::noteOn(9, 38, (uint8) 100)});
            expectEquals(periodOf(messages, 4), tuning.midiNoteToPeriod(38.0, TuningSystem::Envelope));
            expectEquals(messages.size(), 3, "another note moves the envelope's pitch, in three parts, and lets it run");

            messages = play(mapper, {MidiMessage::controllerEvent(9, 83, 127)});
            expectEquals(messages.size(), 2, "a restart inside the lasting note, and nothing else");
            expectEquals(valuesOf(messages, 4, MidiCCType::GPB4RetriggerSwitch), String("1 0"));

            messages = play(mapper, {MidiMessage::controllerEvent(9, 83, 0), MidiMessage::controllerEvent(9, 70, 36)});
            expectEquals(messages.size(), 0, "the release and the same shape again do nothing");
        }

        beginTest("Notes on any MIDI channel play, the higher channel first, and the PSG goes out from the base channel");
        {
            NotesToPsgMapper mapper;
            mapper.setBaseChannel(2);
            mapper.setTuningSystem(createTestTuning());
            auto& tuning = mapper.getTuningSystem();

            auto messages =
                play(mapper, {MidiMessage::noteOn(1, 60, (uint8) 100), MidiMessage::noteOn(16, 64, (uint8) 100)});
            expectEquals(periodOf(messages, 2), tuning.midiNoteToPeriod(64.0), "the first chip channel is on channel 2");
            expectEquals(periodOf(messages, 3), tuning.midiNoteToPeriod(60.0));
            for (const auto& m : messages)
                expect(m.isController(), "no note goes on to the chip");

            messages = play(mapper, {MidiMessage::controllerEvent(1, 86, 100), MidiMessage::noteOn(14, 67, (uint8) 100),
                                     MidiMessage::noteOn(15, 69, (uint8) 100)});
            expectEquals(periodOf(messages, 4), tuning.midiNoteToPeriod(69.0), "channel 15 takes the last chip channel");
            expectEquals(messages.size(), 5, "its volume, period and two switches, so channel 14 waits");
            expectEquals(valuesOf(messages, 3, MidiCCType::CC20PeriodCoarse), String(),
                         "and channel 1 plays on, at the priority controller 86 gave it");
        }

        beginTest("A block's output keeps each message's time; what has no channel passes through");
        {
            NotesToPsgMapper mapper;
            mapper.setTuningSystem(createTestTuning());
            auto messages = play(mapper,
                                 {MidiMessage::noteOn(1, 60, (uint8) 100).withTimeStamp(0.1),
                                  MidiMessage::createSysExMessage("uZX", 3).withTimeStamp(0.25),
                                  MidiMessage::noteOff(1, 60).withTimeStamp(0.5)});
            int checked = 0;
            for (const auto& m : messages) {
                if (m.isSysEx()) {
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
