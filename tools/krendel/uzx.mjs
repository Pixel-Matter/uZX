// Play μZX Studio's AY chip from Strudel over MIDI.
//
// In Strudel:
//
//   await import('<where this file is served>/uzx.mjs')
//
//   $: note("c4 eb4 g4 bb4").midichan(1).uzx('IAC Driver Bus 1')
//   $: note("<c2 g1>").midichan(3).aytone(0).ayenv(1).aybuzz(1).ayshape("<saw tri>").uzx('IAC Driver Bus 1')
//
// The controls are the controllers MIDI to PSG reads (`NoteCC` in src/plugins/uZX/midi_effects/NoteFrame.h),
// prefixed with `ay` because Strudel already has `noise`, `shape`, `env` and `retrig`. Each is sent on the note's
// MIDI channel just before the note.
//
// Tested with @strudel/core 1.2.6, @strudel/mini 1.2.6 and @strudel/midi 1.3.0 (see package.json).

const { registerControl, midimaps, reify, Pattern } = globalThis;

if (!registerControl || !midimaps || !reify || !Pattern) {
    throw new Error('uzx.mjs: load it from Strudel, which provides registerControl, midimaps, reify and Pattern');
}

/** Controller numbers of MIDI to PSG. */
export const cc = {
    noisePeriod: 2,
    envelopeShape: 70,
    tone: 80,
    noise: 81,
    envelope: 82,
    envelopeRestart: 83,
    envelopeDriver: 85,
    priority: 86,
};

/** Envelope shapes in MIDI to PSG's order: controller 70 splits its range into these eight. */
export const shapes = ['tri', 'trifall', 'saw', 'sawfall', 'attack', 'decay', 'attackcut', 'decayhold'];

const numNoisePeriods = 32;

// MIDI to PSG reads step `i` of `steps` from any controller value in its band; send the value
// NoteFrame.h's stepToController sends.
const stepToController = (i, steps) => Math.floor((i * 127) / (steps - 1));

const clampStep = (x, steps) => Math.min(steps - 1, Math.max(0, Math.round(Number(x))));

const toSwitch = (x) => (x === true || Number(x) >= 0.5 ? 127 : 0);

function toShape(x) {
    const i = typeof x === 'string' && Number.isNaN(Number(x)) ? shapes.indexOf(x) : clampStep(x, shapes.length);
    if (i < 0) {
        throw new Error(`ayshape: "${x}" is not one of ${shapes.join(', ')} or 0–7`);
    }
    return stepToController(i, shapes.length);
}

const toNoisePeriod = (x) => stepToController(clampStep(x, numNoisePeriods), numNoisePeriods);

const toController = (x) => Math.min(127, Math.max(0, Math.round(Number(x))));

/** Each control: its controller, and how a Strudel value becomes the controller's value. */
const controls = {
    aytone: [cc.tone, toSwitch],           // 1 or 0
    aynoise: [cc.noise, toSwitch],         // 1 or 0
    ayenv: [cc.envelope, toSwitch],        // 1 or 0
    aybuzz: [cc.envelopeDriver, toSwitch], // 1: the notes' pitch is the envelope's
    ayretrig: [cc.envelopeRestart, toSwitch], // 1 restarts the envelope with the note
    ayshape: [cc.envelopeShape, toShape],  // a name from `shapes`, or 0–7
    aynoiseperiod: [cc.noisePeriod, toNoisePeriod], // 0 highest noise … 31 lowest
    ayprio: [cc.priority, toController],   // 0–127
};

const midimap = {};

for (const [name, [ccn, convert]] of Object.entries(controls)) {
    const control = registerControl(name)[name];
    const converted = (value) => reify(value).fmap(convert);
    const method = Pattern.prototype[name];

    globalThis[name] = (value, pat) => control(converted(value), pat);
    Pattern.prototype[name] = function (value) {
        return method.call(this, converted(value));
    };
    midimap[name] = { ccn, min: 0, max: 127 };
}

// Strudel's own pan, 0 left … 1 right, asks for the chip channel at that place in the AY chip's layout.
midimap.pan = { ccn: 10, min: 0, max: 1 };

await midimaps({ uzx: midimap });

/** Send the pattern to MIDI to PSG on `port`, with the controls above. */
Pattern.prototype.uzx = function (port) {
    return this.midimap('uzx').midi(port);
};
