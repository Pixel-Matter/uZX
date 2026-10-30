#!/usr/bin/env python3
"""Read structural facts from a ProTracker 3 module that a PSG dump loses.

    python3 -I scripts/pt3_info.py FILE.pt3 [...]

Prints the initial speed, the pattern lengths in play order (channel A rows,
honouring the per-channel note-duration command) and any in-pattern speed
changes. Used as weak labels for tempo and meter detection.

The row decoder is approximate: pattern lengths are reliable, but it skips
effect parameters by fixed sizes, so an odd speed value (like 255) means a
misread parameter, not a real speed change.
"""
import sys
from collections import Counter
from pathlib import Path

# parameter bytes that follow the row-ending byte for special commands 1..9
EFFECT_PARAM_BYTES = {1: 3, 2: 5, 3: 1, 4: 1, 5: 2, 6: 0, 7: 0, 8: 3, 9: 1}


def read_word(d, i):
    return d[i] | d[i + 1] << 8


def channel_rows(d, ptr, limit=4096):
    """Counts rows of one channel until its 0x00 terminator. Returns (rows, speeds)
    where speeds lists (row, new_speed) for every set-speed command."""
    rows, skip, i, speeds = 0, 1, ptr, []
    pending = []
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
    return None, speeds


def info(path):
    d = Path(path).read_bytes()
    if not d.startswith(b"ProTracker 3.") and not d.startswith(b"Vortex Tracker II"):
        return None
    speed = d[100]
    patterns_ptr = read_word(d, 103)
    positions = []
    i = 201
    while i < len(d) and d[i] != 0xFF:
        positions.append(d[i] // 3)
        i += 1
    lengths, speed_changes = [], []
    for p in positions:
        ch_a = read_word(d, patterns_ptr + p * 6)
        rows, speeds = channel_rows(d, ch_a)
        if rows is None:
            return None
        lengths.append(rows)
        speed_changes += [(p, r, s) for r, s in speeds]
    return dict(speed=speed, lengths=lengths, speed_changes=speed_changes)


def main(paths):
    for f in paths:
        r = info(f)
        if r is None:
            print(f"? {Path(f).name}")
            continue
        common = Counter(r["lengths"]).most_common(3)
        speeds = sorted({s for _, _, s in r["speed_changes"]})
        print(f"speed={r['speed']} patterns={common} speed_cmds={speeds} {Path(f).name}")


if __name__ == "__main__":
    main(sys.argv[1:])
