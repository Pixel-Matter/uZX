import * as strudel from '@strudel/core'
import * as mini from '@strudel/mini'
import * as tonal from '@strudel/tonal'
import { transpiler } from '@strudel/transpiler'
import { initEditor, updateMiniLocations, highlightMiniLocations } from '@strudel/codemirror'
import { marked } from 'marked'
import { ChipAudio } from './audio.js'
import { controls } from './controls.js'
import { instrumentNames, noteSound, withInstrument } from './sound.js'
import { ScoreView, showInstruments } from './score-view.js'
import help from './help.md?raw'
import './style.css'

const example = `// An instrument is a function that sets controls. Times inside a note are in seconds.
setfps(50) // frames a second: a note starts on a frame, and its sound moves a step each frame
setfpb(24) // the tempo: 24 frames in a beat, which is 125 bpm, and four beats in a cycle
const f = 1 / fps          // a frame, to count times in
const A = 0, B = .5, C = 1 // pan picks the chip channel

// T, N and E switch the tone, the noise and the chip's envelope, as tone(), noise() and envelope() do
const triBass = p => p.T(0).E(1).envdrive(1).envshape("tri") // the envelope alone, at the note's pitch
const envPitch = p => p.T(0).envdrive(1).envshape("tri")     // sets the envelope's pitch, takes no channel
// a triad in scale steps from the root
const pad = p => p.decay(25*f).sustain(.3).orn("0:2:4").ornstep(3*f).prio(1)
const lead = p => p.N(1).noisehold(2*f).decay(6*f).sustain(.6)
  .penv(12).pdecay(3*f).vib(6).vibmod(.3).vibdelay(8*f).prio(1)
const bendLead = p => lead(p).penv(-5).pdecay(12*f)

// instrument() names controls for s("name"). A drum has its own note and the highest priority.
// The kick is a tracker sample: a table has a value for each frame, here the tone's period and the volume.
instrument("kick", note("c2").periods("512:768:1024:1152:1152:1280:1408:1536:1664")
  .vols("15:14:12:10:8:6:4:2:1:0").N(1).noisehold(f).noiseperiod(8).prio(2))
instrument("snare", note("a3").penv(12).pdecay(2*f).N(1).noiseperiod(8).decay(7*f).prio(2))
instrument("hat", note("c6").T(0).N(1).noiseperiod(1).decay(2*f).prio(2))

const bass2 = note("g1 ~ g1 g1 a1 ~ a1 f#1")

stack(
  cat(
    triBass(note("b1 ~ b1 b1 ~ b1 d2 e2")),
    // a square tone through the envelope, which sounds a fifth below it
    stack(envPitch(bass2), bass2.transpose(7).E(1))
  ).pan(B), // no prio, so the lowest priority: a drum takes this channel while it sounds
  s("kick hat snare ~ kick kick snare hat").pan(B),
  pad(n("<[0 5 3 4] [0@3 3 4@2 6@2]>").scale("B3:minor")).pan(A),
  cat(
    lead(n("4 ~ 7 6 4 ~ 2 ~")),
    bendLead(n("4 6 ~ 7@2 9 ~ 8"))
  ).scale("B4:minor").pan(C)
)`

const ui = Object.fromEntries(
    ['editor', 'play', 'playLabel', 'stopLabel', 'update', 'chip', 'status', 'error', 'legend', 'score', 'ownership', 'help']
        .map(id => [id, document.getElementById(id)]),
)
const engine = new ChipAudio()
const view = new ScoreView(ui.score, ui.ownership)
showInstruments(ui.legend, instrumentNames({}))
// The panel shows help.md, with its headings a level under the page's.
marked.use({ walkTokens: token => { if (token.type === 'heading') token.depth++ } })
ui.help.innerHTML = marked.parse(help)

const defaultFps = 50 // a ZX Spectrum's frames
let defining // what the code under evaluation has set so far: its instruments, frame rate and tempo in frames

/** `instrument("hat", note("c6").noise(1).decay(.04))` in a pattern's code: `s("hat")` then plays notes with
    these controls. The controls are a pattern's, or what a function such as `p => p.noise(1)` sets. */
