#!/usr/bin/env python3
"""Find the drums among the sounds of an instrument database and group each kind into types.

    python3 -I scripts/zxart_drum_clusters.py
    python3 -I scripts/zxart_drum_clusters.py tools/instrdump/corpus/zxart-top.sqlite .cache/zxart/drums

A drum is a sound that starts loud and dies out; its kind is the drums zxart_sounds.parts() finds
in it, which may be several at once: "kick+snare". The first frames of each kind (level, pitch,
tone, noise and its period) are clustered with k-medoids. Prints each type and, by the year of
the tunes, an old (to 2004) and a fresh (from 2015) example: the one most tunes of that time use,
among the half of the type nearest its medoid. With an output folder, writes those examples as
WAV files and drums.json, their frames.

Needs numpy. The database comes from scripts/zxart_instruments_db.py.
"""
import collections
import json
import math
import sqlite3
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))   # python3 -I leaves the script's folder out
import zxart_sounds as sounds_of                  # noqa: E402

OLD, FRESH = 2004, 2015
SMALLEST = 20        # a kind with fewer sounds is not split into types


def compared_frames(kind):
    """How much of a kind's start its types are compared by: a crash is long, a closed hat is over at once."""
    if "crash" in kind:
        return 48
    if kind == "closed-hat":
        return 8
    return 16 if kind == "kick" else 24


def shape(sound, frames):
    """What two sounds of a kind are compared by: level, pitch where it sounds, and what sounds."""
    f = sound["frames"][:frames]
    amp = np.minimum(f[:, 0], 15) / 15
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
            "from_hz": round(sounds_of.hz(frames[tone[0], 2])) if len(tone) else None,
            "to_hz": round(sounds_of.hz(frames[tone[-1], 2])) if len(tone) else None,
            "noise_period": round(sound["noise_period"]) if len(noise) else None}


def cluster(db, kind, sounds, out):
    """The types of one kind of drum, largest first, each with its old and fresh example."""
    frames, types = compared_frames(kind), min(7, max(2, len(sounds) // 60))
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
            shown = max(16, min(sounds_of.FRAMES, sound["length"]))
            entry["examples"][era] = {"id": sound["id"], "noise": sound["noise"], "tunes": len(tunes), "title": title,
                                      "year": year, "authors": authors, **describe(sound),
                                      "rows": sound["frames"][:shown].astype(int).tolist()}
            print(f"    {era:5s} {sound['id']} noise {sound['noise']:2d}  in {len(tunes)} tunes, e.g. {authors} - {title} ({year})")
            print("          level " + " ".join(f"{int(v):2d}" for v in sound["frames"][:16, 0]))
            if out:
                sounds_of.write_wav(out / f"{kind}_{number}_{era}.wav", sound["frames"][:shown + 1])
        report.append(entry)
    return report


def main():
    db_path = sys.argv[1] if len(sys.argv) > 1 else "tools/instrdump/corpus/zxart-top.sqlite"
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    if out:
        out.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(db_path)
    sounds = sounds_of.load(db)
    by_kind = collections.defaultdict(list)
    for sound in sounds:
        found, dies = sounds_of.parts(sound)
        if dies and found:
            by_kind[sounds_of.kind(found)].append(sound)
    print(f"{len(sounds)} sounds in use, {sum(map(len, by_kind.values()))} of them drums")
    for kind, members in sorted(by_kind.items(), key=lambda item: -len(item[1])):
        print(f"  {kind:22s} {len(members)}")
    report = [{"kind": kind, "sounds": len(members), "types": cluster(db, kind, members, out)}
              for kind, members in sorted(by_kind.items(), key=lambda item: -len(item[1])) if len(members) >= SMALLEST]
    if out:
        (out / "drums.json").write_text(json.dumps(report, ensure_ascii=False, indent=1))
        print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
