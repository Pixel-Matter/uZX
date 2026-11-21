const QUEUE_CAPACITY = 4096;
const TRACE_WORDS = 74;
const TRACE_BATCH_CAPACITY = 256;
const TRACE_IN_FLIGHT_CAPACITY = 2;

class UZXChipProcessor extends AudioWorkletProcessor {
    constructor(options) {
        super();
        this.wasmModule = options.processorOptions?.wasmModule;
        this.isYm = options.processorOptions?.isYm ? 1 : 0;
        this.engine = null;
        this.memory = null;
        this.heapFloat = null;
        this.heapUint = null;
        this.failed = false;
        this.activeGeneration = null;
        this.engineBaseFrame = 0;
        this.queue = new Array(QUEUE_CAPACITY);
        this.queueHead = 0;
        this.queueSize = 0;
        this.droppedCommands = 0;
        this.rejectedNotes = 0;
        this.lateCommands = 0;
        this.traceWords = new Uint32Array(TRACE_BATCH_CAPACITY * TRACE_WORDS);
        this.traceCount = 0;
        this.traceBaseFrame = 0;
        this.traceGapPending = false;
        this.inFlightTraceIds = new Uint32Array(TRACE_IN_FLIGHT_CAPACITY);
        this.nextTraceBatchId = 1;
        this.lastTracePostFrame = 0;
        this.transportStopped = false;
        this.disposed = false;
        this.port.onmessage = event => this.receive(event.data);
        this.loadWasm();
    }

    loadWasm() {
        try {
            if (!(this.wasmModule instanceof WebAssembly.Module))
                throw new Error("The page did not provide a compiled WASM module");
            this.engine = new WebAssembly.Instance(this.wasmModule, {}).exports;
            this.memory = this.engine.memory;
            if (!this.memory)
                throw new Error("The WASM module does not export linear memory");
            this.heapFloat = new Float32Array(this.memory.buffer);
            this.heapUint = new Uint32Array(this.memory.buffer);
            this.initializeEngine();
            this.port.postMessage({type: "ready", sampleRate});
        } catch (error) {
            this.failed = true;
            this.port.postMessage({type: "error", message: String(error?.message ?? error)});
        }
    }

    initializeEngine() {
        if (this.engine._initialize)
            this.engine._initialize();
        if (!this.engine.uzx_init(sampleRate, this.isYm))
            throw new Error("WASM engine initialization failed");
        this.engineBaseFrame = this.nowFrame();
        this.port.postMessage({type: "initialized", sampleRate});
    }

    nowFrame() {
        return currentFrame;
    }

    receive(message) {
        if (!message || typeof message.type !== "string")
            return;
        if (message.type === "commands") {
            const commands = Array.isArray(message.commands) ? message.commands : [];
            for (const command of commands)
                this.enqueue(command);
        } else if (message.type === "stop") {
            this.clearQueue();
            this.publishTrace(this.nowFrame(), true);
            this.resetEngine(this.nowFrame());
            this.activeGeneration = null;
            this.transportStopped = true;
            this.port.postMessage({type: "stopped", frame: this.nowFrame()});
        } else if (message.type === "reset") {
            this.clearQueue();
            this.publishTrace(this.nowFrame(), true);
            this.resetEngine(this.nowFrame());
            this.activeGeneration = message.generation ?? null;
            this.transportStopped = false;
            this.disposed = false;
        } else if (message.type === "dispose") {
            this.clearQueue();
            this.publishTrace(this.nowFrame(), true);
            this.resetEngine(this.nowFrame());
            this.transportStopped = true;
            this.disposed = true;
            this.port.postMessage({type: "disposed"});
        } else if (message.type === "trace-ack") {
            const index = this.inFlightTraceIds.indexOf(message.sequence >>> 0);
            if (index >= 0)
                this.inFlightTraceIds[index] = 0;
        }
    }

    enqueue(command) {
        if (this.transportStopped)
            return;
        if (!command || !Number.isFinite(command.frame) || !Number.isFinite(command.generation))
            return;
        if (this.queueSize === QUEUE_CAPACITY) {
            this.droppedCommands++;
            this.clearQueue();
            this.publishTrace(this.nowFrame(), true);
            this.resetEngine(this.nowFrame());
            this.activeGeneration = command.generation;
            this.port.postMessage({type: "overflow", dropped: this.droppedCommands, frame: this.nowFrame()});
            return;
        }
        const item = {...command, frame: Math.trunc(command.frame)};
        let index = this.queueSize;
        while (index > 0) {
            const previous = this.at(index - 1);
            if (previous.frame < item.frame || (previous.frame === item.frame && this.priority(previous) <= this.priority(item)))
                break;
            this.queue[this.slot(index)] = previous;
            index--;
        }
        this.queue[this.slot(index)] = item;
        this.queueSize++;
    }

    priority(command) {
        return command.switch ? 0 : command.gate === undefined ? 2 : 1;
    }

    slot(index) {
        return (this.queueHead + index) % QUEUE_CAPACITY;
    }

    at(index) {
        return this.queue[this.slot(index)];
    }

    pop() {
        const item = this.queue[this.queueHead];
        this.queue[this.queueHead] = undefined;
        this.queueHead = (this.queueHead + 1) % QUEUE_CAPACITY;
        this.queueSize--;
        return item;
    }

    clearQueue() {
        this.queue.fill(undefined);
        this.queueHead = 0;
        this.queueSize = 0;
    }

