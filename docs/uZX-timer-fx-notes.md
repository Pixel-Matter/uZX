# Timer FX (TAYM-style timed AY writes) in uZX: design notes

Notes from the pyayay `render` / absolute-timeline work (pyayay branch
`feature/render-frame`, files `src/wrapper.cpp`, `src/aychip.{h,cpp}`,
`tests/test_render.py`). uZX's `src/plugins/uZX/aychip/aychip.h` is the same
code lineage (`uZX::Chip`, `AYInterface`), so the core ports almost line for line.

## Core: writes on an absolute AY-cycle timeline

- A write is `(t, reg, value)`, `t` = absolute AY clock cycle (uint64).
- It is applied right before absolute sample `ceil(t * sample_rate / clock)`.
  Writes at the same sample apply in order (last wins; every R13 write restarts the envelope).
- A block of `n` samples starting at absolute sample `S` takes exactly the writes with
  `t <= floor((S + n) * clock / sample_rate)`. A write landing exactly on `S + n`
  takes effect at the start of the next block.
- Exact integer math, no 128-bit ints (MSVC has none): split into quotient and remainder,
  `a * mul / div = (a / div) * mul + (a % div) * mul / div` (round the second term up for ceil).
  Needs `clock` and `sample_rate` as integers below 2^32: round `chipClock * MHz` to whole Hz.
- Property this gives: block boundaries do not affect output. One block, 50 Hz frames and
  random host block sizes render identical samples, so timer phase never glitches at frame
  boundaries even when a frame is not a whole number of cycles (1773447 Hz @ 44100 Hz =
  35468.94 cycles per 882-sample frame).
- Copy from pyayay `src/wrapper.cpp`: `mulDiv`, `sampleAt`, `cycleAt`, `renderTimed`.

## What changes in a plugin (AYChipPlugin)

1. **Absolute time = host transport, not a free counter.** Each block, take the start sample
   from the playhead (`AudioPlayHead::PositionInfo::getTimeInSamples()` / Tracktion edit time
   in samples). If it is not previous start + previous block size, it is a jump (loop, seek,
   scrub, stop/start): chase timer state at the new position, like CC chasing. TAYM timers are
   deterministic from the last START/MODULATE plus intervals, so the next expiry after `t` can
   be computed directly. Ayumi's own state (tone counters, envelope phase) is not seekable and
   does not need to be: re-assert registers and continue. Bonus: offline bounce == realtime play.
2. **Generate timer writes inside the chip plugin.** MIDI/automation carry the slow part
   (start/stop, rate, targets, duty, lanes); the plugin's timer engine emits
   `(cycle, reg, value)` for the current block. A 5 kHz PWM is 10k writes/s, unusable as MIDI.
3. **One merged, sorted event list per block** (MIDI-derived register changes + timer writes),
   then the segment loop:

   ```cpp
   // block covers absolute samples [S, S + n)
   const uint64_t endCycle = cycleAt(S + n);
   events.clear();                                   // preallocated
   for (auto& m : midi) events.push({cycleOf(m), FromMidi, ...});
   timers.emitUntil(endCycle, events);
   events.sortStable();                              // by cycle; define who wins at equal cycles
   size_t pos = 0;
   for (auto& e : events) {
       const size_t k = sampleAt(e.cycle) - S;       // 0..n
       if (k > pos) { render(pos, k); pos = k; }
       apply(e);
   }
   if (pos < n) render(pos, n);
   ```
4. **One placement rule.** `applyToBuffer` currently uses `roundToInt(timestamp * sampleRate)`
   (nearest) for MIDI. Mixing that with ceil for timer writes can reorder a MIDI write and a
   timer write less than a sample apart. Convert MIDI timestamps to cycles and use ceil for both.
5. **Ownership (TAYM S13.1).** An active timer owns its target registers; background
   (PSG/MIDI `registersFrame`) skips owned regs and re-asserts them when the timer releases.
   Push background registers only when changed: re-writing R8/R11/R12/R13 resets the
   envelope mid-buzz. `channelMuter.applyToChip` must run after every write segment or be
   folded into `apply`.
6. **Render variants.** Mono / stereo / separate / stereo+separate: make the segment loop
   generic over a "render [a, b)" lambda so timing logic exists once.
7. **Realtime safety.** No allocation on the audio thread: fixed-capacity event and scratch
   buffers sized in `initialise()`. If capacity is hit, collapse writes at the same sample
   (only the last per register matters; repeated R13 restarts in one sample sound like one).
8. Variable host block sizes are fine; host sample rates are integers in practice. At 96 kHz
   the sample-grid jitter halves (1 sample = ~18 AY cycles at 1.77 MHz).

Accuracy note: writes land on the sample grid (~40 AY cycles at 44.1 kHz). This matches
taym's current engine. If it ever becomes audible, apply writes inside `ayumi_process` at
Ayumi's internal tick (clock/8) instead; that needs the oversampling loop in `ayumi.c` to
stop at a given tick.

## MIDI ideas

- **Timer FX as an instrument voice** (sync-buzzer, PWM): note-on = START; note number +
  pitch bend = timer rate through the existing tuning tables (like tone period); velocity =
  amplitude; CCs = duty / envelope period; note-off = STOP. Fits next to ChipInstrument /
  NotesToPsg, playable and editable as notes. MPE for per-voice rate glides.
- **Precise rates:** 7-bit CC is not enough. Use 14-bit CC pairs (as CC20/CC52 for tone period),
  NRPN, or pitch bend for the fine part.
- **Raw timed writes between uZX plugins** (TAYM import, emulator capture): one SysEx per
  frame/block with packed entries `(delta_cycles as 7-bit varint, reg, value)`; the message
  timestamp marks the block start, deltas give sub-sample timing. Only inside uZX/Tracktion:
  VST3 does not pass SysEx to plugins at all, and AU/VST2 hosts are inconsistent.
- **NRPN register writes** (CC99/98 select reg, CC6 value): fine for a 50 Hz register stream,
  too heavy for timer fx.
- **External DAW (VSTi):** notes + automatable parameters driving the in-plugin timer engine.
  This is the only option portable across VST3 / AU / CLAP.
