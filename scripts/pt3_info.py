#!/usr/bin/env python3
"""Read structural facts from a ProTracker 3 module that a PSG dump loses.

    python3 -I scripts/pt3_info.py FILE.pt3 [...]

Prints the initial speed, the pattern lengths in play order (channel A rows,
honouring the per-channel note-duration command) and the speeds set by
in-pattern commands in any channel. Used as weak labels for tempo and meter
detection. The channel decoding follows zxtune's PT3 parser
(src/formats/chiptune/aym/protracker3_compiled.cpp).

info() also returns the tone table number and format version from the header,
and the written notes of all three channels weighted by the rows they sound
(note 0 is C1, so note % 12 is the pitch class). With --tuning, main prints
those instead; they are ground truth for tuning table and key detection.
"""
import sys
from collections import Counter
from pathlib import Path

# parameter bytes that follow the row-ending byte for special commands 1..9
EFFECT_PARAM_BYTES = {1: 3, 2: 5, 3: 1, 4: 1, 5: 2, 6: 0, 7: 0, 8: 3, 9: 1}


def read_word(d, i):
    return d[i] | d[i + 1] << 8


def channel_rows(d, ptr, limit=4096, notes=None, state=None):
    """Counts rows of one channel until its 0x00 terminator or `limit` rows.
    Returns (rows, speeds) where speeds lists (row, new_speed) for every
    set-speed command. rows is None when the data runs out first.

    With a notes Counter, adds each row's sounding note (0..95) weighted by the
    rows it lasts; state carries the note held across patterns as state["note"]."""
    rows, skip, i, speeds = 0, 1, ptr, []
    pending = []
    state = state if state is not None else {}
    while i < len(d) and rows < limit:
        b = d[i]
        i += 1
        if b == 0x00:
            return rows, speeds
        if 0x01 <= b <= 0x0F:
            pending.append(b)
        elif b == 0x10:
            i += 1                      # sample
        elif 0x11 <= b <= 0x1F:
            i += 3                      # envelope period word + sample
        elif 0x20 <= b <= 0x4F:
            pass                        # noise base / ornament
        elif 0x50 <= b <= 0xAF or b in (0xC0, 0xD0):
            for cmd in reversed(pending):
                if cmd == 9 and i < len(d):
                    speeds.append((rows, d[i]))
                i += EFFECT_PARAM_BYTES.get(cmd, 0)
            pending = []
            if 0x50 <= b <= 0xAF:
                state["note"] = b - 0x50
            elif b == 0xC0:
                state["note"] = None
            if notes is not None and state.get("note") is not None:
                notes[state["note"]] += skip
            rows += skip
        elif b == 0xB0:
            pass
        elif b == 0xB1:
            skip = d[i]
            i += 1
        elif 0xB2 <= b <= 0xBF:
            i += 2                      # envelope period word
        elif 0xC1 <= b <= 0xEF:
            pass                        # volume / sample
        elif b >= 0xF0:
            i += 1                      # ornament + sample byte
    return (rows if rows >= limit else None), speeds


def info(path):
    d = Path(path).read_bytes()
    if not d.startswith(b"ProTracker 3.") and not d.startswith(b"Vortex Tracker II"):
        return None
    # Players pick table variants by version; Vortex Tracker II modules count as 3.6.
    version = d[13] - ord("0") if d.startswith(b"ProTracker 3.") and 48 <= d[13] <= 57 else 6
    table = d[99]
    speed = d[100]
    patterns_ptr = read_word(d, 103)
    positions = []
    i = 201
    while i < len(d) and d[i] != 0xFF:
        positions.append(d[i] // 3)
        i += 1
    lengths, speed_changes = [], []
    notes = Counter()
    held = [{}, {}, {}]  # per-channel note held across patterns
    for p in positions:
        # channel A ends the pattern; B and C only play while A does
        rows, speeds = channel_rows(d, read_word(d, patterns_ptr + p * 6), notes=notes, state=held[0])
        if rows is None:
            return None
        for ch in (1, 2):
            speeds += channel_rows(d, read_word(d, patterns_ptr + p * 6 + 2 * ch), rows,
                                   notes=notes, state=held[ch])[1]
        lengths.append(rows)
        speed_changes += [(len(lengths) - 1, r, s) for r, s in sorted(speeds) if r < rows]
    return dict(speed=speed, lengths=lengths, speed_changes=speed_changes,
                version=version, table=table, notes=notes)


def pitch_classes(notes):
    """Row-weighted share of each pitch class, C first."""
    pcs = [0] * 12
    for note, rows in notes.items():
        pcs[note % 12] += rows
    total = sum(pcs) or 1
    return [x / total for x in pcs]


def rows_per_speed(meta):
    """Rows played at each speed, walking the order list once with the speed commands."""
    speed, played = meta["speed"], Counter()
    changes = {}
    for pos, row, s in meta["speed_changes"]:
        changes.setdefault(pos, []).append((row, s))
    for pos, length in enumerate(meta["lengths"]):
        row = 0
        for at, s in changes.get(pos, []):
            played[speed] += at - row
            row, speed = at, s
        played[speed] += length - row
    return played


NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def main(paths):
    tuning = "--tuning" in paths
    for f in (p for p in paths if p != "--tuning"):
        r = info(f)
        if r is None:
            print(f"? {Path(f).name}")
            continue
        if tuning:
            pcs = pitch_classes(r["notes"])
            top = sorted(range(12), key=lambda pc: -pcs[pc])[:7]
            print(f"version=3.{r['version']} table={r['table']} notes={sum(r['notes'].values())} "
                  f"top={' '.join(NOTE_NAMES[pc] for pc in top)} {Path(f).name}")
            continue
        common = Counter(r["lengths"]).most_common(3)
        speeds = sorted({s for _, _, s in r["speed_changes"]})
        played = rows_per_speed(r)
        main_speed, main_rows = played.most_common(1)[0]
        share = 100 * main_rows / max(1, sum(played.values()))
        print(f"speed={r['speed']} patterns={common} speed_cmds={speeds} "
              f"main_speed={main_speed} ({share:.0f}% of rows) {Path(f).name}")


if __name__ == "__main__":
    main(sys.argv[1:])
