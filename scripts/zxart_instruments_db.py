#!/usr/bin/env python3
"""Build an SQLite database of the instruments (samples, ornaments) of a downloaded zxart.ee corpus.

    python3 -I scripts/zxart_instruments_db.py
    python3 -I scripts/zxart_instruments_db.py .cache/zxart/top .cache/zxart/top/instruments.sqlite build/instrdump/instrdump

Reads index.jsonl (see zxart_fetch_pt3.py) and runs tools/instrdump over every module file. The database is
rebuilt from scratch on every run. Identical instruments from different modules share one `instrument` row
(id = content hash, see canonical()); `slot` maps each module's slot numbers to them.
"""
import hashlib
import html
import json
import os
import sqlite3
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

TIMEOUT = 60
SCHEMA = """
CREATE TABLE module(id INTEGER PRIMARY KEY, title TEXT, year INTEGER, rating REAL, plays INTEGER, time TEXT,
                    type TEXT, file TEXT, status TEXT NOT NULL);
CREATE TABLE author(id INTEGER PRIMARY KEY, name TEXT NOT NULL);
CREATE TABLE module_author(module_id INTEGER, author_id INTEGER, PRIMARY KEY(module_id, author_id));
CREATE TABLE chip(module_id INTEGER, chip INTEGER, format TEXT, title TEXT, author TEXT, program TEXT,
                  note_table TEXT, PRIMARY KEY(module_id, chip));
CREATE TABLE instrument(id TEXT PRIMARY KEY, kind TEXT NOT NULL, len INTEGER NOT NULL, loop INTEGER NOT NULL,
                        stops INTEGER NOT NULL, lanes TEXT NOT NULL);
CREATE TABLE slot(module_id INTEGER, chip INTEGER, kind TEXT, slot INTEGER, instrument_id TEXT NOT NULL,
                  PRIMARY KEY(module_id, chip, kind, slot));
CREATE TABLE note_table(name TEXT PRIMARY KEY, periods TEXT NOT NULL);
CREATE TABLE usage(module_id INTEGER, chip INTEGER, sample_slot INTEGER, ornament_slot INTEGER, envelope INTEGER,
                   envelope_period INTEGER, noise INTEGER, note INTEGER, notes INTEGER, rows INTEGER, frames INTEGER,
                   PRIMARY KEY(module_id, chip, sample_slot, ornament_slot, envelope, envelope_period, noise, note));
CREATE INDEX slot_instrument ON slot(instrument_id);
CREATE INDEX slot_module ON slot(module_id, chip);
CREATE INDEX module_author_author ON module_author(author_id);
CREATE INDEX usage_module ON usage(module_id, chip);
CREATE VIEW instrument_usage AS
SELECT u.module_id, u.chip, s.instrument_id AS sample_id, o.instrument_id AS ornament_id,
       u.envelope, u.envelope_period, u.noise, u.note, u.notes, u.rows, u.frames, c.note_table
FROM usage u
JOIN chip c ON c.module_id = u.module_id AND c.chip = u.chip
LEFT JOIN slot s ON s.module_id = u.module_id AND s.chip = u.chip AND s.kind = 'sample' AND s.slot = u.sample_slot
LEFT JOIN slot o ON o.module_id = u.module_id AND o.chip = u.chip AND o.kind = 'ornament' AND o.slot = u.ornament_slot;
"""


def num(value, cast):
    try:
        return cast(value)
    except (TypeError, ValueError):
        return None


def canonical(kind, inst):
    """Return (id, len, loop, stops, lanes json) of an instrument; the single definition of its identity."""
    lanes = {}
    for name, values in inst["lanes"].items():
        if name == "toneOfs":
            values = [((v + 2048) % 4096) - 2048 for v in values]
        if any(values):
            lanes[name] = values
    stops = 1 if inst["end"] == "stop" else 0
    loop = 0 if stops else inst["loop"]
    lanes_json = json.dumps(lanes, sort_keys=True, separators=(",", ":"))
    key = json.dumps({"kind": kind, "len": inst["len"], "loop": loop, "stops": stops, "lanes": lanes},
                     sort_keys=True, separators=(",", ":"))
    prefix = "smp:" if kind == "sample" else "orn:"
    return prefix + hashlib.sha1(key.encode()).hexdigest()[:16], inst["len"], loop, stops, lanes_json


def extract(tool, path):
    """Return (status, parsed JSON or None)."""
    try:
        r = subprocess.run([tool, str(path)], capture_output=True, timeout=TIMEOUT, text=True,
                           encoding="utf-8", errors="replace")
    except (subprocess.TimeoutExpired, OSError):
        return "error", None
    if r.returncode == 1:
        return "unsupported", None
    if r.returncode != 0:
        return "error", None
    try:
        return "ok", json.loads(r.stdout)["modules"]
    except (ValueError, KeyError, TypeError):
        return "error", None


