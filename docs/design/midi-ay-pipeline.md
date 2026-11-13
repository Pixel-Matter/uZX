# MIDI to AY pipeline

> **Status: target design, partly implemented.** Both values with their encoders and decoders, and the chip mixer with voice allocation, are in the code; [What exists today](#what-exists-today) lists each stage and its gap. Chip effects are not, and the instrument has no timbre of its own: a note gets the controllers of the channel it is played on. The hardware setups in [Where the stages run](#where-the-stages-run) are a goal with nothing built.

How notes played on a keyboard or written in a clip become AY register writes. Code: `src/plugins/uZX/instrument/`, `src/plugins/uZX/midi_effects/`, `src/plugins/uZX/notes_to_psg/`, `src/plugins/uZX/aychip/`, `src/models/PsgCC.*`.

## Two domains

Plugins on a track talk to each other only through MIDI and audio, so everything between the keyboard and the chip travels as MIDI. That MIDI belongs to one of two domains, and most of the design is keeping them apart.

- **The note domain** is ordinary MIDI: notes and controllers from a keyboard, a clip, a looper or a sequencer, on any channels, possibly MPE or MIDI 2. Up to four notes sound at once on one chip, but nothing here knows that. **MIDI channels in the note domain are not AY channels.** A few controllers get a chip meaning later (envelope and noise modulation, noise pitch, oscillator shape); until the chip mixer they are still ordinary controllers.
- **The PSG domain** is the chip's registers, written as controllers on four MIDI channels: A, B, C, and one for the envelope and the noise. A PSG clip plays straight into this domain.

The chip mixer is the only stage that knows both.

## Stages

```
performance MIDI       chip-note stream (per frame)          PSG controllers
keyboard, clips, ─► instrument ─► chip fx ─► chip mixer ═══════════════► AY chip
loopers, MPE        ADSR,         vibrato,   voices → A/B/C/envelope,
                    quantize      arp, echo  priority, tuning
                                             PSG clip ─────────────────┘
                    └────── NoteFrame ──────┘└─────── PsgPatch ────────┘
```

| Stage | Takes | Gives | Does |
| --- | --- | --- | --- |
| Chip instrument | performance MIDI | chip-note stream | Plays the notes: applies ADSR and other per-note shaping, quantizes to the project or track frame rate. Stays in the note domain and its channel space; it does not produce PSG values |
| Chip effects | chip-note stream | chip-note stream | Echo, portamento, arpeggio, vibrato. Any number, in any order |
| Chip mixer | chip-note stream, all channels | PSG controllers | Assigns sounding voices to chip channels by priority, turns pitch into periods with the tuning, turns timbre into mixer and envelope bits |
| AY chip | PSG controllers | audio | Writes the registers |

**Performance MIDI** is events and gates: note on and off, velocity, whatever a keyboard sends. **The chip-note stream** is what the instrument has already played: for each frame, every sounding voice with its pitch, level and timbre stated outright. Gates, velocity and envelopes no longer exist in it, which is what makes it a simpler subset of MIDI that can be turned into PSG later.

Whether a playing mode or an effect lives inside the instrument or as a plugin of its own is a packaging choice, not an architecture one (see the next section). Where it goes in the chain follows from what it needs:

- An effect that needs the instrument's envelope to start again goes before the instrument, as an ordinary MIDI effect: a classic arpeggiator, note repeat.
- An effect that reshapes a voice that is already sounding goes after it: vibrato, portamento, tracker-style arpeggio and ornaments, echo.

Echo adds voices at a lower priority, and the mixer decides whether they get a chip channel.

## Two values, and the bookkeeping layer

Modular stages were meant to make development and testing easier. What defeated that was MIDI state bookkeeping mixed into every stage's logic: which notes are held, which note-off belongs to which note, what was last sent, what to do on a reset or a transport jump. The bookkeeping is therefore its own layer, written once, and stage logic never sees a MIDI message.

Each domain has one value and one codec:

| Domain | Value | Codec | State |
| --- | --- | --- | --- |
| Note | `NoteFrame`: the voices sounding in one frame. A voice has a channel, a pitch in fractional semitones, a level, a priority, an optional pan, and timbre fields (tone, noise and envelope on, whether its pitch drives the envelope, envelope shape, noise period) | Note codec: one voice per MIDI channel, on the member channels of an MPE lower zone | In the shared decoder and encoder |
| PSG | `PsgPatch`: the PSG parameters set in one frame. The envelope shape is a value to hold, and the envelope retrigger is the event that writes the register and restarts the envelope; a frame can then state the shape without restarting, as the first frame of a clip that begins inside an envelope note must. Today this is `PsgParamFrameData` | `PsgCC` | The chip's registers |

Every note-domain stage has the same shape:

```
MIDI in ─► decoder ─► NoteFrame ─► logic ─► NoteFrame ─► encoder ─► MIDI out
           shared                  the stage's own        shared
```

The decoder and encoder own everything that is about MIDI: held notes and their note-offs, per-note pitch and level, controller values, all-notes-off, transport jumps and resets, frame boundaries, what was last sent, and sending on a block with no input. After a reset or a jump the encoder forgets what it sent, so the next frame goes out whole.

`NoteFrame`, `NoteVoice`, `NoteEncoder` and `NoteDecoder` are in `src/plugins/uZX/midi_effects/NoteFrame.*`; `PsgEncoder`, which remembers what the chip was last sent, is in `src/models/PsgCC.*`. Two stages have this shape today:

- `MPEInstrumentFx`, an instrument, whose input is performance MIDI: JUCE's `MPEInstrument` and `VoiceManager` decode the notes, each voice's `renderFrame` adds itself to the frame, `NoteControllers` gives each voice the timbre, pan and priority of the channel it was played on, and `NoteEncoder` sends what changed.
- `NotesToPsgMapper`, the mixer: `NoteDecoder` reads the stream, `ChipMixer` turns a frame into the chip's parameters, and `PsgEncoder` sends what changed.

The chip-note stream carries its own frame grid: a frame is wherever the stream has messages, so a reader takes one after the messages of each time and adds no delay of its own.

All four stages by layer. In a bookkeeping box the upper name reads the wire and the lower name writes it; the chip writes nothing back, and its registers are its state. A PSG clip writes the PSG wire as the mixer does.

![The four stages across, with transport, bookkeeping and logic as rows](../images/midi-ay-pipeline/layers.svg)

The logic owns only musical state: an envelope's phase, an LFO's phase, an arpeggio's step, an echo's history, which voice has which chip channel. Vibrato is `pitch += lfo(frame)`. Its test feeds frames and compares frames.

A plugin is then one decoder, a chain of frame functions, and one encoder, which is why "inside the instrument or a separate effect" stops mattering: the same function fits either.

## The chip mixer

The mixer takes the chip-note stream on all channels and produces one `PsgPatch` per frame. The chip has three channels and two generators shared by them, the envelope and the noise. Each is a resource that voices ask for, and where more ask than fit, the best voice gets it: the higher priority, then the later voice in the frame, which from a keyboard is the newer note.

| Resource | A voice asks for it by | Who gets it |
| --- | --- | --- |
| A chip channel | Having tone, noise or envelope on | The three best |
| The envelope: period, shape, restart | A timbre field saying its pitch drives the envelope | The best |
| The noise: period | Having noise on | The best of those that got a chip channel |

- **Priority** is a value of the voice, not its channel: under MPE the channel a note lands on means nothing. A voice that nothing gave a priority has its channel's number, which is "the higher channel wins" for plain MIDI. Echo adds voices at a lower priority.
- **The envelope and the channels are asked for apart.** A voice that drives the envelope with its tone, noise and envelope off takes no chip channel: it only moves the envelope under the other voices, as a tracker's envelope column does. A voice that drives the envelope and has its envelope on is a buzzer by itself. A buzzer whose envelope is an interval away from its tone is two voices, one for each pitch.
- **Which chip channel** a voice gets is heard in stereo. A voice with a pan takes the channel that sounds nearest to it, if no better voice asked for the same; otherwise a voice keeps the channel it had in the frame before; otherwise it takes the lowest free one. A pan is matched to where the chip's stereo layout puts A, B and C, which the plugin reads from the first AY chip after it, as it does the clock.
- **Pitch.** The tuning turns a voice's pitch into a tone period, or an envelope period for the voice that drives the envelope, at the clock of the chip the mixer plays on. A voice's pitch is the pitch that sounds: a triangle takes two of the envelope's periods to repeat, so the tuning has a period mode for it (`TuningSystem::EnvelopeTriangle`, picked by `envelopePeriodMode` in `EnvelopeShape.h`) that gives half a sawtooth's period.
- **Timbre.** The voice's timbre fields become the tone, noise and envelope switches. The voice that drives the envelope gives the envelope shape in every frame and the retrigger only when it asks for a restart: a note can start, move the envelope's pitch or last with the envelope running on.

`ChipMixer` in `src/plugins/uZX/notes_to_psg/NotesToPsgMapper.*` is this logic; its only state is which voice had which chip channel. A chip channel without a voice gets volume 0 and its envelope off. After an all-notes-off, a reset or a move to other MIDI channels the mixer silences the chip and forgets what it sent, so the next frame goes out whole: a chip that replaced another gets everything it needs.

## The wires

**Note wire, instrument to mixer.** One voice per MIDI channel, so a voice's pitch wheel and controllers are its alone. `NoteEncoder` sends voices on channels 2 to 16, the member channels of an MPE lower zone, and nothing on channel 1, its master. A voice keeps its channel while it sounds; a new voice takes the channel that has been free the longest, and starts with a reset of all controllers on it, since another voice's are still there. That caps the stream at fifteen voices; MIDI 2 or an in-process path would replace only the note codec.

The channel a voice has in a frame is not the channel it is sent on. It says, with the key, which voice this is: the channel a note was played on, for the instrument. What that channel meant goes with the voice as values: its priority and its controllers.

| A voice's | Travels as |
| --- | --- |
| Start and end | Note on, note off; a note on for a key that sounds starts it again |
| Level | Note-on velocity, then polyphonic aftertouch |
| Pitch | The pitch wheel of its channel, from the key: ±48 semitones on channels 2 to 16 and ±2 on channel 1, an MPE lower zone's ranges (`notePitchBendRange`) |
| Tone, noise, envelope on | Controllers 80, 81, 82 on its channel; on from 64 |
| Its pitch drives the envelope | Controller 85 on its channel; on from 64 |
| Envelope shape | Controller 70 on its channel: the eight shapes over 0 to 127, sixteen values each |
| Noise period | Controller 2 on its channel: the 32 periods over 0 to 127, four values each |
| Envelope restart | Controller 83 on the channel of a voice that drives the envelope, from 64. Another shape on controller 70 is only a value here; the chip restarts when it gets one, since it cannot sound without a write |
| Priority | Controller 86 on its channel, 0 to 127; the channel's number until it is sent |
| Pan | Controller 10 on its channel, 0 left to 127 right; no place asked for until it is sent |

| A fresh channel | Controller 121, reset all controllers: tone on, noise and envelope off, no pan, the channel's own priority, the wheel in the middle |

The wire is not strict MPE: level is polyphonic aftertouch where MPE has channel pressure, channel 1 is read as one more channel of voices and not as the zone's master, and controller 121 here resets pan and the envelope shape as well, which MIDI's recommended practice leaves alone.

**What a MIDI clip can hold of this wire.** A clip plays on one channel, with its controllers and its pitch wheel, so a clip per voice on a channel each writes the wire in full. A clip in MPE mode gives every note a channel from 2 up and holds a bend for each note, in semitones with points along the note, which the mixer reads as that voice's pitch. It holds no switches for a note: MPE has a pressure and one timbre value (controller 74) per note, neither of which this wire reads. A voice that needs its own switches therefore has a clip and a channel of its own, clear of the channels an MPE clip hands out. `tests/fixtures/MPE chord.uzx`, written by `scripts/make_mpe_fixture.py`, is such a project: two tones in an MPE clip, a tone with its envelope on and a voice that drives the envelope on channels 15 and 16, all on one track with the mixer and a chip.

A note clip or a keyboard can send these controllers straight to the mixer, on any channels. Sent to the instrument, they apply to the notes played on their channel: the instrument reads them with the same `NoteControllers` the decoder uses, so a controller means the same before and after it. Controllers hold their value from note to note on a channel.

The note wire reads controllers as MIDI reads its own, not as chip numbers, since keyboards, knobs and clip lanes write it: a switch is on from 64, and a value with fewer steps than a controller takes the controller's whole range, an equal part for each step (`stepToController`, `controllerToStep`). The encoder sends a switch as 127 or 0 and the last step as 127.

The envelope shape on this wire, in a voice's timbre and anywhere the interface offers one is `EnvelopeShape` in `src/plugins/uZX/aychip/EnvelopeShape.h`: the eight shapes the envelope can have, where the chip's register has sixteen values for them. They come in pairs, a shape that starts rising and the same one starting to fall: triangle and sawtooth first, then the two that play once. `toRegister` gives the register value the mixer sends on.

| Controller 70 | Shape | | Register |
| --- | --- | --- | --- |
| 0–15 | Triangle | `/\/\` | 14 |
| 16–31 | Triangle, falling first | `\/\/` | 10 |
| 32–47 | Sawtooth | `////` | 12 |
| 48–63 | Sawtooth, falling | `\\\\` | 8 |
| 64–79 | Attack | `/‾‾‾` | 13 |
| 80–95 | Decay | `\___` | 9, and 0 to 3 |
| 96–111 | Attack, then silent | `/___` | 15, and 4 to 7 |
| 112–127 | Decay, then full | `\‾‾‾` | 11 |

**PSG wire, mixer or PSG clip to chip.** Implemented as `PsgCC` in `src/models/PsgCC.*`: one table that `PsgParamsMidiWriter` (PSG clips), the mapper and `AYChipPlugin` all use. Channels count from the chip's base channel. Unlike the note wire, a controller here carries the register's own value: volume 0 to 15, noise period 0 to 31, shape 0 to 15, a switch 0 or 1 (on for any value but 0). A PSG clip replays a file's register writes exactly, a device writes what it receives, and knobs reach the chip through the mixer, not this wire.

| Parameter | Channel | Controller |
| --- | --- | --- |
| Volume A, B, C | 0, 1, 2 | 7 |
| Tone period A, B, C | 0, 1, 2 | 20 (7 high bits) and 52 (7 low bits) |
| Tone on A, B, C | 0, 1, 2 | 80 |
| Noise on A, B, C | 0, 1, 2 | 81 |
| Envelope on A, B, C | 0, 1, 2 | 82 |
| Retrigger tone A, B, C | 0, 1, 2 | 83 |
| Noise period | 3 | 2 |
| Envelope period | 3 | 21 (2 top bits), then 20 and 52 |
| Envelope shape | 3 | 70 |
| Restart envelope | 3 | 83, pressed (1) and released (0) |

A period is split over a coarse controller and its fine pair, 7 bits each, which holds a tone period's 12 bits. The envelope period has 16: its two top bits go in controller 21, so a period under 16384 has the coarse and fine parts it always had, and a stream that never sends controller 21 reads as before. The three go out in that order, the fine part last, so a device that writes a register as each message arrives can wait for the fine part and write the period once. No single MIDI message holds 16 bits: a controller pair, the pitch wheel and an NRPN all stop at 14, and a system exclusive message is not restated by a host when play starts inside a clip.

The envelope shape and its restart are two things in every part of the pipeline but the chip's registers: on the note wire, in a `PsgPatch`, in a PSG clip's frames, and on this wire. Controller 70 is the shape, a value to hold; controller 83 is the retrigger, an event. The reason is that a value gets stated again where nothing happened: a clip's first frame restates everything the chip should hold, a clip that starts part-way through its frames (trimmed at its start, or the later part of a split) restates what the frames before it left, and Tracktion resends the last value of each controller when play starts inside a clip. The chip writes the shape register on a retrigger, or when controller 70 brings another shape than it holds, which cannot sound without a write; the same shape again does nothing. The retrigger is released at once, so the value a host sends again is the released one. Importing a PSG file sets the retrigger on every write of the shape register.

`PsgEncoder` sends a retrigger each time it is set, and everything else only when it differs from what was sent. `PsgDecoder` reads the wire as a chip does, a frame of controllers decoded over the registers the chip holds; the AY chip uses it, and so does `PsgRecorder` in `src/models/PsgRecorder.*`, which keeps the registers written in each 50 Hz frame as a PSG file has them. **Track → Render to PSG** plays a track offline with a recorder on its mixer and writes the result as a `.psg` file and a PSG clip: the same register writes a device on this wire would make.

**Timers.** A chip's timers, TAYM's timed register writes between frames, travel on this wire too: a channel each after the chip's four, with the same split between values that hold and a start that is an event, and their lanes and actions as a bank chosen by number on the fourth channel. [timer-fx.md](timer-fx.md) has the controllers and what the chip does with them. Only a clip opened from a TAYM writes them today.

Both wires use a track's sixteen channels, on either side of the mixer: it reads notes on all sixteen, passes nothing with a channel on, and writes the PSG wire on four channels from its base channel. A PSG clip before a mixer on the same track is therefore read as notes. The two wires never share a stretch of a track, so the PSG wire stays on channels 1 to 4 by default, for the chip, the mixer and PSG clips alike.

## Where the stages run

> **Only the first row exists.** The two hardware rows are a goal: live performance on a real chip. No firmware is written.

![The same four stages in µZX, with a real chip, and standalone on an Arduino](../images/midi-ay-pipeline/where-it-runs.svg)

- **In µZX.** Every stage is a plugin on the computer, and the chip is emulated.
- **A real chip.** µZX runs the instrument, the effects and the mixer, and the PSG wire leaves the computer for a real AY: in a ZX Spectrum, or on an Arduino. The device needs a decoder and register writes, nothing else.
- **Standalone.** An Arduino with an AY runs the instrument, the effects and the mixer as well, ported to C, so a MIDI keyboard plays the chip with no computer. Inside one program the wires between the stages are not needed: one decoder reads the keyboard, the frame functions are called in a row, and the result is written to the registers.

What this asks of the design:

- **The PSG wire becomes a protocol** that firmware implements, so the wire changes in [Order of work](#order-of-work) come before any firmware.
- **The link to the device is a transport, and need not be MIDI.** Over a MIDI port the device decodes `PsgCC`. A 5-pin MIDI cable carries about 62 bytes per 50 Hz frame, 20 controller messages of three bytes; a whole frame after a reset is up to 25 and spills into the next frame, while an ordinary frame, where `PsgEncoder` sends only what changed, is far smaller. Over a serial link that only this pipeline uses (RS-232 into a Spectrum, USB serial into an Arduino) a second codec for the same value is the better fit: the registers that changed, as a mask and their bytes, 16 bytes a frame at most, where a shape in the mask is a write and needs no restart message. That fits 9600 baud (19 bytes per frame) and takes a few instructions to decode on a Z80. It is one more encoder in the bookkeeping row; the logic does not change.
- **Stage logic stays free of MIDI state**, so it ports in shape. The codecs take JUCE and Tracktion message types today and would be written again over raw bytes. No portability layer is built before a port starts.

## What exists today

| Target role | Today | Gap |
| --- | --- | --- |
| Chip instrument | `ChipInstrumentPlugin`, `ChipInstrumentVoice` on `MPEInstrumentFx` | No timbre parameters of its own; fixed 50 Hz (`noteFrameRate`) |
| Chip effects | None | |
| Chip mixer | `NotesToPsgPlugin`, `NotesToPsgMapper`, `ChipMixer` | It follows the chip's stored stereo layout, not an automated one |
| PSG codec | `PsgCC`, `PsgEncoder` | None |
| AY chip | `AYChipPlugin`, `ChipTimers` | None: it decodes each frame's controllers over the registers the chip holds; its timers hold their own state until play jumps |
| Tuning preview | `MultitrackMidiPreview` | None: it plays note clips with timbre controllers into the mixer, one of them a voice that only drives the envelope |

## Order of work

Refactor first, then change behaviour.

1. Chip effects.
