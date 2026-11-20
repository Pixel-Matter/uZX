# uZX Strudel

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

Open the localhost URL printed by Vite. Select a chip and press **Play**. Edit the pattern and press **Re-evaluate** to switch on the next cycle. A syntax error appears below the editor while the last valid pattern continues. The example uses `s("bass")` and `s("pluck")` to select the two built-in instruments. To change the global tempo with `setcps` or `setcpm`, stop and play the edited pattern.

The playhead separates rendered history on the left from scheduled notes on the right. Expand **Chip ownership** to see which occurrence holds A, B, C, noise and envelope. An unallocated note can stay visible in the roll while another note uses its channel.

For a production build, run `npm run build` after the WASM build, then `npm run preview` to test the resulting `dist/` directory. Use HTTPS or localhost for Web Audio. CMake writes the generated module to `web/strudel/public/engine.wasm`.

## Tests

After building the module, run `npm test` in `web/strudel` to exercise the worklet command queue against the compiled engine.

Run the same C++ tests natively from the repository root with a C++23 compiler.

```sh
cmake -S src/chip -B build/strudel-native -DCMAKE_BUILD_TYPE=Release -DUZX_BUILD_WEB_ENGINE=ON
cmake --build build/strudel-native --parallel
ctest --test-dir build/strudel-native --output-on-failure
```

## Dependencies

The browser entry uses Strudel CodeMirror 1.3.0 and Strudel core, mini and transpiler 1.2.6 under AGPL-3.0-or-later. The lockfile records the resolved packages. The C++ model comes from uZX and the emulator is the repository's Ayumi fork. [Strudel source](https://codeberg.org/uzu/strudel) · [uZX license](../../LICENCE.md) · [Ayumi license](../../third_party/ayumi/LICENSE)
