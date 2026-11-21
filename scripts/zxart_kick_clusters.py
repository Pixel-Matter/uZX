#!/usr/bin/env python3
"""Find the kick drums among the samples of an instrument database and group them into types.

    python3 -I scripts/zxart_kick_clusters.py
    python3 -I scripts/zxart_kick_clusters.py tools/instrdump/corpus/zxart-top.sqlite 7 .cache/zxart/kicks

A kick is a sample that starts loud on a tone falling at least 7 semitones to under 125 Hz, dies
out within 12 frames, has noise on its first two frames at most, and does not sound through the
envelope. Each is played at the note the tunes mostly use it on, and its first 16 frames (level,
pitch, tone and noise) are clustered with k-medoids. Prints each type with its medoid and, by the
year of the tunes, an old (to 2004) and a fresh (from 2015) example. With an output folder,
writes those examples as WAV files and kicks.json, their frames.

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

FRAMES = 24          # rendered per sample
SHAPE = 16           # of them, compared
CLOCK = 1773400      # ZX Spectrum's AY, Hz
OLD, FRESH = 2004, 2015
# AY-3-8910 output per level
LEVELS = [0.0, 0.0100, 0.0145, 0.0211, 0.0307, 0.0455, 0.0645, 0.1074,
          0.1266, 0.2050, 0.2922, 0.3728, 0.4925, 0.6353, 0.8056, 1.0]


def hz(period):
    return CLOCK / (16 * max(period, 1))


def note_period(note):
    """An approximate tone period of a tracker's note; its tables differ by a few percent."""
    return round(3400 / 2 ** (note / 12))


def render(length, loop, stops, lanes, base):
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
                      lane("noise", pos), noise & 31, lane("env", pos))
        pos += 1
        if pos >= length:
            if stops:
                break
            pos = loop
    return out


def weighted_median(counter):
    half, acc = sum(counter.values()) / 2, 0
    for key in sorted(counter):
        acc += counter[key]
        if acc >= half:
            return key


def load(db):
    """Every sample the tunes play, with its usage summed over them."""
    used = collections.defaultdict(lambda: {"notes": 0, "rows": 0, "env_rows": 0, "by_note": collections.Counter(),
                                            "modules": set()})
    for sid, module, envelope, note, notes, rows in db.execute(
            "SELECT sample_id, module_id, envelope, note, notes, rows FROM instrument_usage WHERE sample_id IS NOT NULL"):
        use = used[sid]
        use["notes"] += notes
        use["rows"] += rows
        use["env_rows"] += rows if envelope >= 0 else 0
        use["by_note"][note] += notes
        use["modules"].add(module)
    samples = []
    for sid, length, loop, stops, lanes in db.execute("SELECT id, len, loop, stops, lanes FROM instrument WHERE kind = 'sample'"):
        if sid in used:
            use = used[sid]
            frames = render(length, loop, stops, json.loads(lanes), note_period(weighted_median(use["by_note"])))
            samples.append({"id": sid, "frames": frames, **use})
    return samples


def is_kick(sample):
    f = sample["frames"]
    level, period = f[:, 0], np.maximum(f[:, 2], 1)
    tone = (f[:, 1] > 0) & (level > 0)
    noise = (f[:, 3] > 0) & (level > 0)
    sounding = [i for i in range(10) if tone[i]]
    if sample["notes"] < 8 or len(sounding) < 2 or sounding[0] > 1 or level[:2].max() < 12:
        return False
    # the envelope on silent frames lets a buzzer bass through after the hit: still a kick
    if sample["env_rows"] > 0.3 * sample["rows"] and f[level > 0, 5].any():
        return False
    lowest = max(period[i] for i in sounding)
    return (12 * math.log2(lowest / period[sounding[0]]) >= 7 and hz(lowest) < 125
            and level[12:].mean() <= 3 and noise[2:SHAPE].sum() <= 1)