def add_module(db, tune, status, chips):
    mid = tune["id"]
    db.execute("INSERT INTO module VALUES(?,?,?,?,?,?,?,?,?)",
               (mid, html.unescape(tune.get("title") or ""), num(tune.get("year"), int), num(tune.get("rating"), float),
                num(tune.get("plays"), int), tune.get("time"), tune.get("type"), tune.get("file"), status))
    for a in tune.get("authors") or []:
        if isinstance(a, dict) and isinstance(a.get("id"), int):
            db.execute("INSERT OR IGNORE INTO author VALUES(?,?)", (a["id"], a.get("name") or ""))
            db.execute("INSERT OR IGNORE INTO module_author VALUES(?,?)", (mid, a["id"]))
    for ci, c in enumerate(chips or []):
        db.execute("INSERT INTO chip VALUES(?,?,?,?,?,?,?)",
                   (mid, ci, c.get("format"), c.get("title"), c.get("author"), c.get("program"), c.get("noteTable")))
        db.execute("INSERT OR IGNORE INTO note_table VALUES(?,?)",
                   (c.get("noteTable"), json.dumps(c.get("notePeriods"), separators=(",", ":"))))
        for kind, key in (("sample", "samples"), ("ornament", "ornaments")):
            for inst in c.get(key, []):
                if inst["len"] == 0:
                    continue
                iid, ln, loop, stops, lanes = canonical(kind, inst)
                db.execute("INSERT OR IGNORE INTO instrument VALUES(?,?,?,?,?,?)", (iid, kind, ln, loop, stops, lanes))
                db.execute("INSERT OR REPLACE INTO slot VALUES(?,?,?,?,?)", (mid, ci, kind, inst["slot"], iid))
        for u in c.get("usage", []):
            db.execute("INSERT INTO usage VALUES(?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT DO UPDATE SET "
                       "notes = notes + excluded.notes, rows = rows + excluded.rows, frames = frames + excluded.frames",
                       (mid, ci, u["sample"], u["ornament"], u["envelope"], u["envelopePeriod"], u["noise"], u["note"],
                        u["notes"], u["rows"], u["frames"]))


def build(corpus, tmp, tool):
    tunes = [json.loads(line) for line in (corpus / "index.jsonl").read_text(encoding="utf-8").splitlines()
             if line.strip()]
    todo = [t for t in tunes if t.get("file") and (corpus / t["file"]).is_file()]
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        results = dict(zip((t["id"] for t in todo), pool.map(lambda t: extract(tool, corpus / t["file"]), todo)))
    db = sqlite3.connect(tmp)
    db.executescript(SCHEMA)
    with db:
        for t in tunes:
            status, chips = results.get(t["id"], ("nofile", None))
            add_module(db, t, status, chips)
    return db


def table(db, title, sql):
    print(f"\n{title}")
    rows = db.execute(sql).fetchall()
    widths = [max(len(str(v)) for v in col) for col in zip(*rows)] if rows else []
    for r in rows:
        print("  " + "  ".join(str(v).ljust(w) for v, w in zip(r, widths)).rstrip())
    if not rows:
        print("  (none)")


def summary(db):
    table(db, "modules by status", "SELECT status, COUNT(*) FROM module GROUP BY status ORDER BY 2 DESC")
    table(db, "modules by type and status",
          "SELECT type, status, COUNT(*) FROM module GROUP BY type, status ORDER BY type, status")
    table(db, "counts", "SELECT 'chips', COUNT(*) FROM chip UNION ALL SELECT 'slots', COUNT(*) FROM slot "
          "UNION ALL SELECT 'unique ' || kind || 's', COUNT(*) FROM instrument GROUP BY kind")
    table(db, "instruments in more than one module",
          "SELECT kind, COUNT(*) FROM instrument WHERE (SELECT COUNT(DISTINCT module_id) FROM slot "
          "WHERE instrument_id = instrument.id) > 1 GROUP BY kind")
    table(db, "most shared samples", "SELECT instrument_id, COUNT(DISTINCT module_id) AS modules FROM slot "
          "WHERE kind = 'sample' GROUP BY instrument_id ORDER BY 2 DESC, 1 LIMIT 10")
    table(db, "usage rows with undefined sample slot", "SELECT COUNT(*) FROM instrument_usage WHERE sample_id IS NULL")


def main():
    corpus = Path(sys.argv[1] if len(sys.argv) > 1 else ".cache/zxart/top")
    dest = Path(sys.argv[2] if len(sys.argv) > 2 else corpus / "instruments.sqlite")
    tool = sys.argv[3] if len(sys.argv) > 3 else "build/instrdump/instrdump"
    if not Path(tool).is_file():
        sys.exit(f"error: {tool} not found; build it: see tools/instrdump/CMakeLists.txt")
    tmp = dest.with_name(dest.name + ".tmp")
    tmp.unlink(missing_ok=True)
    db = build(corpus, tmp, tool)
    summary(db)
    db.close()
    tmp.replace(dest)
    print(f"\nwrote {dest}")


if __name__ == "__main__":
    main()
