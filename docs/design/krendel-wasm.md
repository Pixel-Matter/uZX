# A Strudel live editor for AY/YM

**Status: shared voice allocation implemented. Browser instrument proposed.** Build a browser instrument that shares its musical model with native uZX. A musician edits a Strudel pattern while it plays and can follow a note from a line of code to the physical channel that sounds it. The first playable example uses one chip, two named instruments and a scrolling piano roll.

## Ownership of the signal path

| Component | Input → output | Responsibility |
| --- | --- | --- |
| Strudel editor, pattern evaluator and scheduler | Source → timed musical events | Live evaluation, cycles, rests, chords, duration, source highlighting and lookahead |
| Small browser adapter | Scheduled events → timestamped commands | Resolve instrument names, give each note occurrence an ID, batch commands to the worklet and connect the views |
| Shared C++ instrument and mixer | Note gates → instrument voices per frame → PSG changes | Instrument state, tuning, channel allocation and ownership of noise and envelope |
| AudioWorklet with C++ and C compiled by Emscripten | Commands → stereo PCM and trace | Run one engine per chip, apply PSG writes to the vendored Ayumi and render into Web Audio output buffers |
| Browser display | Scheduled notes and rendered trace → synchronized lanes | Draw the score ahead of the playhead and actual voice and chip history behind it |