def shape(sample):
    """What two kicks are compared by: level, pitch where it sounds, and what sounds."""
    f = sample["frames"][:SHAPE]
    amp = f[:, 0] / 15
    tone = (f[:, 1] > 0) & (amp > 0)
    noise = (f[:, 3] > 0) & (amp > 0)
    pitch, last = np.zeros(SHAPE), 0.0
    for i in range(SHAPE):
        if tone[i]:
            last = math.log2(max(f[i, 2], 1) / 1000)
        pitch[i] = last
    return np.concatenate([amp, amp * pitch, 0.5 * amp * tone, amp * noise])


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
    for sample in members:
        ids = ",".join(map(str, sample["modules"]))
        tunes = [t for t in db.execute(
            f"SELECT m.id, m.title, m.year, m.rating, (SELECT group_concat(a.name, ', ') FROM module_author ma "
            f"JOIN author a ON a.id = ma.author_id WHERE ma.module_id = m.id) FROM module m "
            f"WHERE m.id IN ({ids}) AND m.year IS NOT NULL ORDER BY m.rating DESC, m.id") if in_era(t[2])]
        if tunes and (best is None or (len(tunes), sample["notes"]) > (len(best[1]), best[0]["notes"])):
            best = (sample, tunes)
    return best


def describe(frames):
    level = frames[:, 0]
    tone = [i for i in range(FRAMES) if frames[i, 1] and level[i] > 0]
    return {"frames": int((level > 0).sum()), "from_hz": round(hz(frames[tone[0], 2])),
            "to_hz": round(hz(max(frames[i, 2] for i in tone))), "click": bool(frames[0, 3] and level[0] > 0)}


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
    samples = data + [0.0] * (rate // 10)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(rate)
        out.writeframes(struct.pack(f"<{len(samples)}h", *(int(s * 24000) for s in samples)))


def main():
    db_path = sys.argv[1] if len(sys.argv) > 1 else "tools/instrdump/corpus/zxart-top.sqlite"
    types = int(sys.argv[2]) if len(sys.argv) > 2 else 7
    out = Path(sys.argv[3]) if len(sys.argv) > 3 else None
    db = sqlite3.connect(db_path)
    samples = load(db)
    kicks = [s for s in samples if is_kick(s)]
    shapes = np.array([shape(s) for s in kicks])
    dist = np.sqrt(((shapes[:, None, :] - shapes[None, :, :]) ** 2).sum(-1))
    medoids, labels, _ = min((k_medoids(dist, types, seed) for seed in range(40)), key=lambda result: result[2])
    print(f"{len(kicks)} kicks among {len(samples)} samples in use; {types} types, silhouette {silhouette(dist, labels):.2f}")

    report = []
    for number, j in enumerate(sorted(range(types), key=lambda j: -(labels == j).sum()), 1):
        members = [kicks[i] for i in np.where(labels == j)[0]]
        first_years = [y for y in (db.execute(f"SELECT min(year) FROM module WHERE id IN ({','.join(map(str, m['modules']))})")
                                   .fetchone()[0] for m in members) if y]
        medoid = kicks[medoids[j]]
        entry = {"type": number, "samples": len(members), "tunes": len(set().union(*(m["modules"] for m in members))),
                 "old": sum(y <= OLD for y in first_years), "fresh": sum(y >= FRESH for y in first_years),
                 "medoid": {"id": medoid["id"], **describe(medoid["frames"])}, "examples": {}}
        print(f"\ntype {number}: {entry['samples']} samples in {entry['tunes']} tunes; first used to {OLD}: {entry['old']}, "
              f"from {FRESH}: {entry['fresh']}")
        for era, in_era in (("old", lambda y: y <= OLD), ("fresh", lambda y: y >= FRESH)):
            found = example(db, members, in_era)
            if not found:
                continue
            sample, tunes = found
            _, title, year, _, authors = tunes[0]
            d = describe(sample["frames"])
            entry["examples"][era] = {"id": sample["id"], "tunes": len(tunes), "title": title, "year": year,
                                      "authors": authors, **d, "rows": sample["frames"][:SHAPE].astype(int).tolist()}
            print(f"  {era:5s} {sample['id']}  {d['frames']:2d} frames  {d['from_hz']:3d} -> {d['to_hz']:3d} Hz  "
                  f"{'click' if d['click'] else '     '}  in {len(tunes)} tunes, e.g. {authors} - {title} ({year})")
            print("        level " + " ".join(f"{int(v):2d}" for v in sample["frames"][:SHAPE, 0]))
            if out:
                out.mkdir(parents=True, exist_ok=True)
                write_wav(out / f"type{number}_{era}.wav", sample["frames"])
        report.append(entry)
    if out:
        (out / "kicks.json").write_text(json.dumps(report, ensure_ascii=False, indent=1))
        print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
