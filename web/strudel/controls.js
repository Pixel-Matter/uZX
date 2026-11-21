import { registerControl } from '@strudel/core'

// The chip controls carry the `ay` prefix of tools/krendel/uzx.mjs: Strudel has its own `noise`, `shape` and `env`.
// The rest of a note's sound uses Strudel's controls: attack, decay, sustain, release, penv, pattack, pdecay,
// vib, vibmod and pan. sound.js reads them all.
const names = [
    'aytone',         // 1 or 0: the square wave
    'aynoise',        // 1 or 0: the noise
    'ayenv',          // 1 or 0: the chip's envelope shapes the level
    'aybuzz',         // 1: the note's pitch is the envelope's
    'ayretrig',       // 1: the note restarts the envelope it drives
    'ayshape',        // an envelope shape name, or 0–7
    'aynoiseperiod',  // 0 highest noise … 31 lowest
    'ayprio',         // who plays when the chip has too few channels
    'noisehold',      // seconds of noise from the note's start
    'vibdelay',       // seconds before the vibrato starts
    'orn',            // pitch offsets that loop inside the note, as "0:2:4"
    'ornstep',        // cycles per ornament step
]

/** Functions for the evaluation scope. Registering also adds each as a Pattern method. */
export const controls = Object.assign({}, ...names.map(name => registerControl(name)))
