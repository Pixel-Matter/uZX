#!/usr/bin/env python3
"""Tag the samples and ornaments of an instrument database by what they are and how the tunes use them.

    python3 -I scripts/zxart_instrument_tags.py
    python3 -I scripts/zxart_instrument_tags.py tools/instrdump/corpus/zxart-top.sqlite

Writes the table tag(instrument_id, tag, share) into the database, replacing it, and prints how
many instruments and tunes each tag covers. A tag of a sample's own shape has share 1; a tag that
depends on how the patterns play it has the share of its notes it holds for, and is kept from 0.2.

Sample tags
  what sounds      tone, noise, tone+noise, static (a single step: the pattern makes the sound)
  level            one-shot, pluck, sustain, decay, tremolo, swell
  pitch            vibrato, slide-down, slide-up, detune, pitch-attack
  drums            kick, snare, tom, closed-hat, open-hat, crash, perc: several at once for a
                   combined hit; with-kick, with-snare, with-hat for a sound that goes on after one
  role             bass (below F#-3), melody, arp (played on a chord ornament)
  envelope         buzz, saw (shapes 8, 12), triangle (10, 14), and where its pitch lies from the
                   note: env-unison, env-octave-up, env-octave-down, env-2-octaves-down (or more),
                   env-fifth, env-fourth, env-interval (another one), env-free (it does not
                   follow the note)

Ornament tags
  chord            octaves, fifth, fourth, major, minor, sus2, sus4, dim, aug, maj7, min7, dom7,
                   maj6, min6, add9, third, dyad, chord (another one), inversion, flat
  motion           fast (a frame a note), medium, slow; trill; slide-up, slide-down;
                   attack-fall, attack-rise (it settles on one note); noise

Needs numpy. The database comes from scripts/zxart_instruments_db.py.
"""
import collections
import json
import sqlite3
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))   # python3 -I leaves the script's folder out
import zxart_sounds as sounds_of                  # noqa: E402

KEPT_FROM = 0.2      # the share of a sample's notes a usage tag must hold for
BASS_BELOW = 30      # the note number of F#-3
CHORDS = {(0, 7): "fifth", (0, 5): "fourth", (0, 4): "third", (0, 3): "third", (0, 4, 7): "major", (0, 3, 7): "minor",
          (0, 5, 7): "sus4", (0, 2, 7): "sus2", (0, 3, 6): "dim", (0, 4, 8): "aug", (0, 4, 7, 11): "maj7",
          (0, 3, 7, 10): "min7", (0, 4, 7, 10): "dom7", (0, 4, 7, 9): "maj6", (0, 3, 7, 9): "min6", (0, 2, 4, 7): "add9"}
OCTAVES = {0: "env-unison", 12: "env-octave-up", 24: "env-octave-up", -12: "env-octave-down",
           -24: "env-2-octaves-down", -36: "env-2-octaves-down"}
WITH = {"kick": "with-kick", "snare": "with-snare", "closed-hat": "with-hat", "open-hat": "with-hat"}


def ornament_tags(length, loop, stops, lanes):
    notes = lanes.get("noteOfs", [0] * length)
    tags = {"noise"} if "noiseOfs" in lanes else set()
    if "noteAcc" in lanes:
        drift = sum(n for n, acc in zip(notes, lanes["noteAcc"]) if acc)
        return tags | ({"slide-up"} if drift > 0 else {"slide-down"} if drift < 0 else set())
    held = notes if stops else notes[loop:]
    if not any(notes):
        return tags | {"flat"}
    if len(set(held)) == 1:
        # everything before the note it settles on is how it gets there
        return tags | ({"attack-fall"} if notes[0] > held[0] else {"attack-rise"} if notes[0] < held[0] else {"flat"})
    runs = [1]
    for a, b in zip(held, held[1:]):
        if a == b:
            runs[-1] += 1
        else:
            runs.append(1)
    tags.add("fast" if np.median(runs) < 2 else "medium" if np.median(runs) < 4 else "slow")
    classes = sorted({n % 12 for n in held})
    if max(held) - min(held) <= 2:
        return tags | {"trill"}
    if classes == [0]:
        return tags | {"octaves"}
    for root in [0] + [c for c in classes if c]:
        name = CHORDS.get(tuple(sorted((c - root) % 12 for c in classes)))
        if name and (root == 0 or 0 in classes):
            return tags | {name} | ({"inversion"} if root else set())
    return tags | {"dyad" if len(classes) == 2 else "chord"}


