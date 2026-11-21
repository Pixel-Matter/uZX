#!/usr/bin/env python3
"""Find the drums among the samples of an instrument database and group each kind into types.

    python3 -I scripts/zxart_drum_clusters.py
    python3 -I scripts/zxart_drum_clusters.py tools/instrdump/corpus/zxart-top.sqlite .cache/zxart/drums

A sound is a sample with the noise base the patterns play it on, played at the note the tunes
mostly use it on. A drum is a sound that starts loud, is silent within 48 frames and does not
sound through the envelope; classify() tells the kinds apart by what sounds in it: kick, snare,
tom, closed and open hi-hat, crash, and other percussion. The first frames of each kind (level,
pitch, tone, noise and its period) are clustered with k-medoids. Prints each type and, by the
year of the tunes, an old (to 2004) and a fresh (from 2015) example: the one most tunes of that
time use, among the half of the type nearest its medoid. With an output folder, writes those
examples as WAV files and drums.json, their frames.

Needs numpy. The database comes from scripts/zxart_instruments_db.py.
"""
import collections
import json
import math
import sqlite3
import struct
import sys
import wave
from pathlib import Path

import numpy as np

FRAMES = 64          # rendered per sound
SILENT_BY = 48       # a drum has died out by this frame
CLOCK = 1773400      # ZX Spectrum's AY, Hz
OLD, FRESH = 2004, 2015
# kind: (frames its types are compared by, number of types)
KINDS = {"kick": (16, 7), "snare": (24, 6), "tom": (24, 4), "closed hat": (8, 4), "open hat": (16, 4),
         "crash": (48, 4), "other": (16, 3)}
# AY-3-8910 output per level
LEVELS = [0.0, 0.0100, 0.0145, 0.0211, 0.0307, 0.0455, 0.0645, 0.1074,
          0.1266, 0.2050, 0.2922, 0.3728, 0.4925, 0.6353, 0.8056, 1.0]


def hz(period):
    return CLOCK / (16 * max(period, 1))


def note_period(note):
    """An approximate tone period of a tracker's note; its tables differ by a few percent."""
    return round(3400 / 2 ** (note / 12))


def render(length, loop, stops, lanes, base, noise_base):
    """The frames of a sample by the rules of tools/instrdump/Model.h, as rows of
    (level, tone on, tone period, noise on, noise period, envelope on)."""
    def lane(name, i):
        return lanes[name][i] if name in lanes else 0
    out = np.zeros((FRAMES, 6))
    pos = tone_acc = vol_acc = noise_acc = 0
    for frame in range(FRAMES):
        vol_acc = max(-15, min(15, vol_acc + lane("volSlide", pos)))
        tone = tone_acc + lane("toneOfs", pos)
        if lane("toneAcc", pos):
            tone_acc = tone
        noise = noise_acc + lane("noiseOfs", pos)
        if lane("noiseAcc", pos):
            noise_acc = noise
        out[frame] = (max(0, min(15, lane("vol", pos) + vol_acc)), lane("tone", pos), (base + tone) & 0xFFF,
                      lane("noise", pos), (noise if lane("noiseAbs", pos) else noise + noise_base) & 31, lane("env", pos))
        pos += 1
        if pos >= length:
            if stops:
                break
            pos = loop
    return out


def load(db):
    """Every sound the tunes play, with its usage summed over them and what classify() goes by."""
    samples = {sid: (length, loop, stops, json.loads(lanes)) for sid, length, loop, stops, lanes in
               db.execute("SELECT id, len, loop, stops, lanes FROM instrument WHERE kind = 'sample'")}
    used = collections.defaultdict(lambda: {"notes": 0, "rows": 0, "env_rows": 0, "by_note": collections.Counter(),
                                            "modules": set()})
    totals = collections.Counter()
    for sid, module, envelope, noise, note, notes, rows in db.execute(
            "SELECT sample_id, module_id, envelope, noise, note, notes, rows FROM instrument_usage WHERE sample_id IS NOT NULL"):
        lanes = samples[sid][3]
        # the noise base tells sounds apart only where the sample's noise is added to it
        based = "noise" in lanes and "noiseAbs" not in lanes
        use = used[sid, noise if based else 0]
        totals[sid] += notes
        use["notes"] += notes
        use["rows"] += rows
        use["env_rows"] += rows if envelope >= 0 else 0
        use["by_note"][note] += notes
        use["modules"].add(module)
    sounds = []
    for (sid, noise), use in used.items():
        # a base a sample seldom meets is another channel's: the chip has one
        if use["notes"] < 0.2 * totals[sid]:
            continue
        note = max(use["by_note"], key=use["by_note"].get)
        frames = render(*samples[sid], note_period(note), noise)
        sounds.append({"id": sid, "noise": noise, "frames": frames, **use, **measure(frames, use)})
    return sounds


