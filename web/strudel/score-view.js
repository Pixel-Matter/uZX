const labelWidth = 55
const pastRatio = 0.72
const historySeconds = 2.25
const futureSeconds = historySeconds / pastRatio - historySeconds
const colors = ['#92cff0', '#f0bc78']
const ownerLanes = [
    ['A', 'a'], ['B', 'b'], ['C', 'c'], ['Noise', 'noise'], ['Envelope', 'envelope'],
]
const pitches = ['C', 'C♯', 'D', 'E♭', 'E', 'F', 'F♯', 'G', 'A♭', 'A', 'B♭', 'B']
const maxNotes = 2048
const maxRecords = 512
const pitchLabel = pitch => pitches[pitch % 12] + (Math.floor(pitch / 12) - 1)

function surface(canvas) {
    const width = canvas.clientWidth
    const height = canvas.clientHeight
    const ratio = window.devicePixelRatio || 1
    const pixelWidth = Math.round(width * ratio)
    const pixelHeight = Math.round(height * ratio)
    if (canvas.width !== pixelWidth || canvas.height !== pixelHeight) {
        canvas.width = pixelWidth
        canvas.height = pixelHeight
    }
    const ctx = canvas.getContext('2d')
    ctx.setTransform(ratio, 0, 0, ratio, 0, 0)
    ctx.clearRect(0, 0, width, height)
    ctx.font = '11px ui-monospace, monospace'
    ctx.textBaseline = 'middle'
    return { ctx, width, height }
}

export class ScoreView {
    constructor(score, ownership, details) {
        this.score = score
        this.ownership = ownership
        this.details = details
        this.clear()
    }

    clear() {
        this.notes = []
        this.trace = []
    }

    addNote(note) {
        this.notes.push(note)
        if (this.notes.length > maxNotes) this.notes.shift()
    }

    cutScoreAt(frame) {
        this.notes = this.notes.filter(note => note.frame < frame)
        for (const note of this.notes) note.end = Math.min(note.end, frame)
    }

    addTrace(record) {
        this.trace.push(record)
        if (this.trace.length > maxRecords) this.trace.shift()
    }

    draw(now, rate) {
        const earliest = now - historySeconds * rate
        this.notes = this.notes.filter(note => note.end >= earliest)
        while (this.trace.length > 1 && this.trace[1].frame < earliest) this.trace.shift()

        const score = surface(this.score)
        if (score.width <= labelWidth) return
        const playhead = labelWidth + (score.width - labelWidth) * pastRatio
        const scale = (playhead - labelWidth) / (historySeconds * rate)
        const axis = { now, playhead, earliest, rate, x: frame => playhead + (frame - now) * scale }
        const heard = this.trace.filter(record => record.frame <= now)
        this.drawScore(score, axis, heard)
        if (this.details.open) this.drawOwners(surface(this.ownership), axis, heard)
    }

    drawBackground({ ctx, width, height }, { now, playhead, earliest, rate, x }) {
        ctx.fillStyle = '#131a17'
        ctx.fillRect(0, 0, width, height)
        ctx.fillStyle = '#161f19'
        ctx.fillRect(playhead, 0, width - playhead, height)
        ctx.fillStyle = '#202b24'
        const interval = rate / 4
        const latest = now + futureSeconds * rate
        for (let frame = Math.ceil(earliest / interval) * interval; frame <= latest; frame += interval) {
            if (frame >= 0) ctx.fillRect(x(frame), 0, 1, height)
        }
    }

