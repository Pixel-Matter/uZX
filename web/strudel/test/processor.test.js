import test from "node:test";
import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
import {fileURLToPath} from "node:url";

import {noteSound, withInstrument} from "../sound.js";

const wasmPath = fileURLToPath(new URL("../public/engine.wasm", import.meta.url));
const wasmModule = await WebAssembly.compile(await readFile(wasmPath));

let WorkletProcessor;
let registeredName;

class WorkletHost {
    constructor() {
        this.port = {
            onmessage: null,
            messages: [],
            postMessage: message => this.port.messages.push(message),
        };
    }
}

globalThis.AudioWorkletProcessor = WorkletHost;
globalThis.sampleRate = 48000;
globalThis.currentFrame = 0;
globalThis.registerProcessor = (name, constructor) => {
    registeredName = name;
    WorkletProcessor = constructor;
};

await import("../processor.js");

function makeProcessor(acknowledge = true) {
    globalThis.currentFrame = 0;
    assert.equal(registeredName, "uzx-chip");
    const processor = new WorkletProcessor({processorOptions: {wasmModule, isYm: false}});
    assert.ok(processor.port.messages.some(message => message.type === "ready"));
    assert.ok(processor.port.messages.some(message => message.type === "initialized"));
    if (acknowledge) {
        const post = processor.port.postMessage;
        processor.port.postMessage = message => {
            post(message);
            if (message.type === "trace")
                send(processor, {type: "trace-ack", sequence: message.sequence});
        };
    }
    return processor;
}

function send(processor, data) {
    processor.port.onmessage({data});
}

function commands(processor, entries) {
    send(processor, {type: "commands", commands: entries});
}

function processBlock(processor, frame) {
    globalThis.currentFrame = frame;
    const left = new Float32Array(128);
    const right = new Float32Array(128);
    assert.equal(processor.process([], [[left, right]]), true);
    return {left, right};
}

function processRange(processor, start, end) {
    let signal = false;
    for (let frame = start; frame < end; frame += 128)
        signal = hasSignal(processBlock(processor, frame)) || signal;
    return signal;
}

function hasSignal(block) {
    return block.left.some(sample => Math.abs(sample) > 1e-7)
        && block.right.some(sample => Math.abs(sample) > 1e-7);
}

function traceMessages(processor, generation) {
    return processor.port.messages.filter(message => message.type === "trace" && message.generation === generation);
}

function traceRecords(messages) {
    const result = [];
    for (const message of messages) {
        const words = new Uint32Array(message.records);
        for (let record = 0; record < message.count; record++) {
            const offset = record * 74;
            const count = words[offset + 2];
            const ids = [];
            for (let voice = 0; voice < count; voice++)
                ids.push(words[offset + 10 + voice * 4]);
            result.push({
                sample: BigInt(words[offset]) | (BigInt(words[offset + 1]) << 32n),
                count,
                ids,
                channels: [...words.slice(offset + 3, offset + 6)],
            });
        }
    }
    return result.sort((a, b) => a.sample < b.sample ? -1 : a.sample > b.sample ? 1 : 0);
}

test("production worklet switches generations, preserves same-frame order, stops and resumes", () => {
    const processor = makeProcessor();

    commands(processor, [{
        frame: 0, generation: 1, gate: true, occurrence: 101, instrument: 0, pitch: 45, velocity: 0.8,
    }]);
    assert.ok(processRange(processor, 0, 512), "initial bass gate should produce stereo PCM across blocks");

    // A switch at the due frame outranks and discards a stale command from the outgoing generation.
    // The valid incoming-generation gate starts in that same 128-sample worklet quantum.
    commands(processor, [
        {frame: 512, generation: 1, gate: true, occurrence: 102, instrument: 0, pitch: 52, velocity: 0.8},
        {frame: 512, generation: 2, switch: true},
        {frame: 512, generation: 2, gate: true, occurrence: 201, instrument: 0, pitch: 60, velocity: 0.8},
    ]);
    assert.ok(processRange(processor, 512, 1024), "generation switch should start the incoming voice");
    const outgoing = traceRecords(traceMessages(processor, 1));
    assert.equal(outgoing.length, 1);
    assert.deepEqual(outgoing[0].ids, [101]);

    globalThis.currentFrame = 1024;
    send(processor, {type: "stop"});
    const stoppedTrace = traceRecords(traceMessages(processor, 2));
    assert.equal(stoppedTrace.length, 1);
    assert.deepEqual(stoppedTrace[0].ids, [201]);
    commands(processor, [{
        frame: 1024, generation: 2, gate: true, occurrence: 202, instrument: 0, pitch: 45, velocity: 0.8,
    }]);
    const stopped = processBlock(processor, 1024);
    assert.ok(stopped.left.every(sample => sample === 0) && stopped.right.every(sample => sample === 0),
        "stop must leave the worklet output silent and ignore queued gates");

    // Reset resumes command acceptance. Same-frame on then off must keep the gate only through its first tick.
    globalThis.currentFrame = 1152;
    send(processor, {type: "reset", generation: 3});
    commands(processor, [{
        frame: 1152, generation: 3, gate: true, occurrence: 301, instrument: 0, pitch: 48, velocity: 0.8,
    }]);
    assert.ok(processRange(processor, 1152, 1664), "reset should allow a fresh generation to resume audio");

    commands(processor, [
        {frame: 1664, generation: 3, gate: true, occurrence: 302, instrument: 0, pitch: 60, velocity: 0.8},
        {frame: 1664, generation: 3, gate: false, occurrence: 302},
    ]);
    processRange(processor, 1664, 3200);
    globalThis.currentFrame = 3200;
    send(processor, {type: "stop"});

    const resumedTrace = traceRecords(traceMessages(processor, 3));
    assert.deepEqual(resumedTrace.map(record => Number(record.sample)), [0, 960, 1920]);
    assert.deepEqual(resumedTrace[0].ids, [301]);
    assert.deepEqual(resumedTrace[1].ids, [301, 302], "same-frame gate-on then gate-off sounds its first frame");
    assert.deepEqual(resumedTrace[2].ids, [301], "same-frame gate-off releases the occurrence next frame");

    send(processor, {type: "reset", generation: 4});
    commands(processor, [{
        frame: 3200, generation: 4, gate: true, occurrence: 401, instrument: 1, pitch: 67, velocity: 0.9,
    }]);
    assert.ok(processRange(processor, 3200, 3712), "reset can resume with a new pluck generation");
});

