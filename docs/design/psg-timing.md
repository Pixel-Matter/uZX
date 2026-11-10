# PSG frame timing

How PSG frames are placed in time, and what a tempo change does to them. Code: `src/models/PsgList.*` and `src/models/PsgFrameRetimer.*` (namespace `PsgTiming`).

## Model

PSG register dumps are a sequence of frames at a fixed machine rate (50 Hz on a ZX Spectrum). Tracktion positions everything in beats, which are tempo-relative. The clip bridges the two with three stored values:

| Stored | Where | Meaning |
| --- | --- | --- |
| `IDs::i` | each frame | Machine-frame index. The only per-event timestamp; never rewritten by tempo changes |
| `IDs::frameRate` | `PsgList` | Source frame rate of the dump |
| `IDs::framesPerBeat` | `PsgList` | Musical spacing: how many frames make a beat |

Everything else is derived:

- beat position of a frame = `i / framesPerBeat`
- time of a frame = the tempo map applied to that beat
- effective FPS = `framesPerBeat / beatLengthSeconds`

PSG frames do not store a beat (`te::IDs::b`). `getFrameBeatPosition`, `getRawEditBeats` and `getEditTime` derive it; `getEditBeats` / `getEditTime` add quantisation, which is snapping for editing, never the stored position.

**Invariant:** every clip-attached `PsgList` has `framesPerBeat > 0`. `PsgClip::initialise` always runs the migration below, which back-fills it, and `PsgList::clear` keeps it. There is no time-based fallback.

## Tempo changes and "preserve timing"

Frames are beat-locked: a frame's time moves whenever the tempo map moves under it. With preserve on, changing the tempo through `PsgTiming::setTempoBpmRetimingFrames` scales `framesPerBeat` by the beat-length ratio, so frames keep their wall-clock time. With it off, `framesPerBeat` stays and the PSG follows the tempo like MIDI.

Preserve is exact for single-tempo edits only:

- a tempo change inside a clip still moves the frames after it in wall-clock time;
- in a multi-tempo edit the one ratio is applied to every PSG clip, including clips under other tempo sections;
- tempo changes that bypass `setTempoBpmRetimingFrames` are not compensated (open task in [TODO.md](../TODO.md), under **PSG Clip / PSG data**).

True time-locking would need beats derived from `i / frameRate` seconds through the tempo map instead. That is not planned.

## Migration of old edits

Edits saved before frame indexes (frames with `b` but no `i`) are upgraded on load by `PsgList::migrateToFrameIndicesIfNeeded`: it adopts the edit's timecode FPS, back-fills each frame's index by inverting its stored beat through the tempo map, and removes `b`. Lists missing `framesPerBeat` infer it from the frame rate and the current beat length.

## Why not keep beats as the stored position

Storing both a beat and a frame index per event was rejected: two timestamps can disagree. Storing only beats needed a snapshot-and-remap of every frame on each tempo edit to rebuild a timestamp the data already has. With the index stored, preserving timing is one metadata scale.

## Open questions

- `playRate` (the machine-frame multiplier in `PsgData::Options`): is the stored index in play-frames or machine-frames?
- Snap-to-beat editing once positions are frame-quantised: "nearest frame to the beat" or "nearest beat". Decide when PSG editing lands.
