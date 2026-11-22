# Krendel

Live-code one AY-3-8910 or YM2149 in the browser. The Strudel editor sends timed notes to a C++ engine running Ayumi in an AudioWorklet.

## Build and play

Use CMake 3.20 or newer, an activated Emscripten SDK and Node.js with npm. The commands below start in the repository root. The standalone build uses the Ayumi sources already in this repository.

```sh
emcmake cmake -S src/chip -B build/strudel-wasm -DCMAKE_BUILD_TYPE=Release -DUZX_BUILD_WEB_ENGINE=ON
cmake --build build/strudel-wasm --parallel
ctest --test-dir build/strudel-wasm --output-on-failure
cd web/strudel
npm ci
npm run dev
```

Open the localhost URL printed by Vite. Select a chip and press **Play**; the same button then stops. Edit the pattern and press **Re-evaluate** to switch on the next cycle. The keys are Strudel's, anywhere on the page: `Ctrl+Enter` or `Alt+Enter` plays, or re-evaluates while playing, and `Ctrl+.` or `Alt+.` stops. A syntax error appears below the editor while the last valid pattern continues. To change the tempo, stop and play the edited pattern; a new frame rate alone takes effect with the re-evaluated pattern.

The playhead separates rendered history on the left from scheduled notes on the right. Expand **Chip ownership** to see which occurrence holds A, B, C, noise and envelope. An unallocated note can stay visible in the roll while another note uses its channel.

For a production build, run `npm run build` after the WASM build, then `npm run preview` to test the resulting `dist/` directory. Use HTTPS or localhost for Web Audio. CMake writes the generated module to `web/strudel/public/engine.wasm`. Rebuild it after a change under `src/chip`: **Play** reports a module that is older than the page.

## Frames and tempo

The chip's sound changes a fixed number of times a second. `setfps(50)` in the pattern's code sets that number, from 1 to 1000; it is 50 without the call. A note starts on a frame, and its envelopes, ornament and tables move one step each frame. The code reads the rate as `fps`, so `const f = 1 / fps` is one frame, to count times in: `decay(7*f)`.

`setfpb(24)` sets the tempo as the frames in a beat, so that every beat and its even parts start on a frame. A cycle has four beats, or as many as a second argument says: `setfpb(24)` at 50 frames a second is 125 beats a minute and a cycle of 96 frames. A tempo from `setcpm` or `setcps` plays too, with each note moved to the next frame.

## A note's sound

A note is a plain square tone at full volume unless controls say otherwise. Times inside a note are in seconds, so the tempo doesn't stretch them. An instrument is a function that sets controls, such as `const pluck = p => p.noise(1).noisehold(2*f).decay(7*f)`, applied as `pluck(note("c4 e4"))`.

`instrument("hat", note("c6").tone(0).noise(1).decay(2*f))` gives a set of controls a name for `s`, so that one sequence can mix instruments: `s("kick hat snare hat")`. The second argument is the controls, or a function that sets them, such as the `pluck` above. There are no built-in instruments: every name is defined in the pattern's code, and a control on the note overrides the instrument's. A definition takes effect when its pattern does, and each instrument has its own color in the piano roll.

A drum is an instrument with a note of its own, so it plays without one, and a high `prio`, so it takes a channel from another note while it sounds. To have the drums share the bass's channel, give them its `pan` and the bass the lowest `prio`.

A table has a value for each frame of the note, as a tracker's sample does, and its last value holds. `vols` is the chip's volume, and a table that ends at 0 ends the note. `periods` is the tone's period as the chip counts it, and `freqs` its frequency in Hz; either replaces the note's pitch. This kick is a tracker's sample, line by line:

```js
instrument("kick", note("c2").periods("512:768:1024:1152:1152:1280:1408:1536:1664")
  .vols("15:14:12:10:8:6:4:2:1:0").noise(1).noisehold(f).noiseperiod(8).prio(2))
```

| Control | Sets |
| --- | --- |
| `tone`, `noise`, `envelope` | 1 or 0, a tracker's T, N and E: the square wave, the noise, and the level following the chip's envelope. `.T(0)`, `.N(1)` and `.E(1)` are the same switches |
| `envdrive` | 1: the note's pitch is the envelope's. With `tone(0)` and `envelope(0)` the note only sets that pitch and takes no channel |
| `envshape` | The envelope shape: `tri`, `trifall`, `saw`, `sawfall`, `attack`, `decay`, `attackcut`, `decayhold`, or 0–7 |
| `envretrig` | 1: the note restarts the envelope it drives |
| `noiseperiod` | 0 highest noise … 31 lowest |
| `noisehold` | The noise sounds only this long from the note's start |
| `prio` | Who plays when the chip has too few channels: the higher, then the later. 0 to 127; 0 unless set |
| `pan` | The channel: 0 is A, 0.5 is B, 1 is C |
| `gain`, `velocity` | The volume, 1 unless set: full, the chip's 15 |
| `attack`, `decay`, `sustain`, `release` | The level, as in Strudel. A decay without a sustain level falls to silence and ends the note |
| `vols` | A table of the chip's volume, 0–15, instead of attack, decay and sustain: `vols("15:12:8:0")`. At most 64 steps |
| `periods`, `freqs` | A table of the tone's period, 9–4095, or of its frequency in Hz, instead of the note's pitch. At most 64 steps |
| `penv`, `pattack`, `pdecay` | The pitch, in semitones. With `pdecay` the note starts `penv` away and comes back; with `pattack` alone it arrives from `penv` below |
| `vib`, `vibmod`, `vibdelay` | Vibrato rate in Hz, depth in semitones (0.5 unless set), and the time before it starts |
| `orn`, `ornstep` | Pitch offsets that loop inside one note, as `orn("0:2:4")`, each lasting `ornstep` seconds, or one frame without it. At most 16 steps |

The ornament keeps the note's level envelope running, where a pattern of separate notes would restart it. After `.scale()` its offsets are scale steps from the note, so `n("0 2").scale("B3:minor").orn("0:2:4")` plays a minor triad on B and a major one on D. Without a scale they are semitones.

## Tests

After building the module, run `npm test` in `web/strudel` to exercise the worklet command queue against the compiled engine.

Run the same C++ tests natively from the repository root with a C++23 compiler.

```sh
cmake -S src/chip -B build/strudel-native -DCMAKE_BUILD_TYPE=Release -DUZX_BUILD_WEB_ENGINE=ON
cmake --build build/strudel-native --parallel
ctest --test-dir build/strudel-native --output-on-failure
```

## Dependencies

The browser entry uses Strudel CodeMirror 1.3.0 and Strudel core, mini, tonal and transpiler 1.2.6 under AGPL-3.0-or-later, and Tonal 4.10.0 under MIT. The lockfile records the resolved packages. The C++ model comes from uZX and the emulator is the repository's Ayumi fork. [Strudel source](https://codeberg.org/uzu/strudel) · [uZX license](../../LICENCE.md) · [Ayumi license](../../third_party/ayumi/LICENSE)