function instrument(name, definition) {
    // The transpiler turns a double-quoted name into a pattern: of two words if the name has a space.
    const [word, ...more] = name instanceof strudel.Pattern ? name.firstCycleValues : [name]
    const pattern = typeof definition === 'function' ? definition(strudel.pure({})) : definition
    const [controls] = pattern instanceof strudel.Pattern ? pattern.firstCycleValues : []
    if (typeof word !== 'string' || !/^\w+$/.test(word) || more.length || typeof controls !== 'object') {
        throw new Error('instrument("name", note("c2").decay(.1)) takes a one-word name and the controls of its notes')
    }
    defining.instruments[word] = controls
}

/** `setfps(50)` in a pattern's code: how many times a second the chip's sound changes. A note starts on a frame,
    and its envelopes, ornament and tables move a step each frame. The code reads the rate as `fps`. */
function setfps(rate) {
    if (!Number.isInteger(rate) || rate < 1 || rate > 1000) {
        throw new Error('setfps(50) takes a whole number of frames a second, from 1 to 1000')
    }
    defining.fps = globalThis.fps = rate
}

/** `setfpb(24)` in a pattern's code: the tempo as the frames in a beat, so that beats and their even parts start
    on frames. A cycle has four beats unless `beats` says otherwise. */
function setfpb(frames, beats = 4) {
    if (!Number.isInteger(frames) || frames < 1 || !(beats > 0 && beats < Infinity)) {
        throw new Error('setfpb(24) takes a whole number of frames in a beat, then the beats in a cycle if not 4')
    }
    defining.cycleFrames = frames * beats
}

const scope = strudel.evalScope(strudel, mini, tonal, controls, { instrument, setfps, setfpb, fps: defaultFps })
const evaluator = strudel.repl({ transpiler, getTime: () => engine.currentFrame / engine.sampleRate })
const toFrame = seconds => Math.round(seconds * engine.sampleRate)

let active
let pending
let starting = false
let stopping = false
let evaluating = false
let nextGeneration = 1
let nextOccurrence = 1
let heardGeneration
let sourceNotes = []
let announcements = []

const editor = initEditor({
    root: ui.editor,
    initialCode: example,
    onChange: () => {},
    onEvaluate: () => {
        playOrUpdate()
        return true
    },
    onStop: () => {
        void stop()
        return true
    },
})
editor.contentDOM.setAttribute('aria-label', 'Strudel pattern source')

function showError(error) {
    ui.error.hidden = !error
    ui.error.textContent = error?.message ?? ''
}

/** Whether the transport button stops: a pattern plays or is about to. */
const isPlaying = () => Boolean(active || starting)

function updateControls() {
    ui.playLabel.hidden = isPlaying()
    ui.stopLabel.hidden = !isPlaying()
    ui.play.disabled = !isPlaying() && (stopping || evaluating)
    ui.update.disabled = !scheduler.started || evaluating
    ui.chip.disabled = Boolean(active || starting || stopping)
}

function cycleTime(cycle) {
    return scheduler.seconds_at_cps_change
        + (cycle - scheduler.num_cycles_at_cps_change) / scheduler.cps + scheduler.latency
}

function stage(entry, cycle) {
    const frame = toFrame(cycleTime(cycle))
    engine.submit([{ frame, generation: entry.generation, switch: true, frameRate: entry.fps }])
    view.cutScoreAt(frame)
    for (const note of sourceNotes) {
        note.end = Math.min(note.end, frame)
    }
    announcements.push({ frame, entry })
    entry.staged = true
    return frame
}

function query(entry, state) {
    return entry.pattern.query(state).map(hap => hap.setContext({
        ...hap.context,
        chipGeneration: entry.generation,
        chipInstruments: entry.instruments,
        chipFps: entry.fps,
    }))
}

const routing = new strudel.Pattern(state => {
    const { begin, end } = state.span
    if (!active) return []
    if (!active.staged) stage(active, Number(begin))
    if (!pending || end.lte(pending.cycle)) return query(active, state)

    const replacement = pending
    pending = undefined
    const split = begin.lt(replacement.cycle)
    const before = split
        ? query(active, state.setSpan(new strudel.TimeSpan(begin, replacement.cycle)))
        : []
    try {
        const after = query(replacement, state.setSpan(new strudel.TimeSpan(
            split ? replacement.cycle : begin, end,
        )))
        const cutoff = stage(replacement, replacement.cycle)
        active = replacement
        for (const hap of before) hap.context.chipCutoff = cutoff
        return before.concat(after)
    } catch (error) {
        showError(error)
        ui.status.textContent = 'Playing'
        return query(active, state)
    }
})

let commands = []
let batchPending = false