Keep the editor's pattern language. For example, bind the sound name in `note("<c2 g1>").s("bass")` to a C++ instrument preset. Use a second pitched preset for a chord. Both address the same engine instance. Pin compatible Strudel packages together and check the scheduler hook, gate duration, pitch units and source-location fields against that revision. The upstream scheduler currently exposes `onSchedule(hap, time, cps)`. Route these events to the chip exactly once. Use Strudel's existing editor and piano-roll facilities where their public interfaces fit. [Strudel packages](https://codeberg.org/uzu/strudel/src/branch/main/packages)

## Extracting the native model

Start at [NoteFrame.h](../../src/plugins/uZX/midi_effects/NoteFrame.h) and [NotesToPsgMapper.h](../../src/plugins/uZX/notes_to_psg/NotesToPsgMapper.h). The former contains both the bounded voice frame and MIDI conventions. The latter declares both the mixer and its plugin adapter. Extract the value types, the allocation and rendering functions, and the tuning calculations they actually need into a standalone C++ target buildable with a native compiler and Emscripten. Keep MIDI/MPE codecs, project state and plugin lifecycle in their existing native adapters. Pass clock, tuning and channel pans explicitly. Make the native mapper the first consumer. Use [the existing mixer tests](../../src/plugins/uZX/notes_to_psg/NotesToPsgMapper.test.cpp) as the behavioral baseline.

Preserve frame order, fractional-semitone pitch, priority and stable channel placement. The highest priority wins contention, with the later voice in a frame breaking a tie. An envelope driver can own the generator without consuming a tone channel. Noise ownership is selected among allocated voices that request noise. Preserve envelope shape as held state and restart as an event, including a second write of the same shape when a restart is requested. Capture owners when the mixer chooses them. Register values alone cannot identify a contributing voice. [MIDI to AY pipeline](midi-ay-pipeline.md) · [Tuning](tuning.md)

For the prototype, implement two fixed C++ presets with distinct audible behavior, for example a sustained tone bass and a short noise-and-tone pluck. Each takes note-on, note-off and velocity and emits voices onto the same frame grid. Reuse the shaping logic that can run independently of the desktop plugin. Named preset timbre is proposed behavior: today's native instrument takes timbre from channel controllers. Preserve existing desktop projects when connecting the shared model. [Instrument implementation](../../src/plugins/uZX/instrument/ChipInstrumentVoice.cpp)

### JUCE dependencies in the web build

The uZX fork of JUCE 8.0.17 at `aaf33ede` recognizes WASM. A probe using Emscripten 4.0.10 needed two temporary build fixes for `juce_core`: supply the Emscripten header declaring `emscripten_get_now()` and provide entries for WASM in the native thread-priority table. The probe used the existing zero-valued Linux mappings. With those changes, Node.js ran a `juce::String` example and a `juce_audio_basics` example that recovered MIDI note 60 at sample offset 123 from a `MidiBuffer`. The vendored Ayumi passed all three output tests under WASM as shipped.

Keep these JUCE types where the shared model benefits from them. Before depending on further APIs, exercise the actual path in the worklet. Thread scheduling, the complete mixer and browser audio playback still need verification. [Pinned JUCE source](https://github.com/Pixel-Matter/JUCE/tree/aaf33ede1b159fac7d6ed93cd52e2e180128583b)

## Scheduling and rendering contract

- Instantiate WASM and allocate command, audio and trace buffers before playback. Expose a small C ABI for configuring the engine, submitting batches, rendering a block, draining trace and resetting. Keep C++ classes and allocation details inside the module. Build the vendored [Ayumi](../../third_party/ayumi/) into the same module. Keep the web build optional for desktop contributors.
- Express command times as absolute sample frames on the AudioContext clock. Resolve the chosen Strudel hook's time origin once at the adapter boundary. Carry generation, occurrence ID, instrument ID, gate, pitch and velocity. Preserve ordered events at equal timestamps. Overlapping occurrences of the same pitch need independent releases. Map their IDs to distinct live voice slots while preserving the existing native channel/key identity convention.
- Render using the actual sample rate and output block length. Consume scheduled events and advance the instrument on the shared frame grid (initially 50 Hz). On each tick, apply gates due through that tick in order, build the frame, mix it, apply the PSG changes and render samples until the next boundary. Carry fractional tick phase across blocks. Keep a sub-tick gate audible for one tick in the web instrument. Cover this proposed articulation explicitly in the gate tests.
- On a successful live edit, switch generations at the next cycle boundary. Discard queued old-generation events at and after that sample, release old voices and start the replacement. A failed evaluation leaves the playing generation active. Stop invalidates queued events, clears voices and silences the chip. Repeated play/stop starts with fresh mixer and encoder state.
- Use bounded queues and preallocated memory in the audio callback. If command delivery falls behind, apply late commands at the next available tick and count them. On command overflow, silence and reset the affected generation so a lost release cannot hang a note. Trace overflow can drop old display records and mark the gap. A hidden or slow view must never stall audio.

## Scrolling notes and chip ownership

Give the piano roll and chip lanes one horizontal time axis and one playhead anchored to the audio output clock. Color by instrument throughout. Draw scheduled pitch and duration ahead of the cursor. Behind it, draw fractional pitch and level from the rendered note frames, including voices that lost allocation. Use separate styling for an unallocated voice so a held score note remains legible when it loses its channel. Keep the roll usable with the chip lanes collapsed.

The trace carries generation, sample frame, voice occurrence, instrument, pitch and level, plus the owners of A/B/C, noise and envelope and any envelope restart. Record silence and owner changes even when two owners yield identical register values. The UI can associate an occurrence with its Strudel source span when available. Transfer batches from the worklet at display cadence and draw from bounded history. Observe the trace at the output playhead so buffered future audio does not appear as already heard. [Native PSG visualization conventions](../psg-visualization.md)

## Running the shared allocator

The desktop mapper uses the same [allocator](../../src/chip/ChipAllocator.h) as the standalone target. It accepts one bounded note frame and returns the voices assigned to A/B/C, noise and envelope. The result refers to the supplied frame. Consume or copy the ownership information before modifying or discarding the frame. Tuning and PSG rendering still run in the desktop mapper.

From the repository root, run the allocation fixture with a C++23 compiler, then with an activated Emscripten SDK and Node.js:

```sh
cmake -S src/chip -B build/chip-native
cmake --build build/chip-native --parallel
ctest --test-dir build/chip-native --output-on-failure

emcmake cmake -S src/chip -B build/chip-wasm
cmake --build build/chip-wasm --parallel
ctest --test-dir build/chip-wasm --output-on-failure
```

## Implementation order and acceptance

1. **Extract and verify the C++ target.** Run the existing mapper and instrument tests before and after extraction. Build the core target independently of Tracktion, the JUCE GUI modules and the generated JuceHeader.h. Include individual JUCE module headers where needed. Feed a deterministic sequence of note frames through the existing native behavior and save the expected ordered register writes, including retriggers, as a fixture.
2. **Render that fixture in WASM.** A native harness and a WASM harness must produce the same ordered (sample frame, register, value) trace for the same chip clock, tuning, frame rate and sample rate. Include a tie between four voices, a noise conflict, an independent envelope driver, a repeated shape restart and silence after reset. Vary block length and test at 44.1 and 48 kHz. Compare traces between builds at each rate.
3. **Connect the live editor.** Play a bass note under a three-note pluck chord to force contention. Verify audible output on both AY and YM settings. Replace the pattern during sustain, introduce a syntax error, stop, then restart. Check for stale events, stuck notes and doubled playback. Include the runnable example and the exact native, WASM and browser verification commands with the implementation.
4. **Show what happened.** In that same example, align the scrolling notes with all three chip channels and the two generator owners. Verify one voice loses allocation during the chord while its musical duration continues. Compare the displayed owner transition and envelope restart with the captured engine trace at the same sample frame. Repeat with the view hidden to check that playback continues.

The first delivery ends with that playable example. A preset editor, module import, multiple chips and hardware output are follow-up work. Record the pinned Strudel revision and applicable package licenses with the web build before distributing it. [uZX license](../../LICENCE.md) · [Ayumi license](../../third_party/ayumi/LICENSE)
