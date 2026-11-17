#!/usr/bin/env python3
"""Validate PSG tempo detection against PT3 header speeds, or against tempos set by ear.

    python3 -I scripts/validate_tempo_detection.py PT3_DIR PSG_DIR [UZX_TESTS]
    python3 -I scripts/validate_tempo_detection.py --labels [PSG_DIR ...]
    python3 -I scripts/validate_tempo_detection.py --rows MODULE_DIR [DUMP_DIR]

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

  --rows     scores the row grid detected from the registers alone against the rows the
             tracker modules in MODULE_DIR know (the PsgTempo test's row report, enabled by
             UZX_TEMPO_MODULES): exact, coarser (module rows, some skipped), finer (every
             module row and more), wrong, or none, apart for the tunes a fixed grid fits and
             the ones that slip or change speed; DUMP_DIR also gets every module as a PSG with its row starts
             (<name>.rows), for scripts that study the registers against the rows

Pairs come from scripts/zxart_fetch_pt3.py and scripts/pt3_to_psg.py.
"""
import csv
import fractions
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
ROW_FIELDS = ["frames", "rows", "own", "grid", "fixed", "got", "share", "recall", "precision", "own_rpb", "own_fpb", "rpb", "fpb"]
SINGLE_GRID = 0.9  # a fixed grid fits a module's rows this well: the tune has one tempo and no slips
COVERED = 0.9      # share of rows that must agree for the detected grid to count as the module's


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


def row_verdict(r):
    """Against the module's row starts. exact: the detected rows are the module's rows; coarser:
    they are module rows but skip some (notes on every other row); finer: they hit every module
    row and more; else wrong. A tune no fixed grid fits can't be exact."""
    if r["got"] is None:
        return "none"
    covers, clean = r["recall"] >= COVERED, r["precision"] >= COVERED
    return "exact" if covers and clean else "coarser" if clean else "finer" if covers else "wrong"


def check_rows(module_dir, dump_dir, tests):
    env = dict(os.environ, UZX_TEMPO_MODULES=str(module_dir))
    if dump_dir:
        dump_dir.mkdir(parents=True, exist_ok=True)
        env["UZX_TEMPO_DUMP"] = str(dump_dir)
    out = subprocess.run([str(tests), "PsgTempo"], env=env, capture_output=True, text=True).stdout
    tunes = []
    for line in out.splitlines():
        if not line.startswith("ROWS\t"):
            continue
        parts = line.split("\t")
        r = dict(zip(ROW_FIELDS, parts[2:]), name=parts[1])
        r["got"] = None if r["got"] == "-" else r["got"]
        for k in ("fixed", "share", "recall", "precision"):
            r[k] = float(r[k])
        r["verdict"] = row_verdict(r)
        r["single"] = r["fixed"] >= SINGLE_GRID
        tunes.append(r)
    if not tunes:
        sys.exit(f"no ROWS lines from {tests}: build uZXTests first")

    for verdict in ("wrong", "none", "finer", "coarser"):
        for r in sorted((r for r in tunes if r["verdict"] == verdict), key=lambda r: -r["fixed"]):
            print(f"{verdict.upper():8} grid={r['grid']:16} fit={r['fixed']:.2f} app={r['own']:9} got={r['got'] or '-':9} "
                  f"share={r['share']:.2f} recall={r['recall']:.2f} precision={r['precision']:.2f} "
                  f"fpb {r['own_fpb']}->{r['fpb']}  {r['name'][:50]}")
    stats = Counter(r["verdict"] for r in tunes)
    single = [r for r in tunes if r["single"]]
    print(f"\n{len(tunes)} modules; a fixed grid fits the rows ({SINGLE_GRID:.0%}) in {len(single)}")
    for name, group in (("all", tunes), ("single-grid", single), ("slips or speed changes", [r for r in tunes if not r["single"]])):
        c = Counter(r["verdict"] for r in group)
        print(f"  {name:22} " + "  ".join(f"{v} {c[v]}" for v in ("exact", "coarser", "finer", "wrong", "none")))
    answered = [r for r in tunes if r["got"] is not None]
    same = sum(r["fpb"] == r["own_fpb"] for r in answered)
    print(f"  frames per beat as from the module's own rows: {same}/{len(answered)} of the tunes with a tempo")
    return stats


if __name__ == "__main__":
    args = sys.argv[1:]
    if args[:1] == ["--rows"] and len(args) >= 2:
        check_rows(Path(args[1]), Path(args[2]) if len(args) > 2 else None, DEFAULT_TESTS)
    elif args[:1] == ["--labels"]:
        check_labels([Path(p) for p in args[1:]] or DEFAULT_LABEL_DIRS, DEFAULT_TESTS)
    elif len(args) >= 2:
        check_pt3(Path(args[0]), Path(args[1]), Path(args[2]) if len(args) > 2 else DEFAULT_TESTS)
    else:
        sys.exit(__doc__)