def shape_tags(sound, length):
    """What a sample is by itself, from its sound the tunes play most."""
    frames = sound["frames"]
    level = np.minimum(frames[:, 0], 15)
    audible = level > 0
    if not audible.any():
        return {"silent"}
    tags = {"static"} if length == 1 else set()
    tone, noise = (frames[:, 1] > 0) & audible, (frames[:, 3] > 0) & audible
    tags.add("tone" if noise.sum() < 0.15 * audible.sum() else "noise" if tone.sum() < 0.15 * audible.sum() else "tone+noise")

    peak, tail = level.max(), level[32:]
    if sound["length"] <= sounds_of.SILENT_BY:
        tags.add("one-shot")
    elif tail.max() - tail.min() >= 2:
        tags.add("tremolo")
    else:
        tags.add("sustain" if tail.mean() >= 0.8 * peak else "decay")
    if int(level.argmax()) >= 4:
        tags.add("swell")
    elif level[16] <= 0.5 * peak:
        tags.add("pluck")

    offsets = frames[:, 2] - sound["base"]
    late = offsets[32:][tone[32:]]
    if len(late) > 8:
        drift, wobble = late[-1] - late[0], late.max() - late.min()
        if abs(drift) >= 8 and abs(drift) >= 0.7 * wobble:
            tags.add("slide-down" if drift > 0 else "slide-up")      # a longer period is a lower pitch
        elif wobble >= 2:
            tags.add("vibrato")
        elif abs(np.median(late)) >= 1:
            tags.add("detune")
        early = offsets[:8][tone[:8]]
        if len(early) and np.abs(early - np.median(late)).max() >= max(12, 0.03 * sound["base"]):
            tags.add("pitch-attack")
    return tags


def usage_tags(sound, chordal):
    """What a sound is in the tunes, as tag: the share of the sound's notes it holds for."""
    found, dies = sounds_of.parts(sound)
    if dies and found:
        return dict.fromkeys(found, 1.0)
    tags = {WITH[part]: 1.0 for part in found if part in WITH}
    rows = sum(sound["ornaments"].values())
    arp = sum(count for ornament, count in sound["ornaments"].items() if ornament in chordal) / rows
    by_note = sound["rows_by_note"]
    driven = sound["frames"][:, 0] == sounds_of.ENVELOPE
    if sound["envelope_notes"] and driven.any() and (sound["frames"][driven, 1] > 0).mean() < 0.5:
        by_note = sound["envelope_notes"]       # a buzzer with its tone off sounds the envelope's note, not the written one
    low = sum(count for note, count in by_note.items() if note < BASS_BELOW) / sum(by_note.values())
    tags.update({"arp": arp, "bass": (1 - arp) * low, "melody": (1 - arp) * (1 - low)})
    if sound["envelope"]:
        shape = max(sound["shapes"], key=sound["shapes"].get)
        tags["buzz"] = 1.0
        tags["triangle" if shape in sounds_of.TRIANGLES else "saw" if shape in (8, 12) else "env-other"] = 1.0
        if sound["intervals"]:
            interval, count = sound["intervals"].most_common(1)[0]
            follows = count >= 0.6 * sum(sound["intervals"].values())
            named = OCTAVES.get(interval) or {7: "env-fifth", 5: "env-fourth"}.get(interval % 12, "env-interval")
            tags[named if follows else "env-free"] = 1.0
    return tags


def main():
    db = sqlite3.connect(sys.argv[1] if len(sys.argv) > 1 else "tools/instrdump/corpus/zxart-top.sqlite")
    tags = collections.defaultdict(collections.Counter)
    chordal = set()
    for oid, length, loop, stops, lanes in db.execute("SELECT id, len, loop, stops, lanes FROM instrument WHERE kind = 'ornament'"):
        found = ornament_tags(length, loop, stops, json.loads(lanes))
        tags[oid].update(dict.fromkeys(found, 1.0))
        if found & (set(CHORDS.values()) | {"octaves", "dyad", "chord"}):
            chordal.add(oid)

    lengths = dict(db.execute("SELECT id, len FROM instrument WHERE kind = 'sample'"))
    by_sample = collections.defaultdict(list)
    for sound in sounds_of.load(db):
        by_sample[sound["id"]].append(sound)
    for sid, sounds in by_sample.items():
        tags[sid].update(dict.fromkeys(shape_tags(max(sounds, key=lambda s: s["share"]), lengths[sid]), 1.0))
        for sound in sounds:
            for tag, share in usage_tags(sound, chordal).items():
                tags[sid][tag] += share * sound["share"]

    db.executescript("DROP TABLE IF EXISTS tag; "
                     "CREATE TABLE tag(instrument_id TEXT, tag TEXT, share REAL, PRIMARY KEY(instrument_id, tag)); "
                     "CREATE INDEX tag_tag ON tag(tag);")
    db.executemany("INSERT INTO tag VALUES(?,?,?)", [(iid, tag, round(min(share, 1.0), 3)) for iid, found in tags.items()
                                                    for tag, share in found.items() if share >= KEPT_FROM])
    db.commit()
    for kind in ("sample", "ornament"):
        print(f"\n{kind} tags: instruments, tunes")
        for tag, instruments, tunes in db.execute(
                "SELECT t.tag, count(DISTINCT t.instrument_id), count(DISTINCT s.module_id) FROM tag t "
                "JOIN instrument i ON i.id = t.instrument_id JOIN slot s ON s.instrument_id = t.instrument_id "
                "WHERE i.kind = ? GROUP BY 1 ORDER BY 2 DESC", (kind,)):
            print(f"  {tag:20s} {instruments:5d} {tunes:5d}")
    untagged = db.execute("SELECT count(*) FROM instrument i WHERE NOT EXISTS(SELECT 1 FROM tag t WHERE t.instrument_id = i.id)").fetchone()[0]
    print(f"\n{untagged} instruments without a tag: samples no tune plays")


if __name__ == "__main__":
    main()
