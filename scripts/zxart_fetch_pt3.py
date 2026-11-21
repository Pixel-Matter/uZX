#!/usr/bin/env python3
"""Download the top-N rated tunes from zxart.ee into a folder.

    python3 -I scripts/zxart_fetch_pt3.py 100 .cache/zxart/pt3
    python3 -I scripts/zxart_fetch_pt3.py 200 .cache/zxart/stc stc    # another zxart format: pt2, stc, asc, stp, sqt...
    python3 -I scripts/zxart_fetch_pt3.py 1000 .cache/zxart/top all   # every format, no format filter

Writes one module per tune plus index.jsonl with id, title, type (format), authorIds, authors ([{id, name}], resolved ids only),
year, rating, plays, time and file (null when the tune has no original file).
Already-listed tunes are skipped, so re-running with a larger N only fetches the tail. API calls and
downloads are retried with backoff; a page that keeps failing stops the run (re-run to resume).
"""
import json
import shutil
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

API = "https://zxart.ee/api/types:zxMusic/export:zxMusic/language:eng/start:{start}/limit:{limit}/order:votes,desc/filter:{fmt}zxMusicMinRating=4"
AUTHORS = "https://zxart.ee/api/types:author/export:author/language:eng/filter:authorId={ids}"
PAGE = 100
TRIES = 5


def retry(fn, what):
    for i in range(TRIES):
        try:
            return fn()
        except Exception as e:
            if i == TRIES - 1:
                sys.exit(f"error: {what} failed after {TRIES} tries: {e}; re-run to resume")
            print(f"  retry {what} in {2 << i}s: {e}")
            time.sleep(2 << i)


def fetch_json(url):
    def get():
        with urllib.request.urlopen(url, timeout=60) as r:
            return json.load(r)
    return retry(get, url)


def download(url, dest):
    def get():
        with urllib.request.urlopen(url, timeout=60) as r, dest.open("wb") as f:
            shutil.copyfileobj(r, f)
    retry(get, url)


def resolve_authors(ids, names):
    """Fill `names` (id -> author name) for ids not seen yet; ids the API can't resolve stay absent."""
    todo = sorted(set(ids) - names.keys() - UNKNOWN)
    for i in range(0, len(todo), 50):
        chunk = todo[i:i + 50]
        try:
            data = fetch_json(AUTHORS.format(ids=",".join(map(str, chunk))))["responseData"]["author"]
        except (SystemExit, KeyError, TypeError):
            data = []  # an author lookup must not fail the run
        names.update({a["id"]: a["title"] for a in data if a.get("title")})
        UNKNOWN.update(set(chunk) - names.keys())


UNKNOWN = set()


def main(n, out, fmt):
    out.mkdir(parents=True, exist_ok=True)
    index = out / "index.jsonl"
    seen = {json.loads(l)["id"] for l in index.read_text().splitlines()} if index.exists() else set()
    names = {}
    got = 0
    with index.open("a") as idx:
        for start in range(0, n, PAGE):
            fmt_filter = "" if fmt == "all" else f"zxMusicFormat={fmt};"
            page = fetch_json(API.format(start=start, limit=min(PAGE, n - start), fmt=fmt_filter))
            tunes = page["responseData"]["zxMusic"]
            if not tunes:
                break
            resolve_authors([a for t in tunes if t["id"] not in seen for a in t.get("authorIds") or []], names)
            for t in tunes:
                got += 1
                if t["id"] in seen:
                    continue
                file = None
                if t.get("originalUrl"):
                    # the name comes from the server: keep only its last component so it can't leave `out`
                    name = Path(urllib.parse.unquote(t.get("originalFileName") or t["originalUrl"].rsplit("filename:", 1)[-1])).name
                    dest = out / f'{t["id"]}_{name}'
                    download(t["originalUrl"], dest)
                    file = dest.name
                    time.sleep(0.2)  # be polite to zxart
                idx.write(json.dumps({k: t.get(k) for k in ("id", "title", "type", "authorIds")}
                                     | {"authors": [{"id": a, "name": names[a]} for a in t.get("authorIds") or [] if a in names]}
                                     | {k: t.get(k) for k in ("year", "rating", "plays", "time")}
                                     | {"file": file}, ensure_ascii=False) + "\n")
                idx.flush()
                print(f"{got:4d} {file}")
    print(f"done: {got} tunes listed, files in {out}")


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 100,
         Path(sys.argv[2]) if len(sys.argv) > 2 else Path(".cache/zxart/pt3"),
         sys.argv[3] if len(sys.argv) > 3 else "pt3")
