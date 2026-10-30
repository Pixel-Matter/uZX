#!/usr/bin/env python3
"""Prototype: detect the tuning table and key of PSG files.

Reads the built-in period tables from TuningRegistry.cpp, so there is one copy of
that knowledge. For each PSG file it builds a frame-weighted histogram of audible
periods (tone channels, plus the envelope pitch when a channel uses envelope
volume), scores every table by the share of note-frames whose period is in the
table, falls back to an equal-temperament fit when no table fits, maps notes to
pitch classes through the winning grid and ranks the 24 major/minor keys with
Krumhansl-Kessler profiles.

All matching is exact in period space: a tracker writes the table value, whatever
its rounding error in Hz, so high notes need no cents tolerance. Envelope periods
are an exception: trackers derive them as tone / 16 rounded, so they match within
half an envelope step (±8 tone-period units).

    python3 -B scripts/tuning_detect_proto.py path/to/*.psg [--summary] [--hist] [--top N]
"""

import argparse
import math
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "src/models/tuning/TuningRegistry.cpp"
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
FIRST_NOTE, NUM_NOTES = 24, 96  # every registry table starts at C1 and spans 8 octaves
HEARABLE_MIN, HEARABLE_MAX = 8, 4095
ENV_TOLERANCE = 8  # half an envelope step, in tone-period units
ZX_CLOCK, PENTAGON_CLOCK = 1773400.0, 1750000.0
TABLE_TIE_GAP, KEY_TIE_GAP = 0.05, 0.05
FALLBACK_BELOW = 0.2  # best table share under this: try the 12TET fit

# Krumhansl-Kessler probe-tone profiles for a major and a minor key on C.
KK_MAJOR = [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88]
KK_MINOR = [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17]


def tet_table(a4, clock):
    """period -> MIDI note for equal temperament, like AutoTuning computes it."""
    table = {}
    for note in range(FIRST_NOTE, FIRST_NOTE + NUM_NOTES):
        table.setdefault(round(clock / 16 / (a4 * 2 ** ((note - 69) / 12))), note)
    return table


def parse_tables():
    """name -> {period: midi_note} for every explicit table in the registry."""
    src = REGISTRY.read_text()
    tables = {}
    for m in re.finditer(r"case BuiltinTuningType::(\w+):(.*?)(?=case BuiltinTuningType::|\Z)", src, re.S):
        vec = re.search(r"std::vector<int>\s*\{(.*?)\}", m.group(2), re.S)
        if not vec:
            continue
        table = {}
        for i, p in enumerate(int(n) for n in re.findall(r"\b\d+\b", re.sub(r"//.*", "", vec.group(1)))):
            table.setdefault(p, FIRST_NOTE + i)
        tables[m.group(1)] = table
    tables["TET_ZX_440"] = tet_table(440.0, ZX_CLOCK)  # the registry's EqualTemperament entry
    return tables


def read_psg_frames(path):
    """Yields the accumulated 14-register state after every frame."""
    data = path.read_bytes()
    if data[:4] != b"PSG\x1a":
        raise ValueError(f"{path.name}: not a PSG file")
    regs = [0] * 14
    i, n = 16, len(data)
    expect_frame = False
    while i < n:
        if expect_frame:
            while i + 1 < n and data[i] <= 0x0F:
                if data[i] < 14:
                    regs[data[i]] = data[i + 1]
                i += 2
            yield list(regs)
            expect_frame = False
            continue
        marker = data[i]
        i += 1
        if marker == 0xFE:
            for _ in range(data[i] * 4):
                yield list(regs)
            i += 1
            expect_frame = True
        elif marker == 0xFF:
            expect_frame = True
        else:
            return  # 0xFD end marker or damaged tail


def period_histograms(path):
    """(tone Counter, envelope Counter, frame count); mirrors visitFrameNotes."""
    tone, env = Counter(), Counter()
    frames = 0
    for regs in read_psg_frames(path):
        frames += 1
        mixer = regs[7]
        env_period = 16 * max(1, regs[11] | (regs[12] << 8))
        env_used = False
        for ch in range(3):
            vol = regs[8 + ch]
            tone_on = not (mixer >> ch) & 1
            env_mode = bool(vol & 16)
            audible = (vol & 15) > 0 or env_mode
            period = regs[2 * ch] | ((regs[2 * ch + 1] & 15) << 8)
            if env_mode and HEARABLE_MIN <= env_period <= HEARABLE_MAX:
                env_used = True
            if tone_on and audible and period >= HEARABLE_MIN:
                tone[period] += 1
        if env_used:
            env[env_period] += 1
    return tone, env, frames


