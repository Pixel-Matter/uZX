import { Interval, Scale } from '@tonaljs/tonal'

/** The engine's NoteSound record (src/chip/web/Engine.h): 26 floats, then the ornament's offsets and the level
    and tone tables. The worklet checks the length against the engine's. */
export const soundLength = 170
const maxOrnamentSteps = 16
const maxTableSteps = 64
const ornamentAt = 26
const levelsAt = ornamentAt + maxOrnamentSteps
const tonesAt = levelsAt + maxTableSteps

/** Envelope shapes in the engine's order, as `envshape` names them. */
export const shapes = ['tri', 'trifall', 'saw', 'sawfall', 'attack', 'decay', 'attackcut', 'decayhold']

const toSwitch = x => (x === true || Number(x) >= 0.5 ? 1 : 0)
const number = (x, fallback) => (x === undefined ? fallback : Number(x))

/** The numbers of a list control, as `orn("0:2:4")` gives them. */
function steps(name, value, most) {
    const list = value === undefined ? [] : [value].flat().map(Number)
    if (list.length > most) throw new Error(`${name}: at most ${most} steps`)
    return list
}

function toShape(x) {
    const index = typeof x === 'string' && Number.isNaN(Number(x)) ? shapes.indexOf(x) : Number(x)
    if (!Number.isInteger(index) || index < 0 || index >= shapes.length) {
        throw new Error(`envshape: "${x}" is not one of ${shapes.join(', ')} or 0–7`)
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

/** The engine's record for a note with the controls `v`. `pitch` is its MIDI pitch, `scale` the event's scale if
    it has one, `fps` the frame rate: the controls' times are in seconds and the record's in frames. */
export function noteSound(v, { pitch, scale, fps }) {
    const frames = (seconds, fallback = 0) => number(seconds, fallback) * fps
    const ornament = ornamentSemitones(steps('orn', v.orn, maxOrnamentSteps), pitch, scale)
    const [pitchStart, pitchPeak, pitchAttack, pitchDecay] = pitchEnvelope(v)
    // A table has a value for each frame. The chip has 16 volumes; the tone is its period, or a frequency in Hz.
    const levels = steps('vols', v.vols, maxTableSteps).map(volume => volume / 15)
    if (v.periods !== undefined && v.freqs !== undefined) {
        throw new Error('periods and freqs both set the tone: use one of them')
    }
    const tones = v.periods === undefined
        ? steps('freqs', v.freqs, maxTableSteps).map(hz => 69 + 12 * Math.log2(hz / 440))
        : steps('periods', v.periods, maxTableSteps)

    const sound = new Float32Array(soundLength)
    sound.set([
        toSwitch(v.tone ?? 1),
        toSwitch(v.noise),
        toSwitch(v.envelope),
        toSwitch(v.envdrive),
        toSwitch(v.envretrig),
        v.envshape === undefined ? 0 : toShape(v.envshape),
        number(v.noiseperiod, 0),
        frames(v.noisehold),
        number(v.prio, 0),
        number(v.pan, -1),
        frames(v.attack),
        frames(v.decay),
        // As in Strudel, a decay without a sustain level falls to silence.
        number(v.sustain, v.decay === undefined ? 1 : 0),
        frames(v.release),
        pitchStart,
        pitchPeak,
        frames(pitchAttack),
        frames(pitchDecay),
        number(v.vib, 0) / fps,
        number(v.vibmod, 0.5),
        frames(v.vibdelay),
        frames(v.ornstep), // without it each step lasts one frame
        ornament.length,
        levels.length,
        tones.length,
        v.periods === undefined ? 0 : 1,
    ])
    sound.set(ornament, ornamentAt)
    sound.set(levels, levelsAt)
    sound.set(tones, tonesAt)
    if (!sound.every(Number.isFinite)) {
        throw new Error('A sound control of this note is not a number')
    }
    return sound
}
