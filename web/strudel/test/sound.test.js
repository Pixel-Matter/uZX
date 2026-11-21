import test from "node:test";
import assert from "node:assert/strict";

import {noteSound, ornamentSemitones, presetNames, shapes, soundLength, withPreset} from "../sound.js";

const context = {pitch: 60, cps: 0.5};
const fields = [
    "tone", "noise", "envelope", "drivesEnvelope", "restartEnvelope", "envelopeShape", "noisePeriod", "noiseHold",
    "priority", "pan", "attack", "decay", "sustain", "release", "pitchStart", "pitchPeak", "pitchAttack", "pitchDecay",
    "vibratoRate", "vibratoDepth", "vibratoDelay", "ornamentStep", "ornamentLength",
];

/** The record's named fields, rounded to float precision, and its ornament. */
function read(value, extra = {}) {
    const {instrument, controls} = withPreset(value);
    const sound = noteSound(controls, {...context, ...extra});
    assert.equal(sound.length, soundLength);
    const named = Object.fromEntries(fields.map((name, index) => [name, Math.round(sound[index] * 1e4) / 1e4]));
    return {...named, instrument, ornament: [...sound.slice(fields.length, fields.length + named.ornamentLength)]};
}

test("a note without controls is a plain bass tone", () => {
    const sound = read({note: "c4"});
    assert.deepEqual([sound.instrument, sound.tone, sound.noise, sound.envelope, sound.priority], [0, 1, 0, 0, 2]);
    assert.deepEqual([sound.attack, sound.decay, sound.sustain, sound.release, sound.pan], [0, 0, 1, 0, -1]);
});

test("a preset's controls yield to the note's own", () => {
    const pluck = read({s: "pluck"});
    assert.deepEqual([pluck.instrument, pluck.noise, pluck.noiseHold, pluck.noisePeriod, pluck.decay, pluck.sustain],
        [1, 1, 0.04, 5, 0.14, 0]);
    assert.equal(read({s: "pluck", aynoise: 0}).noise, 0);
    assert.throws(() => read({s: "piano"}), /Unknown AY\/YM instrument "piano"/);
});

test("a drum has a pitch of its own, which the note's pitch replaces", () => {
    assert.equal(withPreset({s: "kick"}).controls.note, "e2");
    assert.equal(withPreset({s: "kick", note: "c2"}).controls.note, "c2");
    const snare = read({s: "snare"});
    assert.deepEqual([snare.instrument, snare.noise, snare.noiseHold, snare.priority], [3, 1, 0, 3]);
    // A name's place is its tag in the trace and its color in the piano roll.
    assert.deepEqual(presetNames, ["bass", "pluck", "kick", "snare"]);
});

test("chip controls set the timbre", () => {
    const sound = read({aytone: 0, ayenv: 1, aybuzz: 1, ayretrig: 1, ayshape: "saw", aynoiseperiod: 12, pan: 0.5});
    assert.deepEqual([sound.tone, sound.envelope, sound.drivesEnvelope, sound.restartEnvelope], [0, 1, 1, 1]);
    assert.deepEqual([sound.envelopeShape, sound.noisePeriod, sound.pan], [shapes.indexOf("saw"), 12, 0.5]);
    assert.equal(read({ayshape: 7}).envelopeShape, 7);
    assert.throws(() => read({ayshape: "sine"}), /ayshape/);
    assert.throws(() => read({decay: "long"}), /not a number/);
});

test("a decay falls to silence unless a sustain level is set", () => {
    assert.equal(read({decay: 0.2}).sustain, 0);
    assert.equal(read({decay: 0.2, sustain: 0.6}).sustain, 0.6);
    assert.equal(read({attack: 0.1}).sustain, 1);
});

test("the pitch envelope reads as Strudel's penv", () => {
    const out = read({penv: 12, pdecay: 0.06});
    assert.deepEqual([out.pitchStart, out.pitchPeak, out.pitchAttack, out.pitchDecay], [0, 12, 0, 0.06]);
    const arrive = read({penv: 5, pattack: 0.3});
    assert.deepEqual([arrive.pitchStart, arrive.pitchPeak, arrive.pitchAttack, arrive.pitchDecay], [-5, 0, 0.3, 0]);
    assert.equal(read({pattack: 0.3}).pitchAttack, 0);
});

test("vibrato has Strudel's default depth and its own delay", () => {
    const sound = read({vib: 6, vibdelay: 0.15});
    assert.deepEqual([sound.vibratoRate, sound.vibratoDepth, sound.vibratoDelay], [6, 0.5, 0.15]);
});

test("an ornament step is in cycles, its offsets in scale steps when the note has a scale", () => {
    const semitones = read({orn: [0, 3, 7], ornstep: 1 / 32});
    assert.deepEqual(semitones.ornament, [0, 3, 7]);
    assert.equal(semitones.ornamentStep, 0.0625);
    assert.equal(read({orn: 12}).ornamentStep, 0);
    assert.deepEqual(read({orn: 12}).ornament, [12]);
    assert.throws(() => read({orn: new Array(17).fill(0)}), /at most 16/);

    // B minor: B C# D E F# G A. The same steps make a minor triad on B and a major one on D.
    assert.deepEqual(read({orn: [0, 2, 4]}, {pitch: 59, scale: "B3 minor"}).ornament, [0, 3, 7]);
    assert.deepEqual(read({orn: [0, 2, 4]}, {pitch: 62, scale: "B3 minor"}).ornament, [0, 4, 7]);
});

test("scale steps cross octaves both ways and fall back to semitones off the scale", () => {
    assert.deepEqual(ornamentSemitones([7, -1, 9], 60, "C major"), [12, -1, 16]);
    assert.deepEqual(ornamentSemitones([0, 2], 61, "C major"), [0, 2]);
    assert.deepEqual(ornamentSemitones([0, 2], 60, "C nonsense"), [0, 2]);
    assert.deepEqual(ornamentSemitones([0, 2], 60, undefined), [0, 2]);
    assert.deepEqual(ornamentSemitones([0, 1], 65, "F#2 major"), [0, 1]);
    assert.deepEqual(ornamentSemitones([0, 1], 66, "F#2 major"), [0, 2]);
});
