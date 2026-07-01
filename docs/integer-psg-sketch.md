# Sketch: integer-frame PSG timestamps

> Design note on branch `integer-psg-sketch`.
>
> **Status: Option A implemented with `framesPerBeat`.** Frames carry a machine-frame
> index (`IDs::i`) and the PSG list stores source frame rate (`IDs::frameRate`) plus
> musical spacing (`IDs::framesPerBeat`). The index is the per-event source of
> truth. PSG frame events no longer store `te::IDs::b`; beat positions are derived
> as `i / framesPerBeat`. Tempo changes never rewrite indexes. The preserve option
> only scales PSG list `framesPerBeat`; unchecked mode leaves it fixed so PSG timing
> follows beat-space tempo changes.
>
> **Migration:** old files (frames with `b` but no `i`/`frameRate`) are upgraded
> on load in `PsgClip::initialise` via `PsgList::migrateToFrameIndicesIfNeeded`,
> which adopts the edit's timecode FPS and back-fills each frame's index by
> inverting its stored beat through the tempo map. The legacy `b` property is
> removed after migration. Lists missing `framesPerBeat` infer it from frame rate
> and the current beat length.

## Problem this addresses

PSG/AY register dumps are natively a sequence of frames at a fixed machine rate
(50 Hz on ZX Spectrum/PAL, etc.). Storing PSG event positions as beats is wrong
for imported PSG data, where frame N must always play at `N / frameRate` seconds:
because beats are tempo-relative, a BPM/frames-per-beat change would otherwise
move every frame in wall-clock time.

The `timecode` branch works around this with an opt-in snapshot/remap
(`PsgTimingPreserver`) that pins frames to their absolute time across a tempo
change. That is correct but it is a *workaround*: it reconstructs, on every tempo
edit, a timestamp the data already has natively.

**Idea:** make the integer frame index the stored event timestamp and keep
`framesPerBeat` as list-level musical spacing. Then tempo changes can either
preserve absolute PSG timing by scaling one list property, or preserve beat
positions by leaving that property fixed.

## Current model (where time lives)

- `PsgData::frameNumToSeconds(i) = i / frameRate` — the canonical mapping already
  exists. `PsgData::Options::frameRate` defaults to 50; `playRate` is a machine-frame
  multiplier. (`src/formats/psg/PsgData.h:245`)
- On load, `PsgList::loadFrom` drops empty frames and stores each surviving
  frame's source machine-frame index as `IDs::i`.
- `PsgList` stores `frameRate` for source/native rate and `framesPerBeat` for the
  current musical spacing. Effective FPS is `framesPerBeat / beatLengthSeconds`.
- `PsgParamFrame` does not store a beat. `getFrameBeatPosition`,
  `getRawEditBeats`, and `getEditTime` derive beat position from
  `frameIndex / framesPerBeat`.
- Read paths go through three accessors:
  - `getRawEditBeats` / `getRawEditTime` — clip-offset applied, no quantise.
  - `getEditBeats` / `getEditTime` — adds `getQuantisation().roundBeatToNearest`.

## Consumers that read frame timing (the blast radius)

Everything that talks to the engine is **beat- or seconds-based**, so a frame-index
model needs a translation layer at each of these boundaries:

| Consumer | File | Reads |
|----------|------|-------|
| MIDI render (playback/export) | `models/PsgList.cpp` `getTimeInBase` | `getFrameBeatPosition` / `getEditBeats` / `getEditTime` per `TimeBase` |
| Clip waveform / param drawing | `gui/timeline/PsgClipComponent.cpp:228,328` | `getEditTime` |
| Param editor hit-testing | `gui/timeline/PsgParamEditorComponent.cpp:162,190` | `getEditTime` |
| Tempo-change preserve | `models/PsgFrameRetimer.cpp` | scales list `framesPerBeat`; never rewrites frame indexes |
| Edit ops | `PsgList::addFrameEvent / getFrameAtIndex` | frame-index math |

## Two possible representations

### Option A — frame index is the stored truth; beats derived

`PsgParamFrame` stores `int frameIndex`; `PsgList` stores `frameRate` and
`framesPerBeat`. Beats are derived from `frameIndex / framesPerBeat` whenever a
Tracktion boundary needs beat-space timing.

- **Pro:** tempo changes never touch per-event indexes. Preserving absolute timing
  is a cheap metadata scale; disabling preserve keeps PSG in beat space.
- **Pro:** matches import/export/chip semantics 1:1.
- **Con:** beats are now derived, so any beat-based edit (drag-snap to musical
  grid, groove) must round-trip through frames, or accept that "snap to beat"
  means "snap to nearest frame near that beat."
- **Con:** larger change — every Tracktion-facing path needs a derivation layer,
  because the stored PSG event itself is no longer beat-addressed.

### Option B — rejected: keep `b` authoritative, add `frameIndex` as a parallel anchor

Store both; treat `frameIndex` as the anchor only while "lock to frames" is on,
and repair `b` on tempo change (essentially what the preserve flag did, but
persisted per-frame instead of reconstructed).

- **Pro:** much smaller delta; consumers keep reading beats unchanged.
- **Con:** two timestamps that can disagree → exactly the coherence hazard we're
  trying to remove. Really just persisting the workaround.

**Recommendation: Option A** is the structurally correct one. Option B is a half-step
that keeps the dual-representation smell.

## Migration outline for Option A

1. **Storage:** store `IDs::i` on each frame value tree and `IDs::frameRate` /
   `IDs::framesPerBeat` on `PsgList`. Do not write `te::IDs::b` for PSG frames.
2. **Derivation helper:** `frameIndexToBeats(idx) = idx / framesPerBeat`,
   centralised so every read path uses it.
3. **Tempo changes:** preserve mode scales `framesPerBeat` by
   `newBeatLength / oldBeatLength`; unchecked mode leaves it fixed.
4. **Consumers:** `getEditBeats/getEditTime` derive from `frameIndex`; quantise
   becomes optional snapping *for editing*, never for stored position.
5. **Edits:** frame insertion/lookup operates on `frameIndex`; any beat-snapped UI
   operation must quantise to the nearest PSG frame before storage.
6. **Retire:** no snapshot/remap of PSG frame events. The transport/ruler preserve
   command remains as a metadata-scaling policy toggle.

## Open questions

- Variable tempo within a clip: with frame-index truth, a frame's *beat* still moves
  under a mid-clip tempo change, but its *time* stays fixed — which is the desired
  behaviour. Confirm the drawing/snap code is happy deriving beats live.
- `playRate` (machine-frame multiplier) interaction with `frameIndex`: decide whether
  the stored index is in play-frames or machine-frames.
- Snap-to-beat editing UX once position is frame-quantised: "nearest frame to beat"
  vs "nearest beat" — pick one and make it explicit.
