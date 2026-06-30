# Sketch: integer-frame PSG timestamps

> Design note on branch `integer-psg-sketch`.
>
> **Status: Option A implemented.** Frames now carry a machine-frame index
> (`IDs::fi`) and the PSG list stores its frame rate (`IDs::frameRate`). Tempo
> changes call `PsgTiming::setTempoBpmRetimingFrames`, which re-derives each
> frame's beat from `frameIndex / frameRate` — keeping PSG timing fixed in time by
> construction. The old opt-in preserve flag, the snapshot/restore machinery, and
> the `transportPreservePsgTiming` command have been removed. Manually placed
> frames use index `-1` and stay beat-anchored (legacy behaviour).

## Problem this addresses

PSG/AY register dumps are natively a sequence of frames at a fixed machine rate
(50 Hz on ZX Spectrum/PAL, etc.). Today each `PsgParamFrame` stores its position
as a **beat** (`te::IDs::b`, a `te::BeatPosition`). Because beats are tempo-relative,
a BPM/frames-per-beat change moves every frame in wall-clock time — which is wrong
for imported PSG data, where frame N must always play at `N / frameRate` seconds.

The `timecode` branch works around this with an opt-in snapshot/remap
(`PsgTimingPreserver`) that pins frames to their absolute time across a tempo
change. That is correct but it is a *workaround*: it reconstructs, on every tempo
edit, a timestamp the data already has natively.

**Idea:** make the integer frame index (plus the clip's frame rate) the stored
source of truth, and derive beats/seconds on demand. Then tempo changes never
move PSG frames, and the preserve flag + remap machinery can be retired.

## Current model (where time lives)

- `PsgData::frameNumToSeconds(i) = i / frameRate` — the canonical mapping already
  exists. `PsgData::Options::frameRate` defaults to 50; `playRate` is a machine-frame
  multiplier. (`src/formats/psg/PsgData.h:245`)
- On load, `PsgList::loadFrom` drops empty frames and converts each surviving
  frame's time to **beats** via `tempoSequence.toBeats(seconds)`, storing `b`.
  (`src/models/PsgList.cpp:341`)
- `PsgParamFrame` keeps `te::BeatPosition beatNumber` mirrored from `IDs::b`.
- Read paths go through three accessors:
  - `getRawEditBeats` / `getRawEditTime` — clip-offset applied, no quantise.
  - `getEditBeats` / `getEditTime` — adds `getQuantisation().roundBeatToNearest`.

## Consumers that read frame timing (the blast radius)

Everything that talks to the engine is **beat- or seconds-based**, so a frame-index
model needs a translation layer at each of these boundaries:

| Consumer | File | Reads |
|----------|------|-------|
| MIDI render (playback/export) | `models/PsgMidi.cpp:73` `getTimeInBase` | `getBeatPosition` / `getEditBeats` / `getEditTime` per `TimeBase` |
| Clip waveform / param drawing | `gui/timeline/PsgClipComponent.cpp:228,328` | `getEditTime` |
| Param editor hit-testing | `gui/timeline/PsgParamEditorComponent.cpp:162,190` | `getEditTime` |
| Tempo-change preserve | `models/PsgTimingPreserver.cpp` | raw edit time (would be **deleted**) |
| Edit ops | `PsgList::moveAllBeatPositions / rescale / trimOutside` | beat math |

## Two possible representations

### Option A — frame index is the stored truth; beats derived

`PsgParamFrame` stores `int frameIndex` + the owning clip exposes `frameRate`.
`b` (beat) becomes a *cache*, recomputed from `frameIndex / frameRate → toBeats`
whenever the tempo map changes.

- **Pro:** tempo changes are free — `frameIndex` doesn't move, so frames are
  rock-stable in time by construction. `PsgTimingPreserver` and the preserve
  toggle disappear entirely. Off-grid/quantise round-trip issue is moot.
- **Pro:** matches import/export/chip semantics 1:1.
- **Con:** beats are now derived, so any beat-based edit (drag-snap to musical
  grid, `moveAllBeatPositions`, groove) must round-trip through frames, or accept
  that "snap to beat" means "snap to nearest frame near that beat."
- **Con:** larger change — `b` flips from authoritative to cache; the engine still
  sequences in beats so the cache must stay coherent (re-derive on tempo/rate edit).

### Option B — keep `b` authoritative, add `frameIndex` as a parallel anchor

Store both; treat `frameIndex` as the anchor only while "lock to frames" is on,
recomputing `b` from it on tempo change (essentially what the preserve flag does,
but persisted per-frame instead of reconstructed).

- **Pro:** much smaller delta; consumers keep reading beats unchanged.
- **Con:** two timestamps that can disagree → exactly the coherence hazard we're
  trying to remove. Really just persisting the workaround.

**Recommendation: Option A** is the structurally correct one. Option B is a half-step
that keeps the dual-representation smell.

## Migration outline for Option A

1. **Storage:** add `te::IDs` frame index property (e.g. `fi`) to the frame value
   tree; keep `b` written as a derived value for engine compatibility. `PsgList`
   owns the clip frame rate (already on `PsgData::Options::frameRate` at load).
2. **Derivation helper:** `frameIndexToBeats(idx) = tempoSequence.toBeats(idx / fps)`,
   centralised so every read path uses it.
3. **Recompute hook:** on tempo or frame-rate change, walk frames and rewrite each
   `b` from `frameIndex` (this *replaces* `PsgTimingPreserver` — and is simpler,
   since there is nothing to snapshot; the index is already the anchor).
4. **Consumers:** `getEditBeats/getEditTime` derive from `frameIndex`; quantise
   becomes optional snapping *for editing*, never for stored position.
5. **Edits:** `moveAllBeatPositions`/`rescale` operate on `frameIndex`
   (move/scale integer indices); `trimOutside` filters by index range.
6. **Retire:** delete `PsgTimingPreserver.{h,cpp}`, the `preservePsgTimingOnTempoChange`
   cached value, the `transportPreservePsgTiming` command, and the Ruler/Transport
   menu entries. Update `PsgListTempoTiming` tests to assert frames never move.

## Open questions

- Variable tempo within a clip: with frame-index truth, a frame's *beat* still moves
  under a mid-clip tempo change, but its *time* stays fixed — which is the desired
  behaviour. Confirm the drawing/snap code is happy deriving beats live.
- `playRate` (machine-frame multiplier) interaction with `frameIndex`: decide whether
  the stored index is in play-frames or machine-frames.
- Snap-to-beat editing UX once position is frame-quantised: "nearest frame to beat"
  vs "nearest beat" — pick one and make it explicit.