test("a WASM module older than the worklet is reported, not played", () => {
    // A module that exports its memory and nothing else.
    const stale = new WebAssembly.Module(new Uint8Array([
        0, 97, 115, 109, 1, 0, 0, 0, 5, 3, 1, 0, 1, 7, 10, 1, 6, 109, 101, 109, 111, 114, 121, 2, 0,
    ]));
    const processor = new WorkletProcessor({processorOptions: {wasmModule: stale, isYm: false}});
    const error = processor.port.messages.find(message => message.type === "error");
    assert.match(error.message, /engine\.wasm is out of date: it has no uzx_init, .*uzx_note_sound/);
    assert.ok(!processor.port.messages.some(message => message.type === "ready"));
    assert.equal(processor.process([], [[new Float32Array(128), new Float32Array(128)]]), true);
});

test("a stalled display drops trace batches while audio continues", () => {
    const processor = makeProcessor(false);
    commands(processor, [{
        frame: 0, generation: 1, gate: true, occurrence: 42, instrument: 0, pitch: 69, velocity: 1,
    }]);
    processRange(processor, 0, 8192);
    const pending = traceMessages(processor, 1);
    assert.equal(pending.length, 2);
    assert.ok(processRange(processor, 8192, 12288));
    assert.equal(traceMessages(processor, 1).length, 2);

    send(processor, {type: "trace-ack", sequence: pending[0].sequence});
    processRange(processor, 12288, 14336);
    const resumed = traceMessages(processor, 1);
    assert.equal(resumed.length, 3);
    assert.equal(new Uint32Array(resumed[2].records)[9], 1, "mark discontinuity on the next delivered trace");
});

test("a gate's sound reaches the engine, and the next gate without one is a plain tone", () => {
    const processor = makeProcessor();
    const sound = noteSound({noise: 1, pan: 1}, {pitch: 60, fps: 50});
    commands(processor, [
        {frame: 0, generation: 1, gate: true, occurrence: 7, instrument: 0, pitch: 60, velocity: 0.8, sound},
        {frame: 0, generation: 1, gate: true, occurrence: 8, instrument: 0, pitch: 64, velocity: 0.8},
    ]);
    processRange(processor, 0, 512);
    globalThis.currentFrame = 512;
    send(processor, {type: "stop"});

    const message = traceMessages(processor, 1)[0];
    const [a, b, c, noise] = new Uint32Array(message.records).slice(3, 7);
    assert.deepEqual([a, b, c, noise], [8, 0, 7, 7], "the noisy note takes the right channel and the noise");
});

test("a generation brings its frame rate, and a sound of another size than the engine's is refused", () => {
    const processor = makeProcessor();
    commands(processor, [
        {frame: 0, generation: 1, switch: true, frameRate: 100},
        {frame: 0, generation: 1, gate: true, occurrence: 1, instrument: 0, pitch: 60, velocity: 1,
            sound: noteSound({decay: 0.03}, {pitch: 60, fps: 100})},
    ]);
    processRange(processor, 0, 2048);
    commands(processor, [{frame: 2048, generation: 2, switch: true}]);
    processRange(processor, 2048, 2560);
    const frames = traceRecords(traceMessages(processor, 1));
    assert.deepEqual(frames.map(record => [Number(record.sample), record.count]),
        [[0, 1], [480, 1], [960, 1], [1440, 0], [1920, 0]], "a frame is 480 samples, and the decay lasts three");

    commands(processor, [{
        frame: 2560, generation: 2, gate: true, occurrence: 2, instrument: 0, pitch: 60, velocity: 1,
        sound: new Float32Array(39),
    }]);
    processBlock(processor, 2560);
    const error = processor.port.messages.find(message => message.type === "error");
    assert.match(error.message, /engine\.wasm is out of date: its note record has another size/);
});

test("a kick takes the channel it asks for from the lowest priority and gives it back", () => {
    const processor = makeProcessor();
    const instruments = {kick: {note: "e2", decay: 0.12, prio: 2}};
    const gate = (occurrence, value, pitch) => {
        const {instrument, controls} = withInstrument(value, instruments);
        const sound = noteSound(controls, {pitch, fps: 50});
        return {frame: 0, generation: 1, gate: true, occurrence, instrument, pitch, velocity: 0.8, sound};
    };
    commands(processor, [
        gate(1, {pan: 0.5}, 35),
        gate(2, {pan: 0, prio: 1}, 59),
        gate(3, {pan: 1, prio: 1}, 66),
        gate(4, {s: "kick", pan: 0.5}, 40),
    ]);
    processRange(processor, 0, 9600);
    globalThis.currentFrame = 9600;
    send(processor, {type: "stop"});

    const records = traceRecords(traceMessages(processor, 1));
    assert.deepEqual(records.map(record => record.channels[1]), [4, 4, 4, 4, 4, 4, 1, 1, 1, 1],
        "the kick holds channel B for its six frames, then the bass has it again");
    assert.ok(records.every(record => record.channels[0] === 2 && record.channels[2] === 3));
});