def measure(frames, use):
    level, period = frames[:, 0], np.maximum(frames[:, 2], 1)
    audible = level > 0
    tone = np.where((frames[:, 1] > 0) & audible)[0]
    noise = np.where((frames[:, 3] > 0) & audible)[0]
    total = sum(use["by_note"].values())
    return {
        "length": int(np.where(audible)[0].max()) + 1 if audible.any() else 0,
        "tone": tone, "noise_frames": noise,
        "fall": 12 * math.log2(period[tone].max() / period[tone[0]]) if len(tone) > 1 else 0.0,  # semitones down
        "low_hz": hz(period[tone].max()) if len(tone) else 0.0,
        "noise_period": float(frames[noise, 4].mean()) if len(noise) else 0.0,
        "pitches": sum(1 for count in use["by_note"].values() if count >= 0.05 * total),  # notes it is played on
    }


def classify(sound):
    """The kind of drum a sound is, None when it is not one."""
    frames, level = sound["frames"], sound["frames"][:, 0]
    tone, noise, length = sound["tone"], sound["noise_frames"], sound["length"]
    if sound["notes"] < 8 or level[:2].max() < 10 or length == 0 or length > SILENT_BY:
        return None
    # the envelope on silent frames lets a buzzer bass through after the hit: still a drum
    if sound["env_rows"] > 0.3 * sound["rows"] and frames[level > 0, 5].any():
        return None
    falls = len(tone) > 1 and sound["fall"] >= 3
    late_noise = int((noise >= 2).sum())
    if len(noise) < length / 2:       # noise on its attack at most: the tone is the sound
        if not falls:
            # a steady hit is a drum only when it keeps to a note or two: on more it is a melodic pluck
            return "other" if length <= 12 and sound["pitches"] <= 2 else None
        if late_noise > 1:
            return "snare" if length <= 24 else None
        if tone[0] <= 1 and sound["fall"] >= 7 and sound["low_hz"] < 125 and level[12:24].mean() <= 3:
            return "kick"
        return "tom" if length <= 32 else None
    if length <= 3:
        return "closed hat"
    if falls:
        return "snare" if length <= 24 else None
    # hiss with no falling body: a bright one is a cymbal, a dark one a snare
    bright = sound["noise_period"] <= 5 or (len(tone) and hz(np.median(frames[tone, 2])) >= 1000)
    if length > 16:
        return "crash"
    return "open hat" if bright else "snare"


def shape(sound, frames):
    """What two sounds of a kind are compared by: level, pitch where it sounds, and what sounds."""
    f = sound["frames"][:frames]
    amp = f[:, 0] / 15
    tone = (f[:, 1] > 0) & (amp > 0)
    noise = (f[:, 3] > 0) & (amp > 0)
    pitch, last = np.zeros(frames), 0.0
    for i in range(frames):
        if tone[i]:
            last = math.log2(max(f[i, 2], 1) / 1000)
        pitch[i] = last
    return np.concatenate([amp, amp * pitch, 0.5 * amp * tone, amp * noise, amp * noise * f[:, 4] / 31])


def k_medoids(dist, k, seed):
    rng = np.random.default_rng(seed)
    medoids = [int(rng.integers(len(dist)))]
    while len(medoids) < k:
        far = dist[:, medoids].min(1) ** 2
        medoids.append(int(rng.choice(len(dist), p=far / far.sum())))
    medoids = np.array(medoids)
    while True:
        labels = dist[:, medoids].argmin(1)
        moved = medoids.copy()
        for j in range(k):
            members = np.where(labels == j)[0]
            if len(members):
                moved[j] = members[dist[np.ix_(members, members)].sum(1).argmin()]
        if (moved == medoids).all():
            return medoids, labels, dist[np.arange(len(dist)), medoids[labels]].sum()
        medoids = moved


def silhouette(dist, labels):
    scores = []
    for i, label in enumerate(labels):
        own = labels == label
        if own.sum() < 2:
            scores.append(0.0)
            continue
        inside = dist[i, own].sum() / (own.sum() - 1)
        outside = min(dist[i, labels == other].mean() for other in set(labels) if other != label)
        scores.append((outside - inside) / max(inside, outside))
    return float(np.mean(scores))


def example(db, members, in_era):
    """The member the most tunes of an era use, with the best rated of those tunes."""
    best = None
    for sound in members:
        ids = ",".join(map(str, sound["modules"]))
        tunes = [t for t in db.execute(
            f"SELECT m.id, m.title, m.year, m.rating, (SELECT group_concat(a.name, ', ') FROM module_author ma "
            f"JOIN author a ON a.id = ma.author_id WHERE ma.module_id = m.id) FROM module m "
            f"WHERE m.id IN ({ids}) AND m.year IS NOT NULL ORDER BY m.rating DESC, m.id") if in_era(t[2])]
        if tunes and (best is None or (len(tunes), sound["notes"]) > (len(best[1]), best[0]["notes"])):
            best = (sound, tunes)
    return best