    drawScore(surface, axis, heard) {
        const { ctx, width, height } = surface
        const { earliest, now, playhead, rate, x } = axis
        this.drawBackground(surface, axis)
        const future = this.notes.filter(note => note.end > now && note.frame < now + futureSeconds * rate)
        const visiblePitches = [
            ...heard.flatMap(record => record.voices.map(voice => voice.pitch)),
            ...future.map(note => note.pitch),
        ]
        const low = Math.min(33, Math.floor(Math.min(...visiblePitches)) - 1)
        const high = Math.max(71, Math.ceil(Math.max(...visiblePitches)) + 1)
        const step = height / (high - low + 1)
        const y = pitch => height - (pitch - low + 0.5) * step
        const barHeight = Math.max(3, step * 0.78)

        ctx.fillStyle = '#1a221e'
        ctx.fillRect(0, 0, labelWidth, height)
        for (let pitch = Math.max(0, low); pitch <= high; pitch++) {
            if (pitch % 12 === 0) {
                ctx.fillStyle = '#304036'
                ctx.fillRect(labelWidth, y(pitch), width - labelWidth, 1)
                ctx.fillStyle = '#a3b0a9'
                ctx.fillText(pitchLabel(pitch), 12, y(pitch))
            }
        }

        for (const note of future) {
            const left = x(Math.max(note.frame, now))
            const right = Math.min(width, x(note.end))
            ctx.fillStyle = colors[note.instrument]
            ctx.fillRect(left, y(note.pitch) - barHeight / 2, Math.max(1, right - left - 1), barHeight)
        }

        this.eachInterval(heard, axis, (record, left, right) => {
            const allocated = new Set(Object.values(record.owners))
            for (const voice of record.voices) {
                const middle = y(voice.pitch)
                const color = colors[voice.instrument]
                if (allocated.has(voice.id)) {
                    const thickness = Math.max(2, barHeight * Math.sqrt(voice.level))
                    ctx.fillStyle = color
                    ctx.globalAlpha = 0.35 + 0.65 * voice.level
                    ctx.fillRect(left, middle - thickness / 2, right - left + 0.35, thickness)
                    ctx.globalAlpha = 1
                } else {
                    ctx.strokeStyle = color
                    ctx.globalAlpha = 0.8
                    ctx.setLineDash([2, 2])
                    ctx.strokeRect(left, middle - barHeight / 2, right - left, barHeight)
                    ctx.setLineDash([])
                    ctx.globalAlpha = 1
                }
            }
        })
        // A missing trace batch is a gap rather than an inferred allocation.
        this.markGaps(ctx, heard, earliest, now, x, height)
        ctx.fillStyle = '#dce9e0'
        ctx.fillRect(playhead, 0, 1, height)
    }

    drawOwners(surface, axis, heard) {
        const { ctx, width, height } = surface
        const { earliest, now, playhead, x } = axis
        this.drawBackground(surface, axis)
        ctx.fillStyle = '#1a221e'
        ctx.fillRect(0, 0, labelWidth, height)
        const row = height / ownerLanes.length
        ownerLanes.forEach(([label], i) => {
            ctx.fillStyle = '#304036'
            ctx.fillRect(0, i * row, width, 1)
            ctx.fillStyle = '#a3b0a9'
            ctx.fillText(label, 5, i * row + row / 2)
        })

        const segments = ownerLanes.map(() => [])
        this.eachInterval(heard, axis, (record, left, right) => {
            const voices = new Map(record.voices.map(voice => [voice.id, voice]))
            ownerLanes.forEach(([, key], i) => {
                const voice = voices.get(record.owners[key])
                if (!voice) return
                const lane = segments[i]
                const previous = lane.at(-1)
                if (previous?.voice.id === voice.id && left - previous.right < 1) {
                    previous.right = right
                } else {
                    lane.push({ voice, left, right })
                }
            })
        })
        segments.forEach((lane, i) => {
            for (const { voice, left, right } of lane) {
                ctx.fillStyle = colors[voice.instrument]
                ctx.fillRect(left, i * row + 5, right - left, row - 10)
                const label = pitchLabel(Math.round(voice.pitch))
                if (right - left >= ctx.measureText(label).width + 9) {
                    ctx.fillStyle = '#191e19'
                    ctx.fillText(label, left + 4, (i + 0.5) * row)
                }
            }
        })
        for (const record of heard) {
            if (record.envelopeRestart && record.frame >= earliest) {
                ctx.fillStyle = '#ffffff'
                ctx.fillRect(x(record.frame), height - row + 3, 2, row - 6)
            }
        }
        this.markGaps(ctx, heard, earliest, now, x, height)
        ctx.fillStyle = '#dce9e0'
        ctx.fillRect(playhead, 0, 1, height)
    }

    eachInterval(heard, { earliest, now, rate, x }, draw) {
        for (let i = 0; i < heard.length; i++) {
            const record = heard[i]
            const next = heard[i + 1]
            const start = Math.max(record.frame, earliest)
            const finish = Math.min(now, next?.frame ?? now, record.frame + rate / 50)
            if (finish > start) draw(record, x(start), x(finish))
        }
    }

    markGaps(ctx, heard, earliest, now, x, height) {
        ctx.fillStyle = '#d18278'
        for (const record of heard) {
            if (record.gap && record.frame >= earliest && record.frame <= now) {
                ctx.fillRect(x(record.frame), 0, 2, height)
            }
        }
    }
}
