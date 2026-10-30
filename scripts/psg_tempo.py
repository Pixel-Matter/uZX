#!/usr/bin/env python3
"""Prototype tempo detector for AY register dumps (.psg).

    python3 -I scripts/psg_tempo.py FILE_OR_DIR [...]

Trackers write every note on a row and a row lasts an integer number of frames
("speed"), so the job is to find that integer, an optional two-value swing
(6-7-6-7...), and how many rows make a beat. Prints one line per file:
    speed  rows/beat  fpb  bpm  conf  file
"""
import sys
from pathlib import Path

import numpy as np

MIN_ONSET = 4        # volume jump (0..15) that counts as a note start
MIN_GRID_SHARE = 0.8  # chance-corrected share of onset energy that must land on the row grid
MIN_ENERGY = 600.0    # below this (about 40 full-volume hits) the tune is too sparse to judge
SPEEDS = range(2, 17)
BPM_RANGE = (80.0, 170.0)  # halve or double rows per beat to land here; 187.5 becomes 93.75
HARMONIC_TOLERANCE = 0.02  # energy a doubled period may lose and still be preferred


def read_psg(path):
    """Returns (fps, frames) where frames is a uint8 array [n, 15]: register state plus,
    in column 14, whether R13 was written in that frame (any write retriggers the envelope)."""
    data = Path(path).read_bytes()
    if data[:4] != b"PSG\x1a":
        raise ValueError("not a PSG file")
    fps = data[5] if data[4] >= 10 and data[5] else 50
    regs = np.zeros(15, np.uint8)  # [14] flags a R13 write in this frame
    frames = []
    i = 16
    n = len(data)
    while i < n:
        b = data[i]
        if b == 0xFF:
            i += 1
            regs[14] = 0
            while i < n and data[i] < 16:
                reg, val = data[i], data[i + 1]
                if reg < 14:
                    regs[reg] = val
                if reg == 13:
                    regs[14] = 1
                i += 2
            frames.append(regs.copy())
        elif b == 0xFE:
            regs[14] = 0
            for _ in range(data[i + 1] * 4):
                frames.append(regs.copy())
            i += 2
            # register pairs right after a pause (no 0xFF) belong to the last paused frame
            while i < n and data[i] < 16:
                reg, val = data[i], data[i + 1]
                if reg < 14:
                    regs[reg] = val
                if reg == 13:
                    regs[14] = 1
                i += 2
                if frames:
                    frames[-1] = regs.copy()
        elif b == 0xFD:
            break
        else:
            i += 1
    return fps, np.array(frames, np.uint8)


def onset_energy(frames):
    """Per-frame note-start energy. A volume jump counts only with new-note evidence:
    the channel was silent, its tone period changed, or the envelope shape was
    rewritten (retrigger). Jumps inside a sounding note (sample attack stages,
    tremolo) are ignored."""
    vol = (frames[:, 8:11] & 0x0F).astype(np.int32)
    env_mode = (frames[:, 8:11] & 0x10) != 0
    n = len(frames)
    jump = np.zeros_like(vol)
    jump[1:] = vol[1:] - vol[:-1]
    was_silent = np.zeros_like(vol, bool)
    was_silent[1:] = vol[:-1] == 0
    period = frames[:, 0:6:2].astype(np.int32) | (frames[:, 1:6:2].astype(np.int32) & 0x0F) << 8
    period_changed = np.zeros_like(vol, bool)
    period_changed[1:] = period[1:] != period[:-1]
    r13_written = frames[:, 14] != 0  # any R13 write restarts the envelope
    evidence = was_silent | period_changed | r13_written[:, None]
    hit = np.where((jump >= MIN_ONSET) & evidence, jump, 0)
    energy = hit.sum(axis=1).astype(float)
    # envelope-driven channels keep volume 0 in the register; count a R13 write as a full hit
    energy += 15.0 * (r13_written & env_mode.any(axis=1))
    return energy


