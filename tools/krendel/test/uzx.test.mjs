import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';

import * as core from '@strudel/core';
import * as mini from '@strudel/mini';
import * as midi from '@strudel/midi';

// What Strudel's REPL puts in scope before user code runs.
Object.assign(globalThis, core, mini, midi);

// Node has no Web MIDI: keep the port in the events instead of opening it.
core.Pattern.prototype.midi = function (port) {
    return port === undefined ? this : this.midiport(port);
};

const { cc, shapes } = await import('../uzx.mjs');

const noteFrameH = new URL('../../../src/plugins/uZX/midi_effects/NoteFrame.h', import.meta.url);

/** NoteFrame.h's controllerToStep: the step MIDI to PSG reads from a controller value. */
const controllerToStep = (controller, steps) => Math.floor((controller * steps) / 128);

function noteCCs() {
    const body = readFileSync(noteFrameH, 'utf8').match(/enum class NoteCC\s*\{([^}]*)\}/)[1];
    return Object.fromEntries([...body.matchAll(/(\w+)\s*=\s*(\d+)/g)].map(([, name, n]) => [name, Number(n)]));
}

/** The controller messages `.uzx()` sends for the first event of a pattern, as {ccn: value}. */
function sentCCs(pat) {
    const [hap] = pat.uzx().firstCycle();
    const map = midi.midicontrolMap.get('uzx');
    return Object.fromEntries(
        Object.entries(hap.value)
            .filter(([key]) => map[key])
            .map(([key, value]) => {
                const { ccn, min, max } = map[key];
                return [ccn, Math.round(((value - min) / (max - min)) * 127)];
            }),
    );
}

test('controller numbers are those of NoteCC', () => {
    const expected = noteCCs();
    assert.deepEqual(
        { ...cc, pan: 10 },
        {
            noisePeriod: expected.NoisePeriod,
            envelopeShape: expected.EnvelopeShape,
            tone: expected.Tone,
            noise: expected.Noise,
            envelope: expected.Envelope,
            envelopeRestart: expected.EnvelopeRestart,
            envelopeDriver: expected.EnvelopeDriver,
            priority: expected.Priority,
            pan: expected.Pan,
        },
    );
});

test('each shape name lands in its band of controller 70', () => {
    shapes.forEach((name, i) => {
        const value = sentCCs(core.note('c3').ayshape(name))[cc.envelopeShape];
        assert.equal(controllerToStep(value, 8), i, name);
    });
    assert.equal(sentCCs(core.note('c3').ayshape(5))[cc.envelopeShape], sentCCs(core.note('c3').ayshape('decay'))[cc.envelopeShape]);
});

test('an unknown shape name fails', () => {
    assert.throws(() => core.note('c3').ayshape('square').firstCycle(), /square/);
});

test('every noise period survives the round trip', () => {
    for (let p = 0; p < 32; ++p) {
        const value = sentCCs(core.note('c3').aynoiseperiod(p))[cc.noisePeriod];
        assert.equal(controllerToStep(value, 32), p);
    }
});

test('switches are on from 64', () => {
    const on = sentCCs(core.note('c3').aytone(1).aynoise(true).ayenv(1).aybuzz(1).ayretrig(1));
    const off = sentCCs(core.note('c3').aytone(0).aynoise(false).ayenv(0).aybuzz(0).ayretrig(0));
    for (const n of [cc.tone, cc.noise, cc.envelope, cc.envelopeDriver, cc.envelopeRestart]) {
        assert.ok(on[n] >= 64, `on ${n}`);
        assert.ok(off[n] < 64, `off ${n}`);
    }
});

test('mini-notation patterns the controls', () => {
    const values = core
        .note(mini.mini('c3*2'))
        .ayshape(mini.mini('<saw tri>')) // what Strudel makes of "<saw tri>"
        .uzx()
        .queryArc(0, 2)
        .map((hap) => hap.value.ayshape);
    assert.deepEqual(values.map((v) => controllerToStep(v, 8)), [2, 2, 0, 0]);
});

test('Strudel pan asks for a place, priority passes through', () => {
    const sent = sentCCs(core.note('c3').pan(1).ayprio(100));
    assert.equal(sent[10], 127);
    assert.equal(sent[cc.priority], 100);
});

test('uzx sends to the given port through the uzx map', () => {
    const [hap] = core.note('c3').uzx('IAC Driver Bus 1').firstCycle();
    assert.equal(hap.value.midiport, 'IAC Driver Bus 1');
    assert.equal(hap.value.midimap, 'uzx');
});

test('controls also work as functions', () => {
    const sent = sentCCs(globalThis.ayshape('saw').note('c3'));
    assert.equal(controllerToStep(sent[cc.envelopeShape], 8), 2);
});
