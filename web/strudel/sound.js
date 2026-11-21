import { Interval, Scale } from '@tonaljs/tonal'

/** The engine's NoteSound record (src/chip/web/Engine.h): 23 floats, then the ornament's offsets. */
export const soundLength = 39
const maxOrnamentSteps = 16
const ornamentAt = 23

/** Envelope shapes in the engine's order, as `ayshape` names them. */
export const shapes = ['tri', 'trifall', 'saw', 'sawfall', 'attack', 'decay', 'attackcut', 'decayhold']

/** What `s` selects: controls a note gets unless it sets them itself. A note without `s` is a bass. */
const presets = {
    bass: { ayprio: 2 },
    pluck: { ayprio: 1, aynoise: 1, noisehold: 0.04, aynoiseperiod: 5, decay: 0.14 },
}
const presetNames = Object.keys(presets)

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

/** The engine's record for one Strudel event. `pitch` is the event's MIDI pitch, `cps` the tempo in cycles
    per second, `scale` the event's scale if it has one. Returns the instrument tag for the display too. */
export function noteSound(value, { pitch, cps, scale }) {
    const name = value.s ?? 'bass'
    if (!(name in presets)) {
        throw new Error(`Unknown AY/YM instrument "${name}". Choose ${presetNames.join(' or ')}, or set controls.`)
    }
    const v = { ...presets[name], ...value }
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
    return { instrument: presetNames.indexOf(name), sound }
}