function scheduleNote(hap, _relativeTime, durationSeconds, _cps, targetSeconds) {
    let instrument
    let controls
    try {
        ({ instrument, controls } = withInstrument(hap.value ?? {}, hap.context.chipInstruments))
    } catch (error) {
        showError(error)
        return
    }
    const { note, freq, gain = 1, velocity = 1 } = controls
    if (note === undefined && freq === undefined) return
    const pitch = note === undefined
        ? strudel.freqToMidi(Number(freq))
        : (typeof note === 'number' ? note : strudel.noteToMidi(note))
    const level = Math.max(0, Math.min(1, Number(velocity) * Number(gain)))
    if (!(durationSeconds > 0) || !Number.isFinite(durationSeconds)
        || !Number.isFinite(pitch) || pitch < 0 || pitch > 127 || !(level > 0)) return
    let sound
    try {
        sound = noteSound(controls, { pitch, scale: hap.context.scale, fps: hap.context.chipFps })
    } catch (error) {
        showError(error)
        return
    }

    const frame = toFrame(targetSeconds)
    const offFrame = Math.max(frame + 1, toFrame(targetSeconds + durationSeconds))
    const cutoff = hap.context.chipCutoff ?? Infinity
    if (frame >= cutoff) return
    const generation = hap.context.chipGeneration
    const occurrence = nextOccurrence++
    const event = { frame, generation, occurrence, instrument, pitch, velocity: level }
    commands.push({ ...event, gate: true, sound })
    if (offFrame < cutoff) commands.push({ ...event, frame: offFrame, gate: false })
    const scheduled = { ...event, end: Math.min(offFrame, cutoff), hap }
    view.addNote(scheduled)
    sourceNotes.push(scheduled)
    if (sourceNotes.length > 2048) sourceNotes.shift()

    if (!batchPending) {
        batchPending = true
        queueMicrotask(() => {
            batchPending = false
            const batch = commands
            commands = []
            batch.sort((a, b) => a.frame - b.frame || Number(a.gate) - Number(b.gate))
            if (active && batch.length) engine.submit(batch)
        })
    }
}

// In core 1.2.6 the fifth onTrigger argument is the absolute getTime deadline.
// The relative deadline already accounts for a scheduler window and is not used here.
const scheduler = new strudel.Cyclist({
    interval: 0.05,
    latency: 0.42,
    getTime: () => engine.currentFrame / engine.sampleRate,
    onTrigger: scheduleNote,
    onError: showError,
})

async function evaluate() {
    const code = editor.state.doc.toString()
    await scope
    evaluator.setCps(scheduler.cps)
    const defined = defining = { instruments: {}, fps: globalThis.fps = defaultFps }
    const pattern = await evaluator.evaluate(code, false)
    if (!pattern) throw evaluator.state.evalError ?? new Error('Pattern evaluation failed')
    // setfpb counts the tempo in frames, whichever of it and setfps the code calls first.
    const cps = defined.cycleFrames ? defined.fps / defined.cycleFrames : evaluator.scheduler.cps
    if (!Number.isFinite(cps) || cps <= 0) throw new Error('Tempo must be a positive number')
    return {
        pattern, code, cps, locations: evaluator.state.miniLocations, generation: nextGeneration++,
        instruments: defined.instruments, fps: defined.fps,
    }
}

let transportToken = 0

async function play() {
    if (active || starting || stopping || evaluating) return
    const token = ++transportToken
    starting = true
    showError()
    ui.status.textContent = 'Starting audio…'
    updateControls()
    try {
        // Call start from the event handler so AudioContext can use the user gesture.
        const audioReady = engine.start({ isYm: ui.chip.value === 'ym' })
        const [entry] = await Promise.all([evaluate(), audioReady])
        if (token !== transportToken) return
        active = entry
        scheduler.setCps(entry.cps)
        await scheduler.setPattern(routing)
        if (token !== transportToken) return
        await scheduler.start()
        if (token === transportToken) ui.status.textContent = 'Playing'
    } catch (error) {
        if (token !== transportToken) return
        await stop()
        showError(error)
    } finally {
        if (token === transportToken) {
            starting = false
            updateControls()
        }
    }
}

