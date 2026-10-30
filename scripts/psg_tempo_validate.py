#!/usr/bin/env python3
"""Validate psg_tempo.py against PT3 header speeds.

    python3 -I scripts/psg_tempo_validate.py .cache/zxart/pt3 .cache/zxart/psg

Converts each .pt3 to .psg with zxtune123 (skipped when the .psg exists), reads the
PT3 "delay" byte as the expected speed, runs the detector, and prints mismatches plus
a summary. A detected 2-value swing counts as a match when its mean equals the delay.
"""
import subprocess
from collections import Counter
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from psg_tempo import analyse  # noqa: E402

ZXTUNE = next(Path(".cache/tools/zxtune/bin").rglob("zxtune123"), None)


def pt3_delay(path):
    return Path(path).read_bytes()[100]


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


def main(src, dst):
    dst.mkdir(parents=True, exist_ok=True)
    ok = harmonic = miss = unknown = failed = 0
    bpms = []
    for pt3 in sorted(src.glob("*.pt3")):
        psg = dst / (pt3.stem + ".psg")
        if not convert(pt3, psg):
            failed += 1
            print(f"CONVERT-FAIL {pt3.name}")
            continue
        expected = pt3_delay(pt3)
        r = analyse(psg)
        if r is None:
            unknown += 1
            print(f"UNKNOWN  exp={expected:<3} {pt3.name}")
            continue
        a, b = r["speed"]
        got = (a + b) / 2
        line = f"exp={expected:<2} got={a}-{b} rpb={r['rpb']:<2} fpb={r['fpb']:<5.1f} bpm={r['bpm']:5.1f} g={r['conc']:.2f} {pt3.name[:60]}"
        bpms.append(round(r["bpm"]))
        if got == expected:
            ok += 1
        elif r["fpb"] % expected == 0:
            harmonic += 1       # frames per beat is still a whole number of rows
            print("HARMONIC " + line)
        else:
            miss += 1
            print("MISMATCH " + line)
    total = ok + harmonic + miss + unknown
    print(f"\nspeed ok {ok}/{total}  harmonic (fpb = whole rows) {harmonic}  mismatch {miss}  unknown {unknown}  convert failed {failed}")
    print("bpm histogram:", sorted(Counter(bpms).items()))


if __name__ == "__main__":
    main(Path(sys.argv[1]), Path(sys.argv[2]))
