#!/usr/bin/env python3
"""Validate the PSG tempo detector against PT3 header speeds, or against tempos set by ear.

    python3 -I scripts/psg_tempo_validate.py PT3_DIR PSG_DIR [UZX_TESTS]
    python3 -I scripts/psg_tempo_validate.py --labels PSG_DIR [PSG_DIR ...]

The --labels form compares the detected BPM with scripts/psg_tempo_labels.csv for
every labelled file found in the given folders.

Converts each .pt3 to .psg with zxtune123 (skipped when the .psg exists), runs
the C++ detector over PSG_DIR through the test binary (UZX_PSG_TEMPO_DIR mode),
and compares the detected speed with the PT3 "delay" byte. A detected 2-value
swing or groove counts as a match when its mean row equals the delay; frames per beat that
are a whole number of rows count as harmonic. UZX_TESTS defaults to the Debug
build of uZXTests.
"""
import csv
import os
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ZXTUNE = ROOT / ".cache/tools/zxtune/bin/darwin/release/zxtune123"
UZX_TESTS = ROOT / "build/src/uZXTests_artefacts/Debug/uZXTests"
LINE = re.compile(r"^(?P<rows>[\d-]+) rpb=(?P<rpb>\d+) fpb=(?P<fpb>\d+) bpm=(?P<bpm>[\d.]+) g=[\d.]+ "
                  r"sig=(?P<sig>\d) clear=(?P<clear>\d) first=\d+ silent=\d (?P<name>.+)$")


def convert(pt3, psg):
    if psg.exists():
        return True
    tmp = psg.with_name("convert_tmp.psg")  # zxtune's option map is comma-separated, names may contain commas
    tmp.unlink(missing_ok=True)
    subprocess.run([str(ZXTUNE), "--convert", f"mode=psg,filename={tmp}", "--", str(pt3)],
                   capture_output=True, text=True, timeout=120)
    if tmp.exists():
        tmp.rename(psg)
    return psg.exists()


def detect_all(psg_dir, tests):
    """Runs the C++ detector over a folder: {file name: match or None when undetected}."""
    out = subprocess.run([str(tests), "PsgTempo"], env={**os.environ, "UZX_PSG_TEMPO_DIR": str(psg_dir)},
                         capture_output=True, text=True).stdout
    results = {}
    for line in out.splitlines():
        if m := LINE.match(line):
            results[m["name"]] = m
        elif line.startswith("? "):
            results[line[2:]] = None
    return results


def main(src, dst, tests):
    dst.mkdir(parents=True, exist_ok=True)
    failed = 0
    for pt3 in sorted(src.glob("*.pt3")):
        if not convert(pt3, dst / (pt3.stem + ".psg")):
            failed += 1
            print(f"CONVERT-FAIL {pt3.name}")
    detected = detect_all(dst, tests)
    ok = harmonic = miss = unknown = 0
    bpms, sigs = Counter(), Counter()
    for pt3 in sorted(src.glob("*.pt3")):
        name = pt3.stem + ".psg"
        if name not in detected:
            continue
        expected = pt3.read_bytes()[100]
        m = detected[name]
        if m is None:
            unknown += 1
            print(f"UNKNOWN  exp={expected:<2} {pt3.name[:60]}")
            continue
        rows = [int(r) for r in m["rows"].split("-")]
        rpb, fpb, bpm = int(m["rpb"]), int(m["fpb"]), float(m["bpm"])
        bpms[round(bpm)] += 1
        sigs[f"{m['sig']}/4{'' if m['clear'] == '1' else '?'}"] += 1
        line = f"exp={expected:<2} got={m['rows']} rpb={rpb:<2} fpb={fpb:<3} bpm={bpm:5.1f} {pt3.name[:60]}"
        if sum(rows) / len(rows) == expected:
            ok += 1
        elif fpb % expected == 0:
            harmonic += 1  # frames per beat is still a whole number of rows
            print("HARMONIC " + line)
        else:
            miss += 1
            print("MISMATCH " + line)
    total = ok + harmonic + miss + unknown
    print(f"\nspeed ok {ok}/{total}  harmonic (fpb = whole rows) {harmonic}  mismatch {miss}  "
          f"unknown {unknown}  convert failed {failed}")
    print("bpm histogram:", sorted(bpms.items()))
    print("meter (? = unclear, left as is):", sorted(sigs.items()))


def check_labels(dirs, tests=UZX_TESTS):
    """Prints detected against labelled tempo (a match is within 1%) and meter. An
    unclear meter counts as 4/4, the default an imported edit keeps."""
    with open(Path(__file__).parent / "psg_tempo_labels.csv") as f:
        labels = {r["file"]: r for r in csv.DictReader(line for line in f if not line.startswith("#"))}
    tempo_ok = tempo_total = meter_ok = meter_total = 0
    for d in dirs:
        for name, m in detect_all(d, tests).items():
            label = labels.get(name)
            if label is None:
                continue
            got_bpm = float(m["bpm"]) if m else None
            got_meter = (int(m["sig"]) if m["clear"] == "1" else 4) if m else None
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
                print(f"{'  '.join(marks):60} {name[:55]}")
    print(f"\nlabelled tempo matches {tempo_ok}/{tempo_total}, meter matches {meter_ok}/{meter_total}")


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--labels":
        check_labels([Path(p) for p in sys.argv[2:]])
        sys.exit()
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    main(Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]) if len(sys.argv) > 3 else UZX_TESTS)
