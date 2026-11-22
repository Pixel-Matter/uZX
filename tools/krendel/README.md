# krendel: Strudel → μZX Studio over MIDI

Experimental. Design: [docs/design/krendel-midi.md](../../docs/design/krendel-midi.md).

## Play

1. Make a virtual MIDI port: IAC Driver on macOS (Audio MIDI Setup → MIDI Studio → IAC Driver → Device is online), loopMIDI on Windows, `snd-virmidi` on Linux.
2. In Studio: enable the port in **Audio Settings** (`Cmd+,`), add a track with **MIDI to PSG** and an **AY chip**, pick the port in the track's **I** menu and set monitoring to **On**.
3. In [strudel.cc](https://strudel.cc), run once (the browser asks for MIDI access):

   ```js
   await import('https://cdn.jsdelivr.net/gh/Pixel-Matter/uZX@<branch or tag>/tools/krendel/uzx.mjs')
   ```

4. Play, with your port's name: a lead, a buzzer bass that drives the envelope, and noise hats.

   ```js
   setcpm(30)
   const port = 'IAC Driver Bus 1'
   $: note("c4 eb4 g4 bb4").midichan(1).uzx(port)
   $: note("<c2 g1>").midichan(3).tone(0).envelope(1).envdrive(1).envshape("<saw tri>").uzx(port)
   $: note("c5*8").midichan(5).tone(0).noise(1).noiseperiod("<2 8>").velocity(.6).uzx(port)
   ```

The controls are in `controls` in `uzx.mjs` and in the design note. `.T()`, `.N()` and `.E()` are short for `.tone()`, `.noise()` and `.envelope()`.

## Test

```sh
cd tools/krendel
npm ci
npm test
```
