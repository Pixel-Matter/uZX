#!/usr/bin/env python3
"""Download the top-N rated PT3 tunes from zxart.ee into a folder.

    python3 -I scripts/zxart_fetch_pt3.py 100 .cache/zxart/pt3

Writes one .pt3 per tune plus index.jsonl with id, title, authors, year, rating.
Already-downloaded files are skipped, so re-running with a larger N only fetches the tail.
"""
import json
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

API = "https://zxart.ee/api/types:zxMusic/export:zxMusic/language:eng/start:{start}/limit:{limit}/order:votes,desc/filter:zxMusicFormat=pt3;zxMusicMinRating=4"
PAGE = 100


def fetch_json(url):
    with urllib.request.urlopen(url, timeout=60) as r:
        return json.load(r)


def main(n, out):
    out.mkdir(parents=True, exist_ok=True)
    index = out / "index.jsonl"
    seen = {json.loads(l)["id"] for l in index.read_text().splitlines()} if index.exists() else set()
    got = 0
    with index.open("a") as idx:
        for start in range(0, n, PAGE):
            page = fetch_json(API.format(start=start, limit=min(PAGE, n - start)))
            tunes = page["responseData"]["zxMusic"]
            if not tunes:
                break
            for t in tunes:
                got += 1
                if t["id"] in seen or not t.get("originalUrl"):
                    continue
                # the name comes from the server: keep only its last component so it can't leave `out`
                name = Path(urllib.parse.unquote(t.get("originalFileName") or t["originalUrl"].rsplit("filename:", 1)[-1])).name
                dest = out / f'{t["id"]}_{name}'
                urllib.request.urlretrieve(t["originalUrl"], dest)
                idx.write(json.dumps({k: t.get(k) for k in ("id", "title", "authorIds", "year", "rating", "plays", "time")}
                                     | {"file": dest.name}, ensure_ascii=False) + "\n")
                idx.flush()
                print(f"{got:4d} {dest.name}")
                time.sleep(0.2)  # be polite to zxart
    print(f"done: {got} tunes listed, files in {out}")


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 100,
         Path(sys.argv[2]) if len(sys.argv) > 2 else Path(".cache/zxart/pt3"))
