#!/usr/bin/env python3
"""Validate PSG tuning-table and key detection against the PT3 modules they came from.

    python3 -I scripts/validate_tuning_detection.py PT3_DIR PSG_DIR [UZX_TESTS]

Runs the C++ detector over PSG_DIR (the TuningDetector test's corpus report, enabled
by UZX_TUNING_CORPUS) and joins each result with PT3_DIR/<stem>.pt3, read by
scripts/pt3_info.py. The PT3 header names the tone table the tune was written for,
and its patterns give the written notes, so the report shows per tune:

  table   the PT3 table (0 PT, 1 ST, 2 ASM, 3 REAL) and whether detection matched it;
          a matched table names every note as the composer wrote it
  shift   for a mismatched table, the semitones between written and detected pitch
          classes: +1 is PT #0 read as PT #3, which share all but one period
  key     the key of the written notes and the detected key, by the same profiles;
          ornaments and auto-envelope ratios sound notes that were never written,
          so some disagreement is expected

Pairs come from scripts/zxart_fetch_pt3.py and scripts/pt3_to_psg.py.
"""
import math
import os
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.dont_write_bytecode = True  # importing pt3_info must not leave __pycache__ in scripts/
sys.path.insert(0, str(Path(__file__).resolve().parent))
import pt3_info  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TESTS = ROOT / "build/src/uZXTests_artefacts/Debug/uZXTests"
EXPECTED = {0: "CustomPT_0_PT", 1: "CustomPT_1_ST", 2: "CustomPT_2_ASM", 3: "CustomPT_3_REAL"}
NAMES = pt3_info.NOTE_NAMES
KK_MAJOR = [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88]
KK_MINOR = [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17]


def pearson(xs, ys):
    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
    cov = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    var = math.sqrt(sum((x - mx) ** 2 for x in xs) * sum((y - my) ** 2 for y in ys))
    return cov / var if var else 0.0


def key_of(pcs):
    """(tonic, minor) by Krumhansl-Kessler profiles, the same rule as the C++ detector."""
    return max(((t, m) for t in range(12) for m in (0, 1)),
               key=lambda k: pearson(pcs, [(KK_MINOR if k[1] else KK_MAJOR)[(pc - k[0]) % 12] for pc in range(12)]))


def best_shift(written, detected):
    """Semitones to add to written pitch classes to best match the detected ones."""
    return max(range(-5, 7), key=lambda s: pearson([written[(pc - s) % 12] for pc in range(12)], detected))


def detector_report(tests, psg_dir):
    env = dict(os.environ, UZX_TUNING_CORPUS=str(psg_dir))
    out = subprocess.run([str(tests), "TuningDetector"], env=env, capture_output=True, text=True).stdout
    rows = {}
    for line in out.splitlines():
        if line.startswith("TUNING\t"):
            _, stem, grid, a4, share, ttie, tonic, minor, ktie, pcs = line.split("\t")
            rows[stem] = dict(grid=grid, a4=float(a4), share=float(share), table_tie=ttie == "1",
                              key=(int(tonic), int(minor)), key_tie=ktie == "1",
                              pcs=[float(x) for x in pcs.split(",")])
    return rows


def key_name(key):
    return f"{NAMES[key[0]]} {'minor' if key[1] else 'major'}"


def main(pt3_dir, psg_dir, tests):
    detected = detector_report(tests, psg_dir)
    if not detected:
        sys.exit(f"no TUNING lines from {tests}: build uZXTests first")
    stats = Counter()
    for stem, d in sorted(detected.items()):
        pt3 = pt3_dir / f"{stem}.pt3"
        info = pt3_info.info(pt3) if pt3.exists() else None
        if info is None or not info["notes"]:
            continue
        written = pt3_info.pitch_classes(info["notes"])
        expected = EXPECTED.get(info["table"], f"table{info['table']}")
        table_ok = d["grid"] == expected
        shift = best_shift(written, d["pcs"])
        true_key = key_of(written)
        key_ok = d["key"] == true_key
        stats["tunes"] += 1
        stats[f"table {info['table']}"] += 1
        stats[f"table {info['table']} matched"] += table_ok
        stats["named right"] += table_ok or shift == 0
        stats["key right"] += key_ok
        stats["key right, relative"] += key_ok or (d["key"][0] - true_key[0]) % 12 in (3, 9) and d["key"][1] != true_key[1]
        flag = ("T" if not table_ok else " ") + ("S" if not table_ok and shift else " ") + ("K" if not key_ok else " ")
        print(f"{flag} 3.{info['version']} t{info['table']} {d['grid']:<17} {d['share']:.2f}"
              f"{'?' if d['table_tie'] else ' '} shift={shift:+d} key {key_name(true_key):<9} -> "
              f"{key_name(d['key']):<9}{'?' if d['key_tie'] else ' '} {stem[:50]}")
    print()
    for k, v in sorted(stats.items()):
        print(f"{k:<24} {v}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    main(Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_TESTS)
