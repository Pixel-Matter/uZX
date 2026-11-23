# Strudel → Studio over MIDI

**Status: proposal, not implemented.** Play the AY chip in Studio live from [Strudel](https://strudel.cc) in the browser, through a virtual MIDI port. Strudel's `.midi()` output and Studio's track input already make this work with raw note and controller numbers. The proposal adds a small JS module that names the controls MIDI to PSG reads, a user-guide section, and, as a separate step, Studio following Strudel's MIDI clock. It complements [Krendel](https://pixelmatter.org/krendel/), which runs the chip inside the browser; this one keeps the chip in Studio.

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

**Prototype:** [`tools/krendel/uzx.mjs`](../../tools/krendel/uzx.mjs), with its tests and how to load it in [`tools/krendel/README.md`](../../tools/krendel/README.md). A plain ES module loaded in Strudel with `await import('<url>/uzx.mjs')`; no build step and no C++ change.

It reads Strudel's `registerControl`, `midimaps`, `reify` and `Pattern` from `globalThis`, where the REPL puts every export of its packages before user code runs. It registers one control per controller MIDI to PSG reads and a midimap `uzx` that sends them as CC; `.uzx(port)` is `.midimap('uzx').midi(port)`. Strudel's `.midi()` sends a hap's mapped controllers before its note, on the hap's `midichan`, at the same Web MIDI timestamp.

The numbers are `NoteCC` in `src/plugins/uZX/midi_effects/NoteFrame.h`; the module's test reads that header and fails when they differ. Values go through `stepToController` from the same header, so each lands in the band `controllerToStep` reads back.

| Control | Controller | Value |
| --- | --- | --- |
| `aytone` | 80 | 1 or 0 |
| `aynoise` | 81 | 1 or 0 |
| `ayenv` | 82 | 1 or 0 |
| `ayretrig` | 83 | 1 restarts the envelope |
| `aybuzz` | 85 | 1: the notes drive the envelope |
| `ayshape` | 70 | `tri`, `trifall`, `saw`, `sawfall`, `attack`, `decay`, `attackcut`, `decayhold`, or 0–7, in the user guide's order |
| `aynoiseperiod` | 2 | 0 highest noise … 31 lowest |
| `ayprio` | 86 | 0–127 |
| `pan` (Strudel's own) | 10 | 0 left … 1 right: the chip channel at that place in the AY chip's layout |

The `ay` prefix is needed: Strudel already has `noise`, `shape`, `env` and `retrig`, with other meanings. The module carries no state about the chip; Studio stays the one place that decides what sounds.

Tested in Node with the npm releases pinned in `tools/krendel/package.json` (`@strudel/core` 1.2.6, `@strudel/mini` 1.2.6, `@strudel/midi` 1.3.0). Not yet played through Studio.

## Following Strudel's clock

Checked in the Tracktion source (`playback/devices/tracktion_PhysicalMidiInputDevice.cpp`): a MIDI input can chase **MTC** (`MidiTimecodeReader`, with drift correction through `EditPlaybackContext::setSpeedCompensation`) and obey **MMC** (`setReadingMidiTimecode`, `setAcceptingMMC`). Studio calls neither. Tracktion **does not read MIDI clock**: `0xF8`, Start, Continue, Stop and Song Position reach the track instances as ordinary messages and nothing acts on them. `MidiClockGenerator` exists only on outputs. Strudel's `midicmd` sends `clock`, `start`, `stop` and `continue`, and `.midi()` sends Start by itself for an event at cycle 0; nothing in Strudel sends MTC or MMC, so following it needs a reader of our own.

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
2. **Module.** Prototype done, with Node tests. Accepted when an example with a bass on channel 1 and a three-note chord on channels 2–4 plays on one chip with the expected channel stealing, and `ayshape`/`ayretrig` change the envelope audibly.
3. **Clock, transport only.** `MidiClockFollower` with unit tests feeding message sequences (Start, ticks at 120 BPM, Stop, SPP) into it and checking the transport calls and measured tempo. Accepted when Strudel's Start/Stop starts and stops Studio at the edit start, and a recording of the live notes lands on the beats.
4. **Clock, tempo on Start.** Only after step 3 is used in practice.

## Open questions

- Where to serve `uzx.mjs`: GitHub Pages of this repository, or a CDN path to a tagged file. A tag keeps old songs working when the module changes.
- Whether the clock source should be a global setting rather than per edit, since the virtual port belongs to the machine.
