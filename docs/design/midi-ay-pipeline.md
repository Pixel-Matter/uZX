# MIDI to AY pipeline

> **Status: target design, partly implemented.** The PSG controller wire (`PsgCC`) and the plugins under [What exists today](#what-exists-today) are in the code. `NoteFrame`, the shared decoder and encoder, and the chip mixer are not.

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
| Note | `NoteFrame`: the voices sounding in one frame. A voice has a channel, a pitch in fractional semitones, a level, a priority, and timbre fields (tone, noise and envelope mix, noise pitch, envelope shape) | Note codec: one voice per MIDI channel, MPE | In the shared decoder and encoder |
| PSG | `PsgPatch`: the PSG parameters set in one frame. Today this is `PsgParamFrameData` | `PsgCC` | The chip's registers |

Every note-domain stage has the same shape:

```
MIDI in ─► decoder ─► NoteFrame ─► logic ─► NoteFrame ─► encoder ─► MIDI out
           shared                  the stage's own        shared
```

The decoder and encoder own everything that is about MIDI: held notes and their note-offs, per-note pitch and level, controller values, all-notes-off, transport jumps and resets, frame boundaries, what was last sent, and sending on a block with no input. After a reset or a jump the encoder forgets what it sent, so the next frame goes out whole.

The logic owns only musical state: an envelope's phase, an LFO's phase, an arpeggio's step, an echo's history. Vibrato is `pitch += lfo(frame)`. Its test feeds frames and compares frames.

A plugin is then one decoder, a chain of frame functions, and one encoder, which is why "inside the instrument or a separate effect" stops mattering: the same function fits either.

## The chip mixer

The mixer takes the chip-note stream on all channels and produces one `PsgPatch` per frame:

- **Allocation.** More voices can sound than the chip has channels. Each voice carries a priority, by default its channel number, and the simplest policy is "higher channel number wins". Richer policies come when something needs them.
- **Pitch.** The tuning turns a voice's pitch into a tone period, or an envelope period for an envelope voice, at the clock of the chip the mixer plays on.
- **Timbre.** The voice's timbre fields become the tone, noise and envelope switches, the noise period and the envelope shape.

## The wires

**Note wire, instrument to mixer.** MPE: one voice per MIDI channel, so pitch (pitch bend) and level (pressure) are per note. The instrument spreads a chord over its channel range. That caps the stream at sixteen voices before the mixer; MIDI 2 or an in-process path would replace only the note codec.

**PSG wire, mixer or PSG clip to chip.** Implemented as `PsgCC` in `src/models/PsgCC.*`: one table that `PsgParamsMidiWriter` (PSG clips), the mapper and `AYChipPlugin` all use. Channels count from the chip's base channel.

| Parameter | Channel | Controller |
| --- | --- | --- |
| Volume A, B, C | 0, 1, 2 | 7 |
| Tone period A, B, C | 0, 1, 2 | 20 (7 high bits) and 52 (7 low bits) |
| Tone on A, B, C | 0, 1, 2 | 80 |
| Noise on A, B, C | 0, 1, 2 | 81 |
| Envelope on A, B, C | 0, 1, 2 | 82 |
| Retrigger tone A, B, C | 0, 1, 2 | 83 |
| Noise period | 3 | 2 |
| Envelope period | 3 | 20 and 52 |
| Envelope shape | 3 | 70 |
| Retrigger envelope | 3 | 83 |

Both wires share a track's sixteen channels. Routing them apart is the user's business, as in any DAW; the safer default is the PSG wire on the last four channels (13–16), which leaves the low channels to notes. Today both the chip and MIDI to PSG default to base channel 1.

## What exists today

| Target role | Today | Gap |
| --- | --- | --- |
| Chip instrument | `ChipInstrumentPlugin`, `ChipInstrumentVoice` on `MPEInstrumentFx` | Sends pitch bend on the note's own channel rather than one channel per voice; fixed 50 Hz; tracks its own MIDI state |
| Chip effects | None | |
| Chip mixer | `NotesToPsgPlugin`, `NotesToPsgMapper` | Treats MIDI channel n as AY channel n, one note per channel, no allocation; tracks its own MIDI state; passes controllers through untouched |
| PSG codec | `PsgCC` | An envelope period above 14 bits does not fit two controllers; switches are sent as 1 by a PSG clip and 127 by the mapper; volume is the raw 0–15 |
| AY chip | `AYChipPlugin` | None: it decodes each frame's controllers over the registers the chip holds, and keeps nothing else |
| Tuning preview | `MultitrackMidiPreview` | Writes PSG switches into note clips by hand and relies on the mapper passing them through |

## Order of work

Refactor first, then change behaviour.

1. `NoteFrame` with the shared decoder and encoder; the instrument moves onto them with the same behaviour.
2. The mapper becomes the mixer with the trivial allocation, channel n to AY channel n, same behaviour.
3. Voice allocation and priority; MPE output from the instrument; the PSG wire on the last four channels by default.
4. Wire changes: a full 16-bit envelope period, one meaning for switch and volume values.
