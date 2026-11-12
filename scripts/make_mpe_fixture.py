#!/usr/bin/env python3
"""Write tests/fixtures/MPE chord.uzx: a check of the note wire by ear and by test.

One track with MIDI to PSG and an AY chip, and a wide C major seventh that starts as a cluster and opens
into tune over two seconds of its four. Each voice starts bent toward A3, by 0.85 of a semitone for every key
it is away, so the four start within about three semitones of each other, none of them on a note: the lowest
voice almost twelve semitones sharp, the highest six flat.

- "Tones", a clip in MPE mode: C3 and B3, a bend each. MPE mode gives every note a channel from 2 up.
- "Envelope tone", on channel 15: E4 with its envelope on, so the chip's envelope shapes its level.
- "Envelope", on channel 16: G2 with its tone off, driving the envelope as a sawtooth. It takes no chip channel.

A clip in MPE mode holds a bend, a pressure and one timbre value for each note, and no switches, so a voice
that needs its own switches has a clip and a channel of its own. NotesToPsgPlugin.test.cpp plays the file."""

from pathlib import Path

from make_guide_fixture import edit_xml

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "tests/fixtures/MPE chord.uzx"

BEATS = 8.0          # 4 s at 120 BPM
GLIDE_BEATS = 4.0    # in tune after 2 s
FRAME_BEATS = 0.04   # a 50 Hz frame
VELOCITY = 127       # the chip's full volume, 15
KEY_CENTRE, SEMITONES_PER_KEY = 57, -0.85
MEMBER_BEND_RANGE = 48.0  # semitones, on channels 2 to 16
PITCH_WHEEL = 0x1005      # tracktion::MidiControllerEvent::pitchWheelType
SAWTOOTH = 2 * 127 // 7   # the third of the eight envelope shapes, as controller 70


def bend_points(key):
    """(beat, semitones) for every frame of the glide, from the key's starting bend to none."""
    start = (key - KEY_CENTRE) * SEMITONES_PER_KEY
    frames = round(GLIDE_BEATS / FRAME_BEATS)
    return [(round(i * FRAME_BEATS, 6), round(start * (1 - i / frames), 6)) for i in range(frames + 1)]


def wheel(semitones):
    """juce::MidiMessage::pitchbendToPitchwheelPos."""
    return int(8192 + semitones / MEMBER_BEND_RANGE * (8191 if semitones > 0 else 8192))


def clip_xml(ids, name, channel, body, mpe=False):
    return f'''    <MIDICLIP name="{name}" start="0.0" length="4.0" offset="0.0" id="{next(ids)}" sync="0" currentTake="0" speed="1.0" colour="ff55ff00"{' mpeMode="1"' if mpe else ''}>
      <AUTOMATIONCURVES />
      <QUANTISATION />
      <GROOVE />
      <SEQUENCE ver="1" channelNumber="{channel}">
{chr(10).join(body)}
      </SEQUENCE>
    </MIDICLIP>'''


def mpe_note(key):
    points = bend_points(key)
    lines = [f'        <NOTE p="{key}" b="0.0" l="{BEATS}" v="{VELOCITY}" c="0" bend="{points[0][1]}">']
    lines += [f'          <PITCHBEND b="{beat}" v="{semitones}" />' for beat, semitones in points[1:]]
    return lines + ['        </NOTE>']


def channel_voice(key, controllers):
    """A note alone on its channel: its switches as controllers, its bend as the channel's pitch wheel."""
    lines = [f'        <NOTE p="{key}" b="0.0" l="{BEATS}" v="{VELOCITY}" c="0" />']
    lines += [f'        <CONTROL b="0.0" type="{number}" val="{value << 7}" />' for number, value in controllers]
    return lines + [f'        <CONTROL b="{beat}" type="{PITCH_WHEEL}" val="{wheel(semitones)}" />'
                    for beat, semitones in bend_points(key)]


def main():
    ids = iter(range(1100, 10000))
    clips = [
        clip_xml(ids, "Tones", 1, mpe_note(48) + mpe_note(59), mpe=True),
        clip_xml(ids, "Envelope tone", 15, channel_voice(64, [(82, 127)])),
        clip_xml(ids, "Envelope", 16, channel_voice(43, [(85, 127), (80, 0), (70, SAWTOOTH)])),
    ]
    track = f'''  <TRACK id="{next(ids)}" name="MPE chord">
    <MODIFIERS />
    <OUTPUTDEVICES>
      <DEVICE name="(default audio output)" />
    </OUTPUTDEVICES>
    <CLIPSLOTS />
    <PLUGIN type="uzxmidi2psg" windowLocked="1" id="{next(ids)}" enabled="1" midiBase="1" tuningTable="EqualTemperament" remapOnTempoChange="1" />
    <PLUGIN type="aychip" windowLocked="1" id="{next(ids)}" enabled="1" midi="1" chip="AY" clock="1.7734" noDC="1" monitor="0" volume="0.5" layout="ACB" stereo="0.5" channelA="1" channelB="1" channelC="1" toneA="1" toneB="1" toneC="1" noiseA="1" noiseB="1" noiseC="1" envelopeA="1" envelopeB="1" envelopeC="1" windowSamples="1024" gain="1.0" triggerMode="RisingEdge" triggerLevel="0.0" remapOnTempoChange="1">
      <MODIFIERASSIGNMENTS />
    </PLUGIN>
    <PLUGIN type="volume" id="{next(ids)}" enabled="1" remapOnTempoChange="1">
      <MODIFIERASSIGNMENTS />
    </PLUGIN>
    <PLUGIN type="level" id="{next(ids)}" enabled="1" />
{chr(10).join(clips)}
    <VIEWSTATE height="130" />
  </TRACK>'''
    FIXTURE.write_text(edit_xml([track], "a major seventh that glides into tune, each voice on a channel of its own",
                                "scripts/make_mpe_fixture.py"))
    print(f"Wrote {FIXTURE.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
