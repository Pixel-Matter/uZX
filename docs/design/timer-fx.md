# Timers: timed AY writes between frames

How the AY chip plays TAYM's timer effects: register writes many times within a frame, which make pulse-width modulation, sync-buzzer and samples. Code: `src/plugins/uZX/aychip/ChipTimers.*`, `src/models/TimerCC.*`, `src/models/TimerBank.*`, `src/models/ClipTimers.*`, `src/formats/taym/`, and the format's own C engine in `third_party/taym/c`.

[TAYM](https://github.com/ruguevara/taym) is an interchange format: a chip's frames as a PSG, and timers that own some of its registers for a while and write them from lanes of values at their own rate. Its repository holds the specification and the engine used here; section numbers below are the specification's.

## Where it sits

```
PSG wire ─► PsgDecoder ───► the frame's registers ─┐
        └─► TimerDecoder ─► the frame's commands ──┼─► ChipTimers ─► register writes, each at its sample ─► chip
bank, by number ◄── TimerBankRegistry ◄── clip ────┘
```

A frame brings the chip its ordinary register writes, the background, and at most one command for each timer: start, modulate or stop. `AYChipPlugin` applies a frame at the sample it is rendered up to; `ChipTimers` runs the commands, then decides what the background's writes do:

- A running timer **owns** the registers its actions name. The background's write to an owned register is held back.
- A register a timer takes has the chip's value saved; one it lets go of gets the held write, or the saved value.

Between frames `AYChipPlugin::renderTo` asks for the timers' writes that fall in the samples it is about to render and applies each at its own sample. A mute is applied again after a write to the mixer or a volume, since a mute holds over the registers as written.

The engine itself is TAYM's: `taym_timers.c` is chip-agnostic and reports raw target values; `taym_ay.c` makes AY register writes of them (bytes, the envelope shape's four bits, a sample amplitude combined with a volume through the chip's curve). `ChipTimers` wraps both. Nothing in it allocates or locks.

## Chip time

`ChipTime` counts the chip's clock cycles against rendered samples, both from the plugin's last reset, in whole numbers:

- a frame at sample `s` is at cycle `s · clock / rate`, rounded down;
- a write at cycle `c` sounds from sample `c · rate / clock`, rounded up.

So a timer started in a frame writes first at that frame's sample, and its later writes are whole intervals on in cycles, however the host cuts time into blocks. The tests pin both: a timer's writes sound bit for bit as frames at the same samples do, and one block, ragged blocks and 32-sample blocks render the same audio.

A sample is some forty cycles, so a timer can expire in a frame's sample but before the frame's cycle. That expiry is written first (`AYChipPlugin::writeTimersBefore`), as it comes first in the chip's time: it takes its next interval from the values it had, not from those the frame's command brings. Applied the other way round, a timer drifts a few samples off at every such frame.

To hear a whole tune as the engine plays it, or to compare it with TAYM's Python reference render: `UZX_RENDER_TAYM_TO=<file.wav> uZXTests TaymImport` renders the repository's example tune.

A timer's rate is in the chip's clock, so the chip's clock setting changes its pitch as it would on hardware; a rate given in Hz stays in Hz. Changing the clock stops the timers.

The chip's output filter delays every write alike by about a dozen samples. That is the emulator's, and applies to frames as well.

## On the wire

Timers travel on the PSG wire ([midi-ay-pipeline.md](midi-ay-pipeline.md)), a MIDI channel each after the chip's four: twelve timers for a chip on base channel 1. `TimerCC` in `src/models/TimerCC.*` is the one description of it, with `TimerEncoder` and `TimerDecoder`.

What a timer holds is values, and its start is an event, as with the envelope. A value that changes with no start modulates the running timer, which is TAYM's own split between START and MODULATE (S12).

| A timer's | Controller, on its channel | |
| --- | --- | --- |
| On | 80 | Off stops it |
| Start | 83, pressed and released | Starts it with the values held |
| Base value, 32 bits | 21 (bits 28–31), 18 and 50 (bits 14–27), 20 and 52 (bits 0–13) | Sent whole, 52 last; a base under 16384 has the two parts a tone period has |
| Timer lane | 16 and 48 | The lane's index + 1; 0 for none |
| First action | 17 and 49 | |
| Number of actions | 19 | |

| The chip's | Controller, on its fourth channel | |
| --- | --- | --- |
| Bank | 16 and 48 | 1 to 16383; 0 for none |

The numbers avoid controllers 0 and 32 and the RPN and NRPN ones: Tracktion keeps those for the program change or parameter they belong to and does not state them again when play starts inside a clip. A SysEx message per command was not used for the same reason, and because it allocates on its way through the graph and a VST3 host does not pass it.

**The bank does not travel.** A timer's lanes and actions can be thousands of values. Whatever plays timer commands registers their bank with `TimerBankRegistry` and sends its number; the chip looks it up. A device on a real wire would be sent the bank ahead of time under the same number.

**No chase.** When play jumps or stops the timers stop (`isAllNotesOff`). Tracktion then states each controller's last value again, which has the start released, so a timer that is on with no start since stays silent until its next one. Its phase at the place play landed is not worked out.

`TimerEncoder` states every value at a timer's first start, since the chip may hold another clip's, and after that only what changed.

## In a clip

A TAYM opens as a PSG clip with timers (`ClipTimers`, in the clip's state as a `TIMERS` child):

- **The bank**: a TAYM with the file's chips, timers and tables and nothing else, as bytes. `TimerBank` parses it once; the clip registers it for as long as it lives.
- **The commands**: a `TIMER` for each, with its frame's index in the clip's PSG list. They are on the list's frame grid, so tempo and frame-rate changes move them with the frames, and a clip trimmed at its start plays them from its start on.

`PsgClip::createPlaybackMidiSequence` adds them to the frames' controllers. Commands before the clip's start are left out; a timer running across it is silent until its next start.

A project with such a clip opens in an older µZX, which plays the frames without the timers and keeps the timers when it saves. That is why the edit format version is not raised: raising it would lock every project out of older versions.

## Reading a TAYM

`readTaymModule` in `src/formats/taym/TaymFile.*` gives a `TrackerModule`: each AY chip's frames, the tune's title, clock and chip type, and each chip's timers. It goes through the same import as a PSG file or a tracker module (`Helpers::importPsgFiles`).

A chip's frame data is a standard PSG and is read by `PsgFile`, as a `.psg` file is, with every write kept: a shape written again restarts the envelope. A pause counts four frames for each unit in a PSG, but TAYM's own tools write and read it as one; the track states its frame count, so the reader takes whichever gives that count, fours first.

## What the chip cannot play

A command the chip cannot play is ignored and the rest plays. `ChipTimers::takeFaults` says what was ignored; `AYChipPlugin::getTimerWarnings` keeps it for the interface, and the AY panel shows a `⚠` with the reason:

- the bank chosen for the timers is not registered, or a timer is started with no bank chosen;
- a command names a timer its chip does not have, or a lane or an action its bank does not have.

The warning stays until the chip is reset; one about the bank also ends when a bank is found.

## In the interface

- **In the clip.** `ClipTimers::getSpans` gives what each timer does from one command to its next: the registers it holds, the chip cycles its values and intervals take to come round (`loopCycles`), and its duty. A tone of period `loopCycles / 16` has the same pitch, which is where `paintTimerNotes` in `src/gui/timeline/PsgClipComponent.cpp` draws the note. The clip's pitch range takes the timers in. `formatTimerFrame` writes the frame hover's lines.
- **In the AY panel.** `AYChipPlugin::getTimerChannels` says which channels a timer drives; `AYScopes` draws a stopwatch in the corner of those scopes. `ChannelMuter` has a timers switch per channel, shown while the chip's track has a clip with timers.
- **Muting.** A channel's timers switch mutes its volume and tone period registers (`ChipTimers::setMuted`). A timer still owns a muted register and runs on, so it comes back in step; its writes are dropped, and the frames' writes go to the chip as if no timer had it.

## Limits

- Twelve timers a chip; timer lane and action indexes up to 16383; 127 actions a command. A file past these opens without those timers' effects, and the import says so.
- A timer whose rate drops to zero fires once every 1/50 s, whatever the tune's frame rate.
- Chips other than AY and YM are left out.
- Viz mode and rendered video draw the timers' notes; their scopes have no stopwatch.

## Not built

- **Timer effects as an instrument voice** (sync-buzzer, PWM played from notes): a note starts a timer, its pitch sets the rate through the tuning, the chip mixer writes this wire. Nothing of it exists.
- **Writing a TAYM**: Render to PSG records frames only.
