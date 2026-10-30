#!/usr/bin/env python3
"""Convert PT3 modules to PSG register dumps with zxtune123, to build test corpora.

    python3 -I scripts/pt3_to_psg.py SRC_DIR DST_DIR [ZXTUNE123]

Writes DST_DIR/<stem>.psg for every SRC_DIR/<stem>.pt3, skipping existing ones.
ZXTUNE123 defaults to the binary built by scripts/build_zxtune123.sh. Together
with scripts/zxart_fetch_pt3.py this gives PT3-PSG pairs: the module keeps the
ground truth (speed, pattern lengths, see scripts/pt3_info.py) that the dump loses.
"""
import subprocess
import sys
from pathlib import Path

DEFAULT_ZXTUNE = Path(__file__).resolve().parent.parent / ".cache/tools/zxtune/bin/darwin/release/zxtune123"


def convert(zxtune, src, dst):
    """zxtune's option map is comma-separated and file names often hold commas,
    so convert to a comma-free temporary name and rename."""
    if dst.exists():
        return True
    tmp = dst.with_name("convert_tmp.psg")
    tmp.unlink(missing_ok=True)
    subprocess.run([str(zxtune), "--convert", f"mode=psg,filename={tmp}", "--", str(src)],
                   capture_output=True, text=True, timeout=120)
    if tmp.exists():
        tmp.rename(dst)
    return dst.exists()


def main(src, dst, zxtune):
    if not zxtune.exists():
        sys.exit(f"{zxtune} not found: run scripts/build_zxtune123.sh")
    dst.mkdir(parents=True, exist_ok=True)
    ok = failed = 0
    for pt3 in sorted(src.glob("*.pt3")):
        if convert(zxtune, pt3, dst / (pt3.stem + ".psg")):
            ok += 1
        else:
            failed += 1
            print(f"failed: {pt3.name}")
    print(f"converted {ok}, failed {failed}, into {dst}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    main(Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_ZXTUNE)
