# instrdump: the instruments of tracker modules

A development tool for studying the samples and ornaments of ZX Spectrum AY music: it reads a
tracker module through zxtune's parsers and prints its instruments, and how its patterns use
them, as JSON. `scripts/zxart_instruments_db.py` runs it over a corpus downloaded from zxart.ee
and builds an SQLite database.

## Build a corpus database

```sh
git submodule update --init --depth=1 third_party/zxtune
cmake -S tools/instrdump -B build/instrdump -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/instrdump --parallel

python3 -I scripts/zxart_fetch_pt3.py 1000 .cache/zxart/top all   # the top-rated tunes of every format
python3 -I scripts/zxart_instruments_db.py                        # writes .cache/zxart/top/instruments.sqlite
```

[corpus/zxart-top.sqlite](corpus/zxart-top.sqlite) is the database of the 997 top-rated zxart tunes as
of November 2026, 811 of them tracker modules.

`build/instrdump/instrdump MODULE` prints one module; it exits with 1 for a file that is not a
tracker module zxtune's AY parsers read (an AY or PSG dump, an MP3, another chip's module).

## The model

Every tracker's sample is written as a step per frame, in the same terms: level and its slide,
tone, noise and envelope switches, and the offsets to the tone, noise and envelope periods, each
with a flag that says whether it accumulates. An ornament is a step per frame of note and noise
offsets. After its last step a sequence goes on from its loop step or stops the sound.
[Model.h](Model.h) states what each field means; the adapters in `formats/` derive them from what
zxtune's player of the format does each frame, and say where a format does not fit.

The usage rows count what the patterns sound, played in the order of the positions: per sample,
ornament, envelope shape and note, the times the note was struck, and the rows and frames it
sounded for. A usage row whose ornament slot has no definition plays no ornament: SQ-Tracker's
slot 0, Pro Sound Maker's 32.

## The database

| Table | Holds |
| --- | --- |
| `module` | a zxart tune: title, year, rating, zxart format, file, and `status` (`ok`, `unsupported`, `nofile`, `error`) |
| `author`, `module_author` | the tune's authors by zxart id |
| `chip` | a module of the file (a Turbo Sound file has two): zxtune's format name, the texts in the module, its note table |
| `instrument` | a distinct sample or ornament: `id` is a hash of its content, `lanes` a JSON object of its fields over the steps, all-zero lanes left out |
| `slot` | where an instrument occurs: module, chip, kind, slot number |
| `usage` | the usage rows, by slot numbers |
| `instrument_usage` | view: `usage` with the instrument ids of its sample and ornament |

An instrument that several modules share is one `instrument` row with a `slot` row per occurrence.
Tone offsets are stored modulo 4096, the size of the chip's tone period.

## What the dump leaves out

- What a pattern does to a note besides choosing its sample, ornament and envelope shape: the
  channel volume, the noise base, slides and vibrato, the envelope period, sample and ornament
  offsets.
- ProTracker 1 and Sound Tracker read the ornament at the sample's position, so the two differ
  from the dump once a sample has looped.
- SQ-Tracker loops an ornament that has no loop over the range of the sample it plays with; an
  ornament used with samples of different loops holds its last step instead.
- ASC Sound Master plays the lines past a sample's loop only after a pattern's "break sample",
  and they are left out.
- Pro Sound Creator's volume slide moves the channel volume, which scales the level; the dump
  adds it to the level. Whether a line's offset goes to the envelope or the noise depends there
  on the channel's envelope being on.
- Pro Sound Maker sets the envelope bit only while the level and the channel volume reach 15.
- Global Tracker ORs the noise periods of the three channels together.
- ProTracker 1, Global Tracker, uncompiled Sound Tracker and ASC Sound Master 0.x are written
  from zxtune's sources alone: the corpus has no file of them.