async function reEvaluate() {
    if (!scheduler.started || evaluating) return
    const token = transportToken
    evaluating = true
    showError()
    updateControls()
    try {
        const entry = await evaluate()
        if (token !== transportToken) return
        if (entry.cps !== scheduler.cps) {
            throw new Error('Stop and play again to apply a new tempo.')
        }
        const scheduledThrough = scheduler.lastEnd
        const earliestTime = engine.currentFrame / engine.sampleRate + 0.025
        let cycle = Math.max(1, Math.ceil(
            scheduler.num_cycles_at_cps_change
            + (earliestTime - cycleTime(scheduler.num_cycles_at_cps_change)) * scheduler.cps,
        ))
        // Keep committed transitions in order until the listener reaches them.
        const committed = announcements.at(-1)
        if (committed?.entry.cycle !== undefined) cycle = Math.max(cycle, committed.entry.cycle + 1)
        const replacement = { ...entry, cycle }
        if (cycle < scheduledThrough) {
            // Replace the slice that Cyclist already queried during lookahead.
            // Cyclist handles all later slices, so these onsets are submitted once.
            const state = new strudel.State(new strudel.TimeSpan(cycle, scheduledThrough), {
                _cps: scheduler.cps, cyclist: 'cyclist',
            })
            const haps = query(replacement, state)
            stage(replacement, cycle)
            active = replacement
            pending = undefined
            for (const hap of haps) {
                if (hap.hasOnset()) {
                    scheduleNote(hap, 0, hap.duration / scheduler.cps, scheduler.cps, cycleTime(hap.whole.begin))
                }
            }
        } else {
            pending = replacement
        }
        ui.status.textContent = `Pattern queued for cycle ${cycle + 1}`
    } catch (error) {
        if (token === transportToken) showError(error)
    } finally {
        evaluating = false
        updateControls()
    }
}

function playOrUpdate() {
    if (active) void reEvaluate()
    else void play()
}

async function stop() {
    if (!isPlaying()) return
    ++transportToken
    scheduler.stop()
    active = pending = heardGeneration = undefined
    starting = false
    commands = []
    sourceNotes = []
    announcements = []
    stopping = true
    view.clear()
    updateMiniLocations(editor, [])
    highlightMiniLocations(editor, 0, [])
    showError()
    ui.status.textContent = 'Stopping…'
    updateControls()
    try {
        await engine.stop()
    } catch (error) {
        showError(error)
    } finally {
        stopping = false
        ui.status.textContent = 'Stopped'
        updateControls()
    }
}

engine.onTrace = ({ traces }) => {
    if (active) for (const record of traces) view.addTrace(record)
}
engine.onStatus = status => {
    if (status.type === 'overflow') {
        showError(new Error('The chip command queue overflowed and released its voices.'))
    }
}
engine.onError = showError
ui.play.addEventListener('click', () => (isPlaying() ? stop() : play()))
ui.update.addEventListener('click', reEvaluate)
// Strudel's keys. The editor handles them while it has the focus; this covers the rest of the page.
document.addEventListener('keydown', event => {
    if (event.defaultPrevented || !(event.ctrlKey || event.altKey)) return
    if (event.key === 'Enter') playOrUpdate()
    else if (event.key === '.' || event.code === 'Period') void stop() // Option+. types another character
    else return
    event.preventDefault() // or Enter would also press the focused button
})

let previousFrame = -1
function redraw() {
    const frame = active ? engine.outputFrame : 0
    const rate = engine.sampleRate || 48000
    if (active || frame !== previousFrame) view.draw(frame, rate)
    if (frame !== previousFrame) {
        previousFrame = frame
        let announcement
        while (announcements.length && announcements[0].frame <= frame) {
            announcement = announcements.shift()
        }
        if (announcement) {
            heardGeneration = announcement.entry
            showInstruments(ui.legend, instrumentNames(heardGeneration.instruments))
            updateMiniLocations(editor, heardGeneration.code === editor.state.doc.toString()
                ? heardGeneration.locations : [])
            if (!pending && announcements.length === 0) ui.status.textContent = 'Playing'
        }
        sourceNotes = sourceNotes.filter(note => note.end > frame)
        const haps = editor.state.doc.toString() === heardGeneration?.code
            ? sourceNotes.filter(note => note.frame <= frame && note.generation === heardGeneration.generation)
                .map(note => note.hap)
            : []
        highlightMiniLocations(editor, 0, haps)
    }
    requestAnimationFrame(redraw)
}
const sizeObserver = new ResizeObserver(() => {
    view.draw(active ? engine.outputFrame : 0, engine.sampleRate || 48000)
})
sizeObserver.observe(ui.score)
requestAnimationFrame(redraw)
