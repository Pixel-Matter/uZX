"""The sounds of an instrument database: its samples as the tunes play them. Shared by the
zxart_* scripts that study them; the database comes from scripts/zxart_instruments_db.py.

A sound is a sample in a context that changes what it is: the noise base the patterns play it on,
and whether they play it through the envelope. load() renders each at the note the tunes mostly
use it on, and parts() names the drums it holds: one sound may be a kick and a snare at once, or
a bass that starts with a kick.

Needs numpy.
"""
import collections
import json
import math
import struct
import wave

import numpy as np

FRAMES = 64          # rendered per sound
SILENT_BY = 48       # a sound that dies out has done so by this frame
CLOCK = 1773400      # ZX Spectrum's AY, Hz
ENVELOPE = 16        # the level of a frame the envelope drives
TRIANGLES = (10, 14)  # envelope shapes whose cycle is two periods long
# AY-3-8910 output per level
LEVELS = [0.0, 0.0100, 0.0145, 0.0211, 0.0307, 0.0455, 0.0645, 0.1074,
          0.1266, 0.2050, 0.2922, 0.3728, 0.4925, 0.6353, 0.8056, 1.0]
# a sound's drums, in the order they are named
PARTS = ("kick", "snare", "tom", "closed-hat", "open-hat", "crash", "perc")


def hz(period):
    return CLOCK / (16 * max(period, 1))


def render(length, loop, stops, lanes, base, noise_base, envelope):
    """The frames of a sample by the rules of tools/instrdump/Model.h, as rows of
    (level, tone on, tone period, noise on, noise period). The level is ENVELOPE where the
    pattern's envelope drives the frame."""
    def lane(name, i):
        return lanes[name][i] if name in lanes else 0
    out = np.zeros((FRAMES, 5))
    pos = tone_acc = vol_acc = noise_acc = 0
    for frame in range(FRAMES):
        vol_acc = max(-15, min(15, vol_acc + lane("volSlide", pos)))
        tone = tone_acc + lane("toneOfs", pos)
        if lane("toneAcc", pos):
            tone_acc = tone
        noise = noise_acc + lane("noiseOfs", pos)
        if lane("noiseAcc", pos):
            noise_acc = noise
        level = ENVELOPE if envelope and lane("env", pos) else max(0, min(15, lane("vol", pos) + vol_acc))
        out[frame] = (level, lane("tone", pos), (base + tone) & 0xFFF,
                      lane("noise", pos), (noise if lane("noiseAbs", pos) else noise + noise_base) & 31)
        pos += 1
        if pos >= length:
            if stops:
                break
            pos = loop
    return out


def envelope_interval(shape, envelope_period, tone_period):
    """Semitones from the note's tone up to the pitch of the envelope's cycle."""
    cycle = 256 * envelope_period * (2 if shape in TRIANGLES else 1)
    return 12 * math.log2(16 * tone_period / cycle)


def load(db):
    """Every sound the tunes play, with its usage summed over them and what parts() goes by."""
    samples = {sid: (length, loop, stops, json.loads(lanes)) for sid, length, loop, stops, lanes in
               db.execute("SELECT id, len, loop, stops, lanes FROM instrument WHERE kind = 'sample'")}
    tables = {name: json.loads(periods) for name, periods in db.execute("SELECT name, periods FROM note_table")}
    used = collections.defaultdict(lambda: {
        "notes": 0, "rows": 0, "by_note": collections.Counter(), "rows_by_note": collections.Counter(),
        "ornaments": collections.Counter(), "modules": set(), "tables": collections.Counter(),
        "shapes": collections.Counter(), "intervals": collections.Counter(), "envelope_notes": collections.Counter()})
    totals = collections.Counter()
    for sid, ornament, envelope, period, noise, note, notes, rows, table, module in db.execute(
            "SELECT sample_id, ornament_id, envelope, envelope_period, noise, note, notes, rows, note_table, module_id "
            "FROM instrument_usage WHERE sample_id IS NOT NULL"):
        lanes = samples[sid][3]
        # a context tells sounds apart only where the sample answers to it
        noise_base = noise if "noise" in lanes and "noiseAbs" not in lanes else 0
        through_envelope = envelope >= 0 and "env" in lanes
        use = used[sid, noise_base, through_envelope]
        totals[sid] += notes
        use["notes"] += notes
        use["rows"] += rows
        use["by_note"][note] += notes
        use["rows_by_note"][note] += rows
        use["ornaments"][ornament] += rows
        use["modules"].add(module)
        use["tables"][table] += notes
        if through_envelope:
            use["shapes"][envelope] += rows
            if period and note < len(tables[table]):
                interval = round(envelope_interval(envelope, period, tables[table][note]))
                use["intervals"][interval] += rows
                use["envelope_notes"][note + interval] += rows      # the note the envelope's cycle sounds
    sounds = []
    for (sid, noise, envelope), use in used.items():
        # a context a sample seldom meets is another channel's doing: the chip has one noise and one envelope
        if use["notes"] < 0.2 * totals[sid]:
            continue
        note = max(use["by_note"], key=use["by_note"].get)
        table = tables[max(use["tables"], key=use["tables"].get)]
        base = table[min(note, len(table) - 1)]
        frames = render(*samples[sid], base, noise, envelope)
        sounds.append({"id": sid, "noise": noise, "envelope": envelope, "share": use["notes"] / totals[sid],
                       "base": base, "frames": frames, **use, **measure(frames, use)})
    return sounds


