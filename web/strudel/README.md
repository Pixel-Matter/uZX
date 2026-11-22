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

Open the localhost URL printed by Vite. Select a chip and press **Play**; the same button then stops. Edit the pattern and press **Re-evaluate** to switch on the next cycle. The keys are Strudel's, anywhere on the page: `Ctrl+Enter` or `Alt+Enter` plays, or re-evaluates while playing, and `Ctrl+.` or `Alt+.` stops. A syntax error appears below the editor while the last valid pattern continues. To change the global tempo with `setcps` or `setcpm`, stop and play the edited pattern.

The playhead separates rendered history on the left from scheduled notes on the right. Expand **Chip ownership** to see which occurrence holds A, B, C, noise and envelope. An unallocated note can stay visible in the roll while another note uses its channel.

For a production build, run `npm run build` after the WASM build, then `npm run preview` to test the resulting `dist/` directory. Use HTTPS or localhost for Web Audio. CMake writes the generated module to `web/strudel/public/engine.wasm`. Rebuild it after a change under `src/chip`: **Play** reports a module that is older than the page.

## A note's sound

A note is a plain square tone unless controls say otherwise. Times inside a note are in seconds, so the tempo doesn't stretch them. The chip's sound changes 50 times a second, so count a time in these frames: with `const f = 1/50`, `decay(7*f)` lasts seven. An instrument is a function that sets controls, such as `const pluck = p => p.aynoise(1).noisehold(2*f).decay(7*f)`, applied as `pluck(note("c4 e4"))`.

`instrument("hat", note("c6").aytone(0).aynoise(1).decay(2*f))` gives a set of controls a name for `s`, so that one sequence can mix instruments: `s("kick hat snare hat")`. The second argument is the controls, or a function that sets them, such as the `pluck` above. There are no built-in instruments: every name is defined in the pattern's code, and a control on the note overrides the instrument's. A definition takes effect when its pattern does, and each instrument has its own color in the piano roll.

A drum is an instrument with a note of its own, so it plays without one, and a high `ayprio`, so it takes a channel from another note while it sounds. To have the drums share the bass's channel, give them its `pan` and the bass the lowest `ayprio`. A tracker's drum sample becomes `orn` offsets, one frame each, for the tone's fall, `noisehold` for the frames with noise, and `decay` for the level:

```js
const f = 1/50 // a frame
instrument("kick", note("c2").orn("20.7:13.7:8.7:6.7:6.7:4.9:3.2:1.7:0.3")
  .aynoise(1).noisehold(f).aynoiseperiod(8).decay(8.3*f).gain(1).ayprio(2))
```

| Control | Sets |
| --- | --- |
| `aytone`, `aynoise`, `ayenv` | 1 or 0: the square wave, the noise, and the level following the chip's envelope |
| `aybuzz` | 1: the note's pitch is the envelope's. With `aytone(0)` and `ayenv(0)` the note only sets that pitch and takes no channel |
| `ayshape` | The envelope shape: `tri`, `trifall`, `saw`, `sawfall`, `attack`, `decay`, `attackcut`, `decayhold`, or 0–7 |
| `ayretrig` | 1: the note restarts the envelope it drives |
| `aynoiseperiod` | 0 highest noise … 31 lowest |
| `noisehold` | The noise sounds only this long from the note's start |
| `ayprio` | Who plays when the chip has too few channels: the higher, then the later. 0 to 127; 0 unless set |
| `pan` | The channel: 0 is A, 0.5 is B, 1 is C |
| `attack`, `decay`, `sustain`, `release` | The level, as in Strudel. A decay without a sustain level falls to silence and ends the note |
| `penv`, `pattack`, `pdecay` | The pitch, in semitones. With `pdecay` the note starts `penv` away and comes back; with `pattack` alone it arrives from `penv` below |
| `vib`, `vibmod`, `vibdelay` | Vibrato rate in Hz, depth in semitones (0.5 unless set), and the time before it starts |
| `orn`, `ornstep` | Pitch offsets that loop inside one note, as `orn("0:2:4")`, each lasting `ornstep` cycles, or one 50 Hz frame without it |

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
