#!/usr/bin/env python3
"""Validate the PSG tempo detector against PT3 header speeds.

    python3 -I scripts/psg_tempo_validate.py PT3_DIR PSG_DIR [UZX_TESTS]

Converts each .pt3 to .psg with zxtune123 (skipped when the .psg exists), runs
the C++ detector over PSG_DIR through the test binary (UZX_PSG_TEMPO_DIR mode),
and compares the detected speed with the PT3 "delay" byte. A detected 2-value
swing counts as a match when its mean equals the delay; frames per beat that
are a whole number of rows count as harmonic. UZX_TESTS defaults to the Debug
build of uZXTests.
"""
import os
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ZXTUNE = ROOT / ".cache/tools/zxtune/bin/darwin/release/zxtune123"
UZX_TESTS = ROOT / "build/src/uZXTests_artefacts/Debug/uZXTests"
LINE = re.compile(r"^(\d+)-(\d+) rpb=(\d+) fpb=(\d+) bpm=([\d.]+) g=([\d.]+) sig=(\d) clear=(\d) "
                  r"first=(\d+) silent=(\d) (.+)$")


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
            results[m[11]] = m
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
        a, b, rpb, fpb = int(m[1]), int(m[2]), int(m[3]), int(m[4])
        bpms[round(float(m[5]))] += 1
        sigs[f"{m[7]}/4{'' if m[8] == '1' else '?'}"] += 1
        line = f"exp={expected:<2} got={a}-{b} rpb={rpb:<2} fpb={fpb:<3} bpm={float(m[5]):5.1f} {pt3.name[:60]}"
        if (a + b) / 2 == expected:
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


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    main(Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]) if len(sys.argv) > 3 else UZX_TESTS)