def hit_share(tone, env, table):
    """Share of note-frames on the grid: tone exact, envelope within ±ENV_TOLERANCE."""
    hits = sum(w for p, w in tone.items() if p in table)
    hits += sum(w for p, w in env.items() if any(p + d in table for d in range(-ENV_TOLERANCE, ENV_TOLERANCE + 1)))
    return hits / (sum(tone.values()) + sum(env.values()))


def group_tables(tables):
    """Tables sharing over 90% of their periods are one grid (PT#0 vs PT#3: a semitone
    shift, 95 of 96 periods). The later registry table names the group."""
    groups = []
    for name, table in tables.items():
        for g in groups:
            ref = set(tables[g[0]])
            if len(ref & set(table)) / len(ref | set(table)) > 0.9:
                g.insert(0, name)
                break
        else:
            groups.append([name])
    return groups


def score_tables(tone, env, tables):
    """[(share, label, table)] best first; label lists duplicates as 'later = earlier'."""
    out = []
    for names in group_tables(tables):
        out.append((hit_share(tone, env, tables[names[0]]), " = ".join(names), tables[names[0]]))
    out.sort(key=lambda t: -t[0])
    return out


def fit_tet(tone, env, clock=ZX_CLOCK, steps=200):
    """Sweep A4 over one semitone around 440 Hz and score exact hits against the rounded
    equal-temperament table. A4 and A4 * 2^(1/12) give the same period set, so one
    semitone is the whole search space. Returns (share, a4, a4_low, a4_high): the plateau
    of A4 values within 0.01 of the best share is the fit's real precision; high notes
    widen it, they never fake precision."""
    scored = []
    for k in range(steps):
        a4 = 440.0 * 2 ** ((k / steps - 0.5) / 12)
        scored.append((hit_share(tone, env, tet_table(a4, clock)), a4))
    best = max(scored)
    plateau = [a4 for s, a4 in scored if s >= best[0] - 0.01]
    centre = plateau[len(plateau) // 2]
    return best[0], centre, min(plateau), max(plateau)


def nearest_note(table_log, log_p):
    """Fractional MIDI note of a log2-period against a grid, for off-grid periods."""
    lo, hi = 0, len(table_log) - 1
    while lo < hi:
        mid = (lo + hi) // 2
        if table_log[mid][0] < log_p:
            hi = mid
        else:
            lo = mid + 1
    k = min(range(max(0, lo - 1), min(len(table_log), lo + 2)), key=lambda i: abs(table_log[i][0] - log_p))
    lp, note = table_log[k]
    return note + (lp - log_p) * 12.0


def pitch_class_histogram(tone, env, table):
    table_log = sorted(((math.log2(p), n) for p, n in table.items()), reverse=True)
    pcs = [0.0] * 12
    for p, w in (tone + env).items():
        note = table.get(p)
        if note is None:
            note = round(nearest_note(table_log, math.log2(p)))
        pcs[note % 12] += w
    total = sum(pcs)
    return [x / total for x in pcs]


def pearson(xs, ys):
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    cov = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    var = math.sqrt(sum((x - mx) ** 2 for x in xs)) * math.sqrt(sum((y - my) ** 2 for y in ys))
    return cov / var if var else 0.0


def score_keys(pcs):
    """[(correlation, 'A minor')] for the 24 major/minor keys, best first."""
    out = []
    for tonic in range(12):
        for name, profile in (("major", KK_MAJOR), ("minor", KK_MINOR)):
            rotated = [profile[(pc - tonic) % 12] for pc in range(12)]
            out.append((round(pearson(pcs, rotated), 4), f"{NOTE_NAMES[tonic]} {name}"))
    out.sort(reverse=True)
    return out


def bar(x, width=40):
    return "#" * int(round(x * width))


def folded_histogram(tone, env, table, bins_per_semitone=5, width=50):
    """Pitch-class histogram with sub-semitone resolution: every period is placed at its
    fractional note on the candidate grid (so grid notes sit exactly on integers, whether
    or not the table is octave-periodic) and folded to one octave. Each period's weight is
    smeared over its quantisation band [p - 0.5, p + 0.5] (an envelope period over ±8), so
    a high note shows as a wide low bump and a bass note as a spike."""
    nbins = 12 * bins_per_semitone
    bins = [0.0] * nbins
    table_log = sorted(((math.log2(p), n) for p, n in table.items()), reverse=True)
    pos = lambda p: nearest_note(table_log, math.log2(p)) % 12 * bins_per_semitone  # noqa: E731
    for hist, half in ((tone, 0.5), (env, ENV_TOLERANCE)):
        for p, w in hist.items():
            lo, hi = pos(p + half), pos(p - half)
            if hi < lo:
                hi += nbins
            span = max(1, int(math.ceil(hi - lo)))
            for k in range(span):  # round the midpoint of each sub-band to its bin
                bins[int(math.floor(lo + (k + 0.5) * (hi - lo) / span + 0.5)) % nbins] += w / span
    peak = max(bins) or 1
    lines = []
    for i, w in enumerate(bins):
        mark = f"{NOTE_NAMES[i // bins_per_semitone]:<2}|" if i % bins_per_semitone == 0 else "   "
        lines.append(f"{i / bins_per_semitone - (i // bins_per_semitone):+4.1f} {mark} {bar(w / peak, width)}")
    return "\n".join(lines)


def detect(tone, env, tables):
    """The detector proper. Returns a dict with the chosen grid, table ranking, 12TET fit,
    keys, and the tie flags the picker UI would act on."""
    ranked = score_tables(tone, env, tables)
    best_share, best_label, grid = ranked[0]
    table_gap = best_share - ranked[1][0]
    tet = None
    if best_share < FALLBACK_BELOW:
        share, a4, lo, hi = fit_tet(tone, env)
        if share > best_share:
            tet = (share, a4, lo, hi)
            best_label, grid, best_share = f"12TET A4={a4:.1f}Hz@ZX ({lo:.1f}-{hi:.1f})", tet_table(a4, ZX_CLOCK), share
            table_gap = share - ranked[0][0]
    pcs = pitch_class_histogram(tone, env, grid)
    keys = score_keys(pcs)
    return {
        "ranked": ranked, "label": best_label, "grid": grid, "share": best_share, "tet": tet,
        "table_tie": table_gap < TABLE_TIE_GAP, "pcs": pcs, "keys": keys,
        "key_tie": keys[0][0] - keys[1][0] < KEY_TIE_GAP,
    }


def analyse(path, tables, top, show_hist, summary):
    tone, env, frames = period_histograms(path)
    if not tone and not env:
        print(f"== {path.name}: no audible notes")
        return
    r = detect(tone, env, tables)
    env_share = sum(env.values()) / (sum(tone.values()) + sum(env.values()))
    if summary:
        flags = ("T" if r["table_tie"] else " ") + ("K" if r["key_tie"] else " ")
        print(f"{flags} {r['share']:5.3f} {r['label']:<36} {r['keys'][0][0]:5.3f} {r['keys'][0][1]:<9} env={env_share:4.2f} {path.name[:40]}")
        return
    print(f"\n== {path.name}  frames={frames} tone-frames={sum(tone.values())} env-frames={sum(env.values())}")
    print(f"  chosen grid: {r['label']}  share={r['share']:.3f}{'  <-- TIE, ask user' if r['table_tie'] else ''}")
    for share, label, _ in r["ranked"][:top]:
        print(f"    {share:6.3f}  {label}")
    if r["tet"] is None:
        share, a4, lo, hi = fit_tet(tone, env)
        print(f"  12TET fit for reference: share={share:.3f} A4={a4:.1f} Hz at ZX clock (plateau {lo:.1f}-{hi:.1f}), {a4 * PENTAGON_CLOCK / ZX_CLOCK:.1f} Hz at Pentagon")
    print(f"  keys: " + ", ".join(f"{n} {c:.3f}" for c, n in r["keys"][:top]) + ("  <-- TIE, ask user" if r["key_tie"] else ""))
    print("  pitch classes:")
    for pc in range(12):
        print(f"    {NOTE_NAMES[pc]:2} {bar(r['pcs'][pc] / max(r['pcs']))}")
    if show_hist:
        print("  pitch classes at sub-semitone resolution on the chosen grid (deviation in semitones):")
        print(folded_histogram(tone, env, r["grid"]))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+", type=Path)
    ap.add_argument("--top", type=int, default=4)
    ap.add_argument("--hist", action="store_true", help="print the folded log-period histogram")
    ap.add_argument("--summary", action="store_true", help="one line per file: flags (T table tie, K key tie), share, grid, key")
    args = ap.parse_args()
    tables = parse_tables()
    if not args.summary:
        print(f"tables: {', '.join(tables)}")
    for f in args.files:
        try:
            analyse(f, tables, args.top, args.hist, args.summary)
        except ValueError as e:
            print(e, file=sys.stderr)


if __name__ == "__main__":
    main()