def describe(sound):
    frames, tone, noise = sound["frames"], sound["tone"], sound["noise_frames"]
    return {"frames": sound["length"], "tone_frames": len(tone), "noise_frames": len(noise),
            "from_hz": round(hz(frames[tone[0], 2])) if len(tone) else None,
            "to_hz": round(hz(frames[tone[-1], 2])) if len(tone) else None,
            "noise_period": round(sound["noise_period"]) if len(noise) else None}


def write_wav(path, frames, rate=44100, frame_rate=50):
    """One AY channel, as the chip mixes a tone and noise."""
    per_frame = rate // frame_rate
    tick = CLOCK / 8 / rate           # the chip's divider steps per output sample
    tone_count = noise_count = 0.0
    tone_out, lfsr, data = 1, 1, []
    for level, tone_on, tone_period, noise_on, noise_period, _ in frames:
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
            data.append(LEVELS[int(level)] if high else 0.0)
    data += [0.0] * (rate // 10)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(struct.pack(f"<{len(data)}h", *(int(s * 24000) for s in data)))


def cluster(db, kind, sounds, out):
    """The types of one kind of drum, largest first, each with its old and fresh example."""
    frames, types = KINDS[kind]
    shapes = np.array([shape(s, frames) for s in sounds])
    dist = np.sqrt(((shapes[:, None, :] - shapes[None, :, :]) ** 2).sum(-1))
    medoids, labels, _ = min((k_medoids(dist, types, seed) for seed in range(40)), key=lambda result: result[2])
    print(f"\n{kind}: {len(sounds)} sounds, {types} types, silhouette {silhouette(dist, labels):.2f}")
    report = []
    for number, j in enumerate(sorted(range(types), key=lambda j: -(labels == j).sum()), 1):
        indices = np.where(labels == j)[0]
        members = [sounds[i] for i in indices]
        # examples come from the half of the type nearest its medoid: the far half shades into other types
        spread = dist[indices, medoids[j]]
        core = [sounds[i] for i, d in zip(indices, spread) if d <= np.median(spread)]
        first_years = [y for y in (db.execute(f"SELECT min(year) FROM module WHERE id IN ({','.join(map(str, m['modules']))})")
                                   .fetchone()[0] for m in members) if y]
        entry = {"type": number, "sounds": len(members), "tunes": len(set().union(*(m["modules"] for m in members))),
                 "old": sum(y <= OLD for y in first_years), "fresh": sum(y >= FRESH for y in first_years),
                 "medoid": describe(sounds[medoids[j]]), "examples": {}}
        print(f"  type {number}: {entry['sounds']} sounds in {entry['tunes']} tunes; first used to {OLD}: {entry['old']}, "
              f"from {FRESH}: {entry['fresh']}; medoid {entry['medoid']}")
        for era, in_era in (("old", lambda y: y <= OLD), ("fresh", lambda y: y >= FRESH)):
            found = example(db, core, in_era)
            if not found:
                continue
            sound, tunes = found
            _, title, year, _, authors = tunes[0]
            shown = max(16, min(FRAMES, sound["length"]))
            entry["examples"][era] = {"id": sound["id"], "noise": sound["noise"], "tunes": len(tunes), "title": title,
                                      "year": year, "authors": authors, **describe(sound),
                                      "rows": sound["frames"][:shown, :5].astype(int).tolist()}
            print(f"    {era:5s} {sound['id']} noise {sound['noise']:2d}  in {len(tunes)} tunes, e.g. {authors} - {title} ({year})")
            print("          level " + " ".join(f"{int(v):2d}" for v in sound["frames"][:16, 0]))
            if out:
                write_wav(out / f"{kind.replace(' ', '_')}_{number}_{era}.wav", sound["frames"][:shown + 1])
        report.append(entry)
    return report


def main():
    db_path = sys.argv[1] if len(sys.argv) > 1 else "tools/instrdump/corpus/zxart-top.sqlite"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    if out:
        out.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(db_path)
    sounds = load(db)
    by_kind = collections.defaultdict(list)
    for sound in sounds:
        by_kind[classify(sound)].append(sound)
    print(f"{len(sounds)} sounds in use, {len(sounds) - len(by_kind[None])} of them drums")
    report = [{"kind": kind, "sounds": len(by_kind[kind]), "types": cluster(db, kind, by_kind[kind], out)}
              for kind in KINDS if len(by_kind[kind]) >= KINDS[kind][1]]
    if out:
        (out / "drums.json").write_text(json.dumps(report, ensure_ascii=False, indent=1))
        print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
