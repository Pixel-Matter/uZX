# Strudel → Studio over MIDI

**Status: proposal, not implemented.** Play the AY chip in Studio live from [Strudel](https://strudel.cc) in the browser, through a virtual MIDI port. Strudel's `.midi()` output and Studio's track input already make this work with raw note and controller numbers. The proposal adds a small JS module that names the controls MIDI to PSG reads, a user-guide section, and, as a separate step, Studio following Strudel's MIDI clock. It complements [krendel-wasm.md](krendel-wasm.md), which runs the chip inside the browser; this one keeps the chip in Studio.

## Prior art

MAP Audio's PAM sampler does this with [mapuzu](https://github.com/MAP-Audio/mapuzu): a module loaded into Strudel with `await import(...)` adds `.pam()`, named `knob()`, `variation()` and `pamclock()`, all sent as Web MIDI to a virtual port (IAC on macOS, loopMIDI on Windows, snd-virmidi on Linux). A CLI writes a CC mapping file that PAM imports. There is no WebSocket and no native code in the browser. mapuzu is AGPL-3.0: take the idea, not the code.

## What works today

| Step | Where | State |
| --- | --- | --- |
| Pattern → MIDI notes, CC, channel | Strudel `.midi(port)`, `.midichan()`, `ccn`/`ccv` | Upstream Strudel |
| Virtual port | OS (IAC, loopMIDI, snd-virmidi) | User setup |
| Port enabled in Studio | Audio Settings (`Cmd+,`), MIDI inputs | Exists |
| Port → track | Track header **I** menu, monitoring auto/on (`TrackComponents.cpp`) | Exists |
| Notes and CC → chip | MIDI to PSG → AY chip on the track | Exists |
| AY chip knobs from CC | Right-click → MIDI mapping (learn) | Exists |

The live input keeps every MIDI channel: `MidiInputDeviceNode` rewrites the channel only when the device has a `channelToUse`, and Studio never sets one. So MPE-style playing (a note per channel 2–16, priority by channel) arrives as sent. Incoming messages are timestamped on arrival unless the engine behaviour says the driver's timing is used; Web MIDI already sends each message at its scheduled time, so the remaining jitter is the browser's and the OS driver's.

## The Strudel module

A plain ES module, `uzx.mjs`, kept in this repository and served from a CDN that mirrors GitHub, loaded with `await import('<url>/uzx.mjs')`. No build step and no C++ change.

It names the controllers MIDI to PSG reads, as listed in the user guide under **Track Devices / Plugins**. The guide is the single source of their numbers and meaning; the module copies them and a test checks the two agree.

| Strudel control | Controller | Meaning in MIDI to PSG |
| --- | --- | --- |
| `tone` | 80 | Switch: tone on from 64 |
| `noise` | 81 | Switch: noise on |
| `env` | 82 | Switch: envelope on |
| `noiseperiod` | 2 | 0 highest noise … 127 lowest |
| `shape` | 70 | Envelope shape, eight bands of 16; the module accepts shape names |
| `retrig` | 83 | Restart the envelope from 64 |
| `buzz` | 85 | Notes on this channel drive the envelope |
| `prio` | 86 | Note priority on this channel |
| `chpan` | 10 | Preferred chip channel by place in the AY chip's layout |

Its surface:

- `uzxsetup({ port, channels })`: picks the Web MIDI output once and registers the names above, through Strudel's MIDI controller map if the pinned revision has one, otherwise as `ccn`/`ccv` pairs.
- `.uzx()`: `.midi(port)` with that port, so patterns read like `note("c3 [eb3 g3]").tone(1).env(1).shape("saw").uzx()`.
- `uzxclock()`: the clock pattern for the step below, a thin wrapper over Strudel's `midicmd`.

Booleans become 0/127. Values are 7-bit, as everywhere on MIDI. The module carries no state about the chip; Studio stays the one place that decides what sounds.

The names, the `midimap` hook and `midicmd` are checked against one pinned Strudel release, recorded in the module's header with the date it was tested.

## Following Strudel's clock

Checked in the Tracktion source (`playback/devices/tracktion_PhysicalMidiInputDevice.cpp`): a MIDI input can chase **MTC** (`MidiTimecodeReader`, with drift correction through `EditPlaybackContext::setSpeedCompensation`) and obey **MMC** (`setReadingMidiTimecode`, `setAcceptingMMC`). Studio calls neither. Tracktion **does not read MIDI clock**: `0xF8`, Start, Continue, Stop and Song Position reach the track instances as ordinary messages and nothing acts on them. `MidiClockGenerator` exists only on outputs. Strudel's `midicmd` sends clock and Start/Stop (to be confirmed on the pinned release); nothing in Strudel sends MTC or MMC, so following it needs a reader of our own.

Without one, live playing still works: monitoring sounds the chip whatever the transport does. A clock is needed only to record in time with the edit, or to line up Strudel with clips already on the timeline.

### Clock reader

A `MidiClockFollower` in `controllers/`, one per edit, attached as a `PhysicalMidiInputDevice::Listener` to the input chosen as the clock source. The listener is called on the MIDI thread for every message, before Tracktion's filtering, so it sees the realtime messages.

| Message | Action |
| --- | --- |
| Start `0xFA` | Position to the edit start (or the loop start), then play on the next clock tick |
| Continue `0xFB` | Play from the current position |
| Stop `0xFC` | Stop, position kept |
| Song Position `0xF2` | Position in sixteenths, while stopped |
| Clock `0xF8` | Measure tempo: 24 per beat, smoothed over a beat or more |

Transport calls go to the message thread through `MessageManager::callAsync`, as Tracktion's own MTC reader posts them. The follower is off by default; turning it on is a per-edit choice of input, saved in the edit as an attribute older apps can ignore, so it needs no format bump.

### Tempo

Following the clock's tempo is the hard part, because the tempo is part of the project here. Changing it moves PSG frames in wall-clock time, or rescales `framesPerBeat` when timing is preserved ([psg-timing.md](psg-timing.md)), and lands in undo and in the saved file. Writing a measured tempo continuously would also fill the tempo map with jitter.

Proposed order:

1. **Transport only.** Start, Stop, Continue and Song Position; the user sets the same BPM in both apps. Studio shows the measured clock tempo next to its own and marks a mismatch. Drift between the two is corrected the way the MTC reader does it, by speed compensation, not by tempo writes.
2. **Tempo on Start, if asked.** On Start, set the edit's tempo once, rounded to 0.01 BPM, through `PsgTiming::setTempoBpmRetimingFrames` so PSG clips keep their timing; no changes while playing.

Continuous tempo following is out of scope.

## Order of work and acceptance

1. **Guide.** A **Strudel** section in `docs/user-guide.md`: the virtual port per OS, enabling it in Audio Settings, routing it with **I**, monitoring on, a three-line example with raw `ccn`/`ccv`. This works today.
2. **Module.** `uzx.mjs` with the table above, its pinned Strudel release, and a check that its controller numbers match the guide. Accepted when an example with a bass on channel 1 and a three-note chord on channels 2–4 plays on one chip with the expected channel stealing, and `shape`/`retrig` change the envelope audibly.
3. **Clock, transport only.** `MidiClockFollower` with unit tests feeding message sequences (Start, ticks at 120 BPM, Stop, SPP) into it and checking the transport calls and measured tempo. Accepted when Strudel's Start/Stop starts and stops Studio at the edit start, and a recording of the live notes lands on the beats.
4. **Clock, tempo on Start.** Only after step 3 is used in practice.

## Open questions

- Where to serve `uzx.mjs`: GitHub Pages of this repository, or a CDN path to a tagged file. A tag keeps old songs working when the module changes.
- Whether the clock source should be a global setting rather than per edit, since the virtual port belongs to the machine.
