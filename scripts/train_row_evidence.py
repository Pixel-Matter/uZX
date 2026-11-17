#!/usr/bin/env python3
"""Learn which register events start a tracker row, and write the table the tempo detector reads.

    python3 -I scripts/train_row_evidence.py DUMP_DIR INDEX_JSONL [--out TABLE.inc] [--test-share 0.25]

DUMP_DIR is what `validate_tempo_detection.py --rows MODULE_DIR DUMP_DIR` leaves: for every module
its row starts (<name>.rows) and the event cells of every frame (<name>.cells, four little-endian
uint16 a frame: channels A, B, C and the chip, as src/models/PsgRowEvidence.h defines them).
INDEX_JSONL is the index scripts/zxart_fetch_pt3.py writes, for the authors.

A cell's weight is the log of how much likelier it is on a row start than on any frame: the share
of its frames that start a row, drawn towards the overall share by PRIOR_FRAMES frames' worth of
doubt (a cell never seen weighs nothing), against that overall share. The cells mean nothing to
this script, so the C++ stays their only definition.

Authors stay on one side of the split: tunes that share an author (co-written ones chain them)
form a group, and whole groups go to the test side until it holds --test-share of the tunes.
The test tunes are listed in DUMP_DIR/heldout.txt, for validate_tempo_detection.py --rows to
sum up apart. The table is counted from the training side only. Needs numpy.
"""
import argparse
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
PRIOR_FRAMES = 50.0
CHANNEL_CELLS, CHIP_CELLS = 8 * 9 * 9 * 3, 4 * 3 * 2  # numChannelCells, numChipCells


def split_by_author(stems, authors, test_share):
    parent = {}

    def find(a):
        while parent.setdefault(a, a) != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for stem in stems:
        ids = sorted(authors[stem])
        for a in ids[1:]:
            parent[find(a)] = find(ids[0])
    groups = {}
    for stem in stems:
        groups.setdefault(find(min(authors[stem])), []).append(stem)
    test, target = [], test_share * len(stems)
    for group in sorted(groups, key=lambda g: (g * 2654435761) % 1000003):  # a fixed pseudo-random order
        if len(test) + len(groups[group]) <= target * 1.05:
            test += groups[group]
    held = set(test)
    return [s for s in stems if s not in held], sorted(test)


def load(dump, stem):
    """Cells (frames, 4) of every chip of the tune, each with the row-start flags of its frames."""
    rows = np.array([int(x) for x in (dump / (stem + ".rows")).read_text().split()], dtype=np.int64)
    for path in [dump / (stem + ".cells")] + sorted(dump.glob(glob_escape(stem) + ".[0-9].cells")):
        cells = np.fromfile(path, dtype="<u2").reshape(-1, 4)
        starts = np.zeros(len(cells), dtype=bool)
        starts[rows[rows < len(cells)]] = True
        yield cells, starts


def glob_escape(name):
    return "".join(f"[{c}]" if c in "*?[" else c for c in name)


def average_precision(score, truth):
    order = np.argsort(-score, kind="stable")
    s, t = score[order], truth[order]
    hits = np.cumsum(t)
    last = np.r_[s[1:] != s[:-1], True]  # equal scores count together
    precision, recall = hits[last] / (np.flatnonzero(last) + 1), hits[last] / max(1, t.sum())
    return float(np.sum(np.diff(np.r_[0.0, recall]) * precision))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dump", type=Path)
    parser.add_argument("index", type=Path)
    parser.add_argument("--out", type=Path, default=ROOT / "src/models/PsgRowEvidenceTable.inc")
    parser.add_argument("--test-share", type=float, default=0.25)
    args = parser.parse_args()

    authors = {}
    for line in args.index.read_text().splitlines():
        record = json.loads(line)
        authors[Path(record["file"]).stem] = set(record.get("authorIds") or [0])
    stems = sorted(p.stem for p in args.dump.glob("*.rows") if p.stem in authors and (args.dump / (p.stem + ".cells")).exists())
    if not stems:
        raise SystemExit(f"no <name>.rows with <name>.cells and an author in {args.dump}")
    train, test = split_by_author(stems, authors, args.test_share)
    (args.dump / "heldout.txt").write_text("\n".join(test) + "\n")
    count = lambda tunes: len({a for s in tunes for a in authors[s]})

    on = [np.zeros(CHANNEL_CELLS), np.zeros(CHIP_CELLS)]
    seen = [np.zeros(CHANNEL_CELLS), np.zeros(CHIP_CELLS)]
    rows = frames = 0
    for stem in train:
        for cells, starts in load(args.dump, stem):
            for ch in range(3):
                on[0] += np.bincount(cells[starts, ch], minlength=CHANNEL_CELLS)
                seen[0] += np.bincount(cells[:, ch], minlength=CHANNEL_CELLS)
            on[1] += np.bincount(cells[starts, 3], minlength=CHIP_CELLS)
            seen[1] += np.bincount(cells[:, 3], minlength=CHIP_CELLS)
            rows += int(starts.sum())
            frames += len(starts)
    share = rows / frames
    logit = lambda p: np.log(p / (1 - p))
    weights = [logit((n1 + PRIOR_FRAMES * share) / (n + PRIOR_FRAMES)) - logit(share) for n1, n in zip(on, seen)]
    weights = [np.clip(w, -12.0, 12.0) for w in weights]  # a cell always or never on a row start, seen often

    def values(w):
        text = [f"{v:.3f}f" for v in w]
        return "\n".join("    " + ", ".join(text[i:i + 12]) + "," for i in range(0, len(text), 12))

    args.out.write_text(
        f"// Written by scripts/train_row_evidence.py from {len(train)} tracker modules by {count(train)} authors\n"
        f"// ({frames} frames, {rows} row starts). Do not edit: train again.\n"
        f"constexpr double rowStartShare = {share:.4f};  // of all frames\n"
        f"constexpr std::array<float, numChannelCells> channelWeights{{\n{values(weights[0])}\n}};\n"
        f"constexpr std::array<float, numChipCells> chipWeights{{\n{values(weights[1])}\n}};\n")

    print(f"{len(train)} training tunes by {count(train)} authors, {len(test)} held out by {count(test)}; "
          f"row starts are {share:.3f} of the frames")
    for name, tunes in (("training", train), ("held out", test)):
        score, truth = [], []
        for stem in tunes:
            for cells, starts in load(args.dump, stem):
                score.append(weights[0][cells[:, :3]].sum(axis=1) + weights[1][cells[:, 3]])
                truth.append(starts)
        score, truth = np.concatenate(score), np.concatenate(truth)
        sure = score > 0
        print(f"  {name}: frames with evidence start a row {truth[sure].mean():.3f} of the time and hold "
              f"{truth[sure].sum() / truth.sum():.3f} of the row starts; average precision {average_precision(score, truth):.3f}")
    print(f"wrote {args.out} and {args.dump / 'heldout.txt'}")


if __name__ == "__main__":
    main()
