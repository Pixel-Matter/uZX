import { Interval, Scale } from '@tonaljs/tonal'

/** The engine's NoteSound record (src/chip/web/Engine.h): 23 floats, then the ornament's offsets. */
export const soundLength = 39
const maxOrnamentSteps = 16
const ornamentAt = 23

/** Envelope shapes in the engine's order, as `ayshape` names them. */
export const shapes = ['tri', 'trifall', 'saw', 'sawfall', 'attack', 'decay', 'attackcut', 'decayhold']

const toSwitch = x => (x === true || Number(x) >= 0.5 ? 1 : 0)
const number = (x, fallback) => (x === undefined ? fallback : Number(x))

function toShape(x) {
    const index = typeof x === 'string' && Number.isNaN(Number(x)) ? shapes.indexOf(x) : Number(x)
    if (!Number.isInteger(index) || index < 0 || index >= shapes.length) {
        throw new Error(`ayshape: "${x}" is not one of ${shapes.join(', ')} or 0–7`)
    }
    return index
}

/** The ornament's offsets in semitones from `pitch`.
    With a scale (`.scale()` names it as "B3 minor") and a pitch on one of its steps, the offsets are scale steps.
    Otherwise they are semitones already.
*/
export function ornamentSemitones(offsets, pitch, scale) {
    const steps = scale ? Scale.get(scale) : undefined
    if (!steps || steps.empty || !steps.tonic) return offsets
    const semitones = steps.intervals.map(interval => Interval.semitones(interval))
    const tonic = Interval.semitones(Interval.distance('C', steps.tonic.replace(/-?\d+$/, '')))
    const from = semitones.indexOf((((pitch - tonic) % 12) + 12) % 12)
    if (from < 0) return offsets
    return offsets.map(offset => {
        const step = from + Math.round(offset)
        const octave = Math.floor(step / semitones.length)
        return semitones[step - octave * semitones.length] + 12 * octave - semitones[from]
    })
}

/** The pitch envelope as Strudel's `penv` reads: with a decay the pitch goes out to `penv` and comes back,
    with an attack alone it arrives from `penv` below. */
function pitchEnvelope({ penv, pattack, pdecay }) {
    if (!Number(penv)) return [0, 0, 0, 0]
    return pdecay === undefined
        ? [-Number(penv), 0, number(pattack, 0.2), 0]
        : [0, Number(penv), number(pattack, 0), Number(pdecay)]
}

/** The names by instrument tag, as the piano roll's legend shows them: a note without `s`, then the instruments
    that a pattern's code defined. */
export const instrumentNames = instruments => ['note', ...Object.keys(instruments)]

/** A Strudel event's controls over those of the instrument its `s` names, with the instrument's tag.
    `instruments` has the controls of each instrument that the pattern's code defined. */
export function withInstrument(value, instruments = {}) {
    if (value.s === undefined) return { instrument: 0, controls: value }
    const tag = instrumentNames(instruments).indexOf(value.s, 1)
    if (tag < 0) {
        throw new Error(`No instrument "${value.s}". Define it: instrument("${value.s}", note("c2").decay(.1))`)
    }
    return { instrument: tag, controls: { ...instruments[value.s], ...value } }
}

/** The engine's record for a note with the controls `v`. `pitch` is its MIDI pitch, `cps` the tempo in cycles
    per second, `scale` the event's scale if it has one. */
export function noteSound(v, { pitch, cps, scale }) {
    const ornament = v.orn === undefined ? [] : ornamentSemitones([v.orn].flat().map(Number), pitch, scale)
    if (ornament.length > maxOrnamentSteps) {
        throw new Error(`orn: an ornament has at most ${maxOrnamentSteps} steps`)
    }

    const sound = new Float32Array(soundLength)
    sound.set([
        toSwitch(v.aytone ?? 1),
        toSwitch(v.aynoise),
        toSwitch(v.ayenv),
        toSwitch(v.aybuzz),
        toSwitch(v.ayretrig),
        v.ayshape === undefined ? 0 : toShape(v.ayshape),
        number(v.aynoiseperiod, 0),
        number(v.noisehold, 0),
        number(v.ayprio, 0),
        number(v.pan, -1),
        number(v.attack, 0),
        number(v.decay, 0),
        // As in Strudel, a decay without a sustain level falls to silence.
        number(v.sustain, v.decay === undefined ? 1 : 0),
        number(v.release, 0),
        ...pitchEnvelope(v),
        number(v.vib, 0),
        number(v.vibmod, 0.5),
        number(v.vibdelay, 0),
        // `ornstep` is in cycles, so the ornament keeps the tempo. Without it each step lasts one frame.
        v.ornstep === undefined ? 0 : Number(v.ornstep) / cps,
        ornament.length,
    ])
    sound.set(ornament, ornamentAt)
    if (!sound.every(Number.isFinite)) {
        throw new Error('A sound control of this note is not a number')
    }
    return sound
}
