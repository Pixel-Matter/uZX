const baseUrl = import.meta.env?.BASE_URL ?? "/";
let modulePromise;

function loadEngineModule() {
    if (!modulePromise) {
        modulePromise = fetch(`${baseUrl}engine.wasm`).then(response => {
            if (!response.ok)
                throw new Error(`WASM request failed (${response.status})`);
            return response.arrayBuffer();
        }).then(bytes => WebAssembly.compile(bytes)).catch(error => {
            modulePromise = null;
            throw error;
        });
    }
    return modulePromise;
}

/** Page-side transport for the single WASM AY/YM engine in the AudioWorklet. */
export class ChipAudio {
    constructor() {
        this.context = null;
        this.node = null;
        this.ready = null;
        this.workletLoaded = false;
        this.startToken = 0;
        this.sampleRate = 0;
        this.isYm = false;
        this.onTrace = null;
        this.onStatus = null;
        this.onError = null;
        this.stopped = true;
        this.startGeneration = null;
    }

    async start({isYm = false, generation = null} = {}) {
        let token = ++this.startToken;
        if (!this.context) {
            this.context = new AudioContext({latencyHint: "interactive"});
        }
        if (this.node && !this.stopped && isYm !== this.isYm) {
            await this.stop();
            token = ++this.startToken;
        }
        if (token !== this.startToken)
            return this;
        if (this.node && !this.stopped)
            return this;

        try {
            if (!this.node || isYm !== this.isYm) {
                const wasmModule = await loadEngineModule();
                if (token !== this.startToken)
                    return this;
                if (!this.workletLoaded) {
                    await this.context.audioWorklet.addModule(new URL("./processor.js", import.meta.url));
                    this.workletLoaded = true;
                    if (token !== this.startToken)
                        return this;
                }
                await this.disposeNode();
                this.node = new AudioWorkletNode(this.context, "uzx-chip", {
                    numberOfInputs: 0,
                    numberOfOutputs: 1,
                    outputChannelCount: [2],
                    processorOptions: {wasmModule, isYm},
                });
                this.node.port.onmessage = event => this.handleMessage(event.data);
                this.node.connect(this.context.destination);
                this.isYm = isYm;
                this.ready = new Promise((resolve, reject) => {
                    this.resolveReady = resolve;
                    this.rejectReady = reject;
                });
                await this.ready;
                if (token !== this.startToken) {
                    this.node.port.postMessage({type: "stop"});
                    return this;
                }
            }
            await this.context.resume();
            if (token !== this.startToken) {
                await this.stop();
                return this;
            }
            this.sampleRate = this.context.sampleRate;
            this.stopped = false;
            this.startGeneration = generation;
            this.node.port.postMessage({type: "reset", generation});
            this.onStatus?.({type: "started", sampleRate: this.sampleRate});
            return this;
        } catch (error) {
            if (token === this.startToken) {
                await this.disposeNode();
                this.stopped = true;
                if (this.context?.state === "running")
                    await this.context.suspend();
            }
            throw error;
        }
    }

    /** Submit scheduled gates and generation switches using absolute AudioContext sample frames. */
    submit(commands) {
        if (!this.node || this.stopped || !Array.isArray(commands) || commands.length === 0)
            return false;
        this.node.port.postMessage({type: "commands", commands});
        return true;
    }

    async stop() {
        this.startToken++;
        if (!this.context)
            return;
        this.stopped = true;
        if (this.node) {
            const stopped = new Promise(resolve => { this.resolveStopped = resolve; });
            this.node.port.postMessage({type: "stop"});
            await stopped;
        }
        if (this.context.state === "running")
            await this.context.suspend();
        this.onStatus?.({type: "stopped", frame: this.currentFrame});
    }

    async disposeNode() {
        if (this.node) {
            const disposed = new Promise(resolve => { this.resolveDisposed = resolve; });
            this.node.port.postMessage({type: "dispose"});
            await disposed;
        }
        this.node?.disconnect();
        this.node?.port.close();
        this.node = null;
        this.ready = null;
        this.resolveReady = null;
        this.rejectReady = null;
        this.resolveDisposed = null;
    }

    get currentFrame() {
        return this.context ? Math.floor(this.context.currentTime * this.context.sampleRate) : 0;
    }

    get outputFrame() {
        if (!this.context)
            return 0;
        const latency = (this.context.baseLatency ?? 0) + (this.context.outputLatency ?? 0);
        return Math.max(0, Math.floor((this.context.currentTime - latency) * this.context.sampleRate));
    }

    handleMessage(message) {
        if (!message)
            return;
        if (message.type === "ready") {
            this.sampleRate = message.sampleRate;
            this.resolveReady?.(this);
            this.resolveReady = null;
        } else if (message.type === "error") {
            const error = new Error(message.message);
            this.rejectReady?.(error);
            this.rejectReady = null;
            this.resolveStopped?.();
            this.resolveStopped = null;
            this.onError?.(error);
        } else if (message.type === "stopped") {
            this.resolveStopped?.();
            this.resolveStopped = null;
        } else if (message.type === "disposed") {
            this.resolveDisposed?.();
            this.resolveDisposed = null;
        } else if (message.type === "trace") {
            try {
                this.onTrace?.(decodeTrace(message));
            } finally {
                this.node?.port.postMessage({type: "trace-ack", sequence: message.sequence});
            }
        } else {
            this.onStatus?.(message);
        }
    }
}

function decodeTrace(message) {
    const words = new Uint32Array(message.records);
    const floats = new Float32Array(message.records);
    const traces = new Array(message.count);
    for (let index = 0; index < message.count; index++) {
        const base = index * 74;
        const voiceCount = Math.min(words[base + 2], 16);
        const voices = new Array(voiceCount);
        for (let voice = 0; voice < voiceCount; voice++) {
            const offset = base + 10 + voice * 4;
            voices[voice] = {
                id: words[offset],
                instrument: words[offset + 1],
                pitch: floats[offset + 2],
                level: floats[offset + 3],
            };
        }
        traces[index] = {
            generation: message.generation,
            frame: message.baseFrame + words[base] + words[base + 1] * 0x100000000,
            voices,
            owners: {a: words[base + 3], b: words[base + 4], c: words[base + 5], noise: words[base + 6], envelope: words[base + 7]},
            envelopeRestart: words[base + 8] !== 0,
            gap: words[base + 9] !== 0,
        };
    }
    return {traces, lateCommands: message.lateCommands, droppedCommands: message.droppedCommands};
}
