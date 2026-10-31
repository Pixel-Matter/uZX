#!/usr/bin/env python3
"""Validate PSG tempo detection against PT3 header speeds, or against tempos set by ear.

    python3 -I scripts/validate_tempo_detection.py PT3_DIR PSG_DIR [UZX_TESTS]
    python3 -I scripts/validate_tempo_detection.py --labels [PSG_DIR ...]

Runs the C++ detector over the PSG folders (the PsgTempo test's corpus report, enabled
by UZX_TEMPO_CORPUS) and joins each result:

  PT3 form   converts each PT3_DIR/<stem>.pt3 to PSG_DIR/<stem>.psg with zxtune123 when
             missing, reads the PT3 "delay" byte as the expected row speed, and reports
             exact speeds, harmonics (frames per beat still a whole number of rows),
             mismatches and tunes without a tempo; a swing or groove counts as exact
             when its mean row equals the delay
  --labels   compares with scripts/psg_tempo_labels.csv (tempo within 1%, and meter; an
             unclear meter counts as 4/4, which an imported edit keeps); the folders
             default to tests/fixtures/psg and .cache/zxart/psg

Pairs come from scripts/zxart_fetch_pt3.py and scripts/pt3_to_psg.py.
"""
import csv
import os
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.dont_write_bytecode = True  # importing pt3_to_psg must not leave __pycache__ in scripts/
sys.path.insert(0, str(Path(__file__).resolve().parent))
import pt3_to_psg  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TESTS = ROOT / "build/src/uZXTests_artefacts/Debug/uZXTests"
DEFAULT_LABEL_DIRS = [ROOT / "tests/fixtures/psg", ROOT / ".cache/zxart/psg"]
FIELDS = ["rows", "rpb", "fpb", "bpm", "share", "sig", "clear", "first", "silent"]


def detector_report(psg_dir, tests):
    """{stem: dict of FIELDS, or None when the detector found no tempo}."""
    env = dict(os.environ, UZX_TEMPO_CORPUS=str(psg_dir))
    out = subprocess.run([str(tests), "PsgTempo"], env=env, capture_output=True, text=True).stdout
    rows = {}
    for line in out.splitlines():
        if not line.startswith("TEMPO\t"):
            continue
        parts = line.split("\t")
        if parts[2] == "-":
            rows[parts[1]] = None
            continue
        r = dict(zip(FIELDS, parts[2:]))
        r["row_lengths"] = [int(x) for x in r["rows"].split("-")]
        for k in ("rpb", "fpb", "sig", "clear", "first", "silent"):
            r[k] = int(r[k])
        r["bpm"], r["share"] = float(r["bpm"]), float(r["share"])
        rows[parts[1]] = r
    return rows


def check_pt3(pt3_dir, psg_dir, tests):
    psg_dir.mkdir(parents=True, exist_ok=True)
    zxtune = pt3_to_psg.DEFAULT_ZXTUNE
    failed = sum(not pt3_to_psg.convert(zxtune, pt3, psg_dir / (pt3.stem + ".psg")) for pt3 in sorted(pt3_dir.glob("*.pt3")))
    detected = detector_report(psg_dir, tests)
    if not detected:
        sys.exit(f"no TEMPO lines from {tests}: build uZXTests first")
    stats, bpms, meters = Counter(), Counter(), Counter()
    for pt3 in sorted(pt3_dir.glob("*.pt3")):
        if pt3.stem not in detected:
            continue
        expected = pt3.read_bytes()[100]
        r = detected[pt3.stem]
        if r is None:
            stats["unknown"] += 1
            print(f"UNKNOWN  exp={expected:<2} {pt3.name[:60]}")
            continue
        bpms[round(r["bpm"])] += 1
        meters[f"{r['sig']}/4{'' if r['clear'] else '?'}"] += 1
        line = f"exp={expected:<2} got={r['rows']} rpb={r['rpb']:<2} fpb={r['fpb']:<3} bpm={r['bpm']:6.2f} {pt3.name[:60]}"
        if sum(r["row_lengths"]) / len(r["row_lengths"]) == expected:
            stats["speed ok"] += 1
        elif r["fpb"] % expected == 0:
            stats["harmonic"] += 1
            print("HARMONIC " + line)
        else:
            stats["mismatch"] += 1
            print("MISMATCH " + line)
    total = sum(stats.values())
    print(f"\nspeed ok {stats['speed ok']}/{total}  harmonic (fpb = whole rows) {stats['harmonic']}  "
          f"mismatch {stats['mismatch']}  unknown {stats['unknown']}  convert failed {failed}")
    print("bpm histogram:", sorted(bpms.items()))
    print("meter (? = unclear, left as is):", sorted(meters.items()))


def check_labels(dirs, tests):
    with open(Path(__file__).parent / "psg_tempo_labels.csv") as f:
        labels = {Path(r["file"]).stem: r for r in csv.DictReader(line for line in f if not line.startswith("#"))}
    tempo_ok = tempo_total = meter_ok = meter_total = 0
    for d in dirs:
        if not d.is_dir():
            continue
        for stem, r in sorted(detector_report(d, tests).items()):
            label = labels.get(stem)
            if label is None:
                continue
            got_bpm = r["bpm"] if r else None
            got_meter = (r["sig"] if r["clear"] else 4) if r else None
            marks = []
            if label["bpm"]:
                want = float(label["bpm"])
                hit = got_bpm is not None and abs(got_bpm - want) <= 0.01 * want
                tempo_ok += hit
                tempo_total += 1
                marks.append(f"{'ok' if hit else 'MISS'} bpm {want:6.1f} got {'-' if got_bpm is None else f'{got_bpm:6.1f}'}")
            if label.get("meter"):
                hit = got_meter == int(label["meter"])
                meter_ok += hit
                meter_total += 1
                marks.append(f"{'ok' if hit else 'MISS'} meter {label['meter']}/4 got {got_meter}/4")
            if marks:
                print(f"{'  '.join(marks):60} {stem[:55]}")
    print(f"\nlabelled tempo matches {tempo_ok}/{tempo_total}, meter matches {meter_ok}/{meter_total}")


if __name__ == "__main__":
    args = sys.argv[1:]
    if args[:1] == ["--labels"]:
        check_labels([Path(p) for p in args[1:]] or DEFAULT_LABEL_DIRS, DEFAULT_TESTS)
    elif len(args) >= 2:
        check_pt3(Path(args[0]), Path(args[1]), Path(args[2]) if len(args) > 2 else DEFAULT_TESTS)
    else:
        sys.exit(__doc__)