    resetEngine(frame) {
        if (this.engine && !this.failed) {
            this.engine.uzx_reset();
            this.engineBaseFrame = frame;
        }
    }

    apply(command, frame) {
        if (command.frame < frame)
            this.lateCommands++;
        if (command.switch) {
            const newGeneration = command.generation;
            const oldGeneration = this.activeGeneration;
            this.publishTrace(frame, true);
            this.resetEngine(frame);
            this.activeGeneration = newGeneration;
            if (oldGeneration !== null)
                this.discardGeneration(oldGeneration);
            return;
        }
        if (this.activeGeneration === null)
            this.activeGeneration = command.generation;
        if (command.generation !== this.activeGeneration)
            return;
        if (command.gate) {
            // A gate without a sound plays the engine's default, a plain tone.
            if (command.sound)
                this.heapFloat.set(command.sound, (this.engine.uzx_note_sound() >>> 0) >>> 2);
            if (!this.engine.uzx_note_on(command.occurrence >>> 0, command.instrument >>> 0,
                Number(command.pitch) || 0, Math.max(0, Math.min(1, Number(command.velocity) || 0))))
                this.rejectedNotes++;
        } else if (command.gate === false) {
            this.engine.uzx_note_off(command.occurrence >>> 0);
        }
    }

    discardGeneration(generation) {
        let writeIndex = 0;
        for (let readIndex = 0; readIndex < this.queueSize; readIndex++) {
            const item = this.at(readIndex);
            if (item.generation !== generation || item.switch)
                this.queue[this.slot(writeIndex++)] = item;
        }
        for (let i = writeIndex; i < this.queueSize; i++)
            this.queue[this.slot(i)] = undefined;
        this.queueSize = writeIndex;
    }

    render(count, output, outputOffset) {
        this.engine.uzx_render(count);
        const leftIndex = (this.engine.uzx_audio_left() >>> 0) >>> 2;
        const rightIndex = (this.engine.uzx_audio_right() >>> 0) >>> 2;
        for (let i = 0; i < count; i++) {
            if (output[0])
                output[0][outputOffset + i] = this.heapFloat[leftIndex + i];
            if (output[1])
                output[1][outputOffset + i] = this.heapFloat[rightIndex + i];
        }
        this.collectTrace();
    }

    collectTrace() {
        const count = this.engine.uzx_trace_count() >>> 0;
        if (count === 0)
            return;
        const pointer = this.engine.uzx_trace_data() >>> 0;
        if (this.traceCount === 0)
            this.traceBaseFrame = this.engineBaseFrame;
        const sourceStart = pointer >>> 2;
        for (let i = 0; i < count; i++) {
            if (this.traceCount === TRACE_BATCH_CAPACITY) {
                this.traceWords.copyWithin(0, TRACE_WORDS);
                this.traceCount--;
                this.traceWords[9] = 1;
            }
            const target = this.traceCount * TRACE_WORDS;
            const source = sourceStart + i * TRACE_WORDS;
            for (let word = 0; word < TRACE_WORDS; word++)
                this.traceWords[target + word] = this.heapUint[source + word];
            if (this.traceGapPending) {
                this.traceWords[target + 9] = 1;
                this.traceGapPending = false;
            }
            this.traceCount++;
        }
        this.engine.uzx_trace_clear();
    }

    publishTrace(frame, force = false) {
        if (this.traceCount === 0)
            return;
        const slot = this.inFlightTraceIds.indexOf(0);
        if (slot < 0) {
            this.traceCount = 0;
            this.traceGapPending = true;
            return;
        }
        if (!force && frame - this.lastTracePostFrame < sampleRate / 30)
            return;
        const length = this.traceCount * TRACE_WORDS;
        const batch = this.traceWords.slice(0, length);
        const sequence = this.nextTraceBatchId++ >>> 0 || this.nextTraceBatchId++ >>> 0;
        this.inFlightTraceIds[slot] = sequence;
        this.port.postMessage({
            type: "trace",
            sequence,
            generation: this.activeGeneration,
            baseFrame: this.traceBaseFrame,
            records: batch.buffer,
            count: this.traceCount,
            lateCommands: this.lateCommands,
            droppedCommands: this.droppedCommands,
            rejectedNotes: this.rejectedNotes,
        }, [batch.buffer]);
        this.traceCount = 0;
        this.lastTracePostFrame = frame;
    }

    process(_inputs, outputs) {
        const output = outputs[0] ?? [];
        const length = output[0]?.length ?? 128;
        for (const channel of output)
            channel.fill(0);
        if (this.disposed)
            return false;
        if (this.transportStopped || this.failed || !this.engine)
            return true;
        const blockStart = this.nowFrame();

        let offset = 0;
        while (offset < length) {
            const absoluteFrame = blockStart + offset;
            while (this.queueSize > 0 && this.at(0).frame <= absoluteFrame)
                this.apply(this.pop(), absoluteFrame);

            let count = Math.min(128, length - offset);
            if (this.queueSize > 0) {
                const untilNext = this.at(0).frame - absoluteFrame;
                if (untilNext > 0)
                    count = Math.min(count, untilNext);
            }
            if (count > 0) {
                this.render(count, output, offset);
                offset += count;
            }
        }
        this.publishTrace(blockStart + length);
        return true;
    }
}

registerProcessor("uzx-chip", UZXChipProcessor);
