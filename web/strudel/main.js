import * as strudel from '@strudel/core'
import * as mini from '@strudel/mini'
import { transpiler } from '@strudel/transpiler'
import { initEditor, updateMiniLocations, highlightMiniLocations } from '@strudel/codemirror'
import { ChipAudio } from './audio.js'
import { ScoreView } from './score-view.js'
import './style.css'

const example = `// The chord asks for three channels while the bass is held.
stack(
  note("<c2 g1 eb2 bb1>").s("bass").gain(0.65),
  note("~ [c4,e4,g4] ~ [d4,f4,a4]").s("pluck").gain(0.75)
)`

const ui = Object.fromEntries(
    ['editor', 'play', 'update', 'stop', 'chip', 'status', 'error', 'score', 'owners', 'ownership']
        .map(id => [id, document.getElementById(id)]),
)
const engine = new ChipAudio()
const view = new ScoreView(ui.score, ui.ownership, ui.owners)
const instruments = new Map([['bass', 0], ['pluck', 1]])
const scope = strudel.evalScope(strudel, mini)
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
        if (active) void reEvaluate()
        else void play()
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

function updateControls() {
    ui.play.disabled = Boolean(active || starting || stopping || evaluating)
    ui.stop.disabled = !active && !starting
    ui.update.disabled = !scheduler.started || evaluating
    ui.chip.disabled = Boolean(active || starting || stopping)
}

function cycleTime(cycle) {
    return scheduler.seconds_at_cps_change
        + (cycle - scheduler.num_cycles_at_cps_change) / scheduler.cps + scheduler.latency
}

function stage(entry, cycle) {
    const frame = toFrame(cycleTime(cycle))
    engine.submit([{ frame, generation: entry.generation, switch: true }])
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
    const { s, note, freq, gain = 0.8, velocity = 1 } = hap.value ?? {}
    const instrument = instruments.get(s ?? 'bass')
    if (instrument === undefined) {
        showError(new Error(`Unknown AY/YM instrument "${s}". Choose bass or pluck.`))
        return
    }
    if (note === undefined && freq === undefined) return
    const pitch = note === undefined
        ? strudel.freqToMidi(Number(freq))
        : (typeof note === 'number' ? note : strudel.noteToMidi(note))
    const level = Math.max(0, Math.min(1, Number(velocity) * Number(gain)))
    if (!(durationSeconds > 0) || !Number.isFinite(durationSeconds)
        || !Number.isFinite(pitch) || pitch < 0 || pitch > 127 || !(level > 0)) return

    const frame = toFrame(targetSeconds)
    const offFrame = Math.max(frame + 1, toFrame(targetSeconds + durationSeconds))
    const cutoff = hap.context.chipCutoff ?? Infinity
    if (frame >= cutoff) return
    const generation = hap.context.chipGeneration
    const occurrence = nextOccurrence++
    const event = { frame, generation, occurrence, instrument, pitch, velocity: level }
    commands.push({ ...event, gate: true })
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
    const pattern = await evaluator.evaluate(code, false)
    if (!pattern) throw evaluator.state.evalError ?? new Error('Pattern evaluation failed')
    const cps = evaluator.scheduler.cps
    if (!Number.isFinite(cps) || cps <= 0) throw new Error('Tempo must be a positive number')
    return { pattern, code, cps, locations: evaluator.state.miniLocations, generation: nextGeneration++ }
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
            throw new Error('Stop and play again to apply a new setcps or setcpm tempo.')
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

async function stop() {
    if (!active && !starting) return
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
ui.play.addEventListener('click', play)
ui.update.addEventListener('click', reEvaluate)
ui.stop.addEventListener('click', stop)

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
ui.owners.addEventListener('toggle', () => {
    if (ui.owners.open) view.draw(active ? engine.outputFrame : 0, engine.sampleRate || 48000)
})
requestAnimationFrame(redraw)