def grid_share(energy, period, n_phases=1):
    """Share of onset energy that lies on the best n_phases residues mod period,
    corrected for the share random off-grid energy would land there by chance.
    Returns (share, phases)."""
    by_phase = np.bincount(np.arange(len(energy)) % period, weights=energy, minlength=period)
    phases = np.argsort(by_phase)[::-1][:n_phases]
    raw = by_phase[phases].sum() / energy.sum()
    chance = n_phases / period
    return (raw - chance) / (1.0 - chance), sorted(int(p) for p in phases)


def detect_speed(energy):
    """Returns (speed_a, speed_b, phase, share). speed_b == speed_a when no swing.

    Every divisor of the true speed also holds, so the largest period wins, but a
    multiple only beats its divisors if it keeps (almost) all of their energy;
    otherwise the tune just has most of its notes on even rows."""
    share = {p: grid_share(energy, p) for p in SPEEDS}
    best = None
    for p in SPEEDS:
        g, (phase,) = share[p]
        divisors = [share[d][0] for d in SPEEDS if d < p and p % d == 0]
        if g >= MIN_GRID_SHARE and g >= max(divisors, default=0.0) - HARMONIC_TOLERANCE:
            best = (p, p, phase, g)
    if best:
        return best
    # two-value swing: period a+b with two phases a apart, a and b near each other
    for total in range(5, 33):
        g, (p0, p1) = grid_share(energy, total, 2)
        a, b = p1 - p0, total - (p1 - p0)
        if a in SPEEDS and b in SPEEDS and abs(a - b) <= 2 and g >= MIN_GRID_SHARE:
            if best is None or g > best[3]:
                best = (a, b, p0, g)
    return best


def detect_rows_per_beat(energy, speed_a, speed_b, phase, fps):
    """Picks 3 or 4 rows per beat from the row-accent autocorrelation, then doubles or
    halves to land inside BPM_RANGE."""
    row_len = (speed_a + speed_b) / 2.0
    row_starts = np.arange(phase, len(energy), row_len).round().astype(int)
    rows = energy[np.clip(row_starts, 0, len(energy) - 1)]
    rows = rows - rows.mean()
    def ac(lag):
        return float((rows[:-lag] * rows[lag:]).sum()) if lag < len(rows) else 0.0
    rpb = 3 if ac(3) > 0 and ac(3) > ac(4) * 1.2 else 4   # duple unless triplets clearly win
    bpm = fps * 60.0 / (row_len * rpb)
    while bpm < BPM_RANGE[0] and rpb > 1:
        rpb //= 2; bpm *= 2
    while bpm > BPM_RANGE[1]:
        rpb *= 2; bpm /= 2
    return rpb, bpm


def analyse(path):
    fps, frames = read_psg(path)
    energy = onset_energy(frames)
    if energy.sum() < MIN_ENERGY:
        return None
    found = detect_speed(energy)
    if not found:
        return None
    a, b, phase, conc = found
    rpb, bpm = detect_rows_per_beat(energy, a, b, phase, fps)
    fpb = (a + b) / 2.0 * rpb
    return dict(fps=fps, speed=(a, b), phase=phase, conc=conc, rpb=rpb, fpb=fpb, bpm=bpm)


def main(args):
    files = []
    for a in args:
        p = Path(a)
        files += sorted(p.glob("*.psg")) if p.is_dir() else [p]
    for f in files:
        r = analyse(f)
        if r is None:
            print(f"{'?':>6} {'?':>3} {'?':>5} {'?':>6} {'-':>5}  {f.name}")
            continue
        speed = f"{r['speed'][0]}" if r['speed'][0] == r['speed'][1] else f"{r['speed'][0]}-{r['speed'][1]}"
        print(f"{speed:>6} {r['rpb']:>3} {r['fpb']:>5.1f} {r['bpm']:>6.1f} {r['conc']:>5.2f}  {f.name}")


if __name__ == "__main__":
    main(sys.argv[1:] or ["."])
