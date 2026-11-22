import { Pattern, registerControl } from '@strudel/core'

// The chip's switches have Studio's names for them, as tools/krendel/uzx.mjs does. `envelope` and `envshape` are
// longer because Strudel has its own `env` and `shape`, which mean something else.
// The rest of a note's sound uses Strudel's controls: noise, attack, decay, sustain, release, penv, pattack,
// pdecay, vib, vibmod and pan. sound.js reads them all.
const names = [
    'tone',         // 1 or 0: the square wave
    'envelope',     // 1 or 0: the chip's envelope shapes the level
    'envdrive',     // 1: the note's pitch is the envelope's
    'envretrig',    // 1: the note restarts the envelope it drives
    'envshape',     // an envelope shape name, or 0–7
    'noiseperiod',  // 0 highest noise … 31 lowest
    'noisehold',    // seconds of noise from the note's start
    'prio',         // who plays when the chip has too few channels
    'vibdelay',     // seconds before the vibrato starts
    'orn',          // pitch offsets that loop inside the note, as "0:2:4"
    'ornstep',      // seconds per ornament step
    'vols',         // a table: the chip's volume, 0–15, each frame
    'periods',      // a table: the tone's period each frame
    'freqs',        // a table: the tone's frequency in Hz each frame
]

/** Functions for the evaluation scope. Registering also adds each as a Pattern method. */
export const controls = Object.assign({}, ...names.map(name => registerControl(name)))

// A tracker's mask letters, as methods only: `.T(0).N(1)`. Lower case would take Strudel's `n`, the note number.
for (const [letter, name] of [['T', 'tone'], ['N', 'noise'], ['E', 'envelope']]) {
    Pattern.prototype[letter] = Pattern.prototype[name]
}