def measure(frames, use):
    level, period = frames[:, 0], np.maximum(frames[:, 2], 1)
    audible = level > 0
    tone = np.where((frames[:, 1] > 0) & audible)[0]
    noise = np.where((frames[:, 3] > 0) & audible)[0]
    attack = tone[tone < 8]
    total = sum(use["by_note"].values())
    return {
        "length": int(np.where(audible)[0].max()) + 1 if audible.any() else 0,
        "tone": tone, "noise_frames": noise,
        # semitones the tone falls from its first frame: over the whole sound, and within its first 8 frames
        "fall": 12 * math.log2(period[tone].max() / period[tone[0]]) if len(tone) > 1 else 0.0,
        "attack_fall": 12 * math.log2(period[attack].max() / period[attack[0]]) if len(attack) > 1 else 0.0,
        "attack_hz": hz(period[attack].max()) if len(attack) else 0.0,
        "noise_period": float(frames[noise, 4].mean()) if len(noise) else 0.0,
        "pitches": sum(1 for count in use["by_note"].values() if count >= 0.05 * total),  # notes it is played on
    }


def parts(sound):
    """The drums a sound holds, as a set of PARTS, and whether it is one: a sound that dies out.
    One that goes on has a body besides, and its parts are how it starts."""
    frames, level = sound["frames"], sound["frames"][:, 0]
    tone, noise, length = sound["tone"], sound["noise_frames"], sound["length"]
    dies = 0 < length <= SILENT_BY
    found = set()
    if sound["notes"] < 8 or level[:2].max() < 10 or length == 0:
        return found, dies
    falls = len(tone) > 1 and sound["fall"] >= 3
    if len(tone) and tone[0] <= 1 and sound["attack_fall"] >= 7 and sound["attack_hz"] < 125:
        found.add("kick")
    # noise that runs on into the body is the sound's timbre, not a drum on its start
    hiss = len(noise) if dies else (len(noise) if len(noise) and noise[-1] < 16 else 0)
    if hiss >= 4 and falls and (not dies or length <= 24):
        found.add("snare")     # hiss with a falling tone under it
    elif hiss > 16:
        found.add("crash")
    elif hiss >= 4:
        found.add("snare" if sound["noise_period"] > 5 else "open-hat")   # a dark hiss alone, or a bright one
    elif hiss >= 2 or (hiss == 1 and len(tone) == 0):
        found.add("closed-hat")
    if dies and not found and hiss <= 1:
        if falls and length <= 32:
            found.add("tom")
        # a steady hit is a drum only when it keeps to a note or two: on more it is a melodic pluck
        elif length <= 12 and sound["pitches"] <= 2:
            found.add("perc")
    return found, dies


def kind(found):
    """The name of a combination of parts: "kick+snare"."""
    return "+".join(part for part in PARTS if part in found)


def write_wav(path, frames, rate=44100, frame_rate=50):
    """One AY channel, as the chip mixes a tone and noise; a frame the envelope drives plays at full level."""
    per_frame = rate // frame_rate
    tick = CLOCK / 8 / rate           # the chip's divider steps per output sample
    tone_count = noise_count = 0.0
    tone_out, lfsr, data = 1, 1, []
    for level, tone_on, tone_period, noise_on, noise_period in frames:
        for _ in range(per_frame):
            tone_count += tick
            while tone_count >= max(tone_period, 1):
                tone_count -= max(tone_period, 1)
                tone_out ^= 1
            noise_count += tick
            while noise_count >= 2 * max(noise_period, 1):
                noise_count -= 2 * max(noise_period, 1)
                lfsr = (lfsr >> 1) | (((lfsr ^ (lfsr >> 3)) & 1) << 16)
            high = (tone_out or not tone_on) and ((lfsr & 1) or not noise_on)
            data.append(LEVELS[min(int(level), 15)] if high else 0.0)
    data += [0.0] * (rate // 10)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(struct.pack(f"<{len(data)}h", *(int(s * 24000) for s in data)))
