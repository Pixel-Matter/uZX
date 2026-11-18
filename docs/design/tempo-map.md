# Tempo map for frame-based music

> **Status: research and proposal, not implemented.** For µZX Studio. Detecting sections from imported modules or dumps is out of scope. The proposal rests on assumptions listed under [Open questions](#open-questions); none is confirmed yet.

Sections of a tune with their own meter, tempo and groove: an intro in 3/4, a one-beat pickup bar, a break at half speed, an outro that speeds up. Every section runs on the same frame rate, so a beat is always a whole number of frames and the beat and groove grid changes from section to section. Code it touches: `src/models/PsgFrameRetimer.*` (`PsgTiming`), `src/models/SnapLadder.*`, `src/models/EditUtilities.*`, `src/models/PsgList.*`, `src/gui/timeline/` (`TimelineGrid`, `Ruler`), `src/gui/common/Transport.*`.

## What the code has today

| Need | Today |
| --- | --- |
| Tempo storage | Tracktion's `TempoSequence`: `TEMPO` and `TIMESIG` nodes in the edit. µZX never adds a second of either |
| Frames per beat | Derived, not stored: `PsgTiming::getFramesPerBeat` is the project fps times the beat length of the tempo at the transport position. `setFramesPerBeat` and `setBpmSnappedToFrames` write a BPM whose beat is a whole number of frames |
| Groove | `IDs::gridSubdivision` on the edit: frame counts of the groups in one beat, such as `6 7 6 7`. One pattern for the whole edit. It is a grid, not a playback effect |
| Grid and snap | `SnapLadder`: frames, groove rows, beats, bars. Rounding reads the tempo at each beat, so it already follows a tempo change; the ladder's levels and its pattern are built once, from the tempo at the start of the view (`TimelineGrid::makeSnapLadder`) |
| PSG frames | Beat-locked: frame `i` sits at `i / framesPerBeat` beats in its clip, one `framesPerBeat` per clip ([psg-timing.md](psg-timing.md)) |
| Tempo edits | `PsgTiming` assumes a single tempo: preserve-timing and `setTimeSig` scale `framesPerBeat` of every PSG clip by one ratio |
| Transport boxes | BPM, Groove and meter act on the tempo and time signature at the transport position |
| Global tracks | The tempo track is hidden, as the marker and arranger tracks are ([timeline-markers.md](timeline-markers.md)) |

## What Tracktion gives

Read from `tracktion_core/utilities/tracktion_Tempo.h` and `tracktion_engine/model/edit/tracktion_TempoSequence.*`; not run.

- **Two independent lists.** Tempo changes (`startBeat`, `bpm`, `curve`) and time signature changes (`startBeat`, numerator, denominator, triplets). The engine merges them into flat sections of constant tempo and meter.
- **Stepped tempo is native.** A curve of `1` or `-1` holds the BPM until the next point; `insertTempo(TimePosition)` uses `1` by default. Any other curve is a ramp, cut into four pieces per beat, each with its own BPM, so beats stop being whole frames.
- **A beat is the denominator unit.** `TempoSequence` builds its sequence with `LengthOfOneBeat::dependsOnTimeSignature`: a beat lasts `240 / (bpm * denominator)` seconds, so a 6/8 beat is half a 6/4 beat at the same BPM.
- **Limits.** `TempoSetting` clamps BPM to 20–300. At 50 Hz that is 10 to 150 frames per beat in x/4, 5 to 75 in x/8, 3 to 37 in x/16.
- **Positions.** A tempo point may sit on any beat fraction. A time signature point is rounded to a whole beat and kept a beat away from its neighbours. A signature placed inside a bar changes the beat length at once and the bar length from the next bar line of the old signature.
- **What moves.** `EditTimecodeRemapperSnapshot` keeps clip starts, ends and offsets, automation points and the loop range on their beats across a tempo change. Clips with `Clip::syncAbsolute` stay in time. Notes in a MIDI clip are stored in beats and follow.
- **Editing helpers.** `insertSpaceIntoSequence` and `deleteRegion` shift the later tempo and signature points; moving clips is left to the application.
- **Groove is a playback effect.** `GrooveTemplate` is a list of lateness values per subdivision, picked by name on a `MidiClip` from an engine-wide list kept in the user's settings, and applied to MIDI notes when the clip's sequence is built. It draws no grid, does nothing for PSG clips and is not stored per section.
- **The tempo track has no UI.** `TempoTrack` wraps the two lists as track items. The lane, its curve editor and its menus are Waveform's own code.
- **Cost.** `toTime` and `toBeats` scan the flat sections linearly from the end. A few hundred sections are fine; a tempo point per groove row is not a design to build on.

Two risks found in the source:

- **Stepped BPM passes through a `float`.** With a later tempo point present, the section's BPM comes from `calcCurveBpm`, and `getBezierEnds` casts the held value to `float`. A BPM such as 3000/13 loses about 3 parts in 10⁸, which is about 0.001 frame after ten minutes at 50 Hz and more at 200 Hz. `SnapLadder` treats 0.001 frame as its tolerance. A one-line patch in the µZX fork of the engine removes the cast; a test with many sections should decide whether it is needed.
- **Time signature points are at least a beat apart.** A 1/4 bar followed by another signature is the shortest section the engine allows, which is enough for the goal.

## Other software

Waveform, OpenMPT and Furnace were checked against their forum and manuals for this note; the rest is from general knowledge.

| Application | Tempo and meter | What a change moves | Groove |
| --- | --- | --- | --- |
| Waveform | A tempo track among the global tracks: a tempo curve with points, and time signature items. A signature change is inserted from the right-click menu of the bar ruler and edited in the properties panel | Clips follow beats or stay in time, per clip | Groove templates on MIDI clips |
| Reaper | One marker type carries tempo and, optionally, a time signature, with "gradual transition" and "allow partial measure" options | Timebase per item and track: time, beats (position only), or beats (position, length, rate) | Groove quantise as an edit action |
| Ableton Live | Tempo is automation on the main track. Time signature markers go on bar lines; a marker placed elsewhere leaves a fragmentary bar to complete or delete | Warped clips follow, unwarped stay | Groove pool, per clip |
| Cubase, Logic | A tempo track and a signature track, both global. Signatures on bar lines | Musical or linear time base per track | Quantise presets |
| OpenMPT | Rows per beat and rows per measure for the song, and per pattern as an override | Rows are the positions, so nothing moves | Tempo swing: a length for each row of a beat, for the song or per pattern; the rows of a beat always add up to the beat |
| Furnace | Speed in ticks per row; tick rate in Hz; highlight values draw beats and bars | Rows are the positions | A groove is a cycle of up to 16 speeds, one per row. The song keeps a list of grooves, and effect `09xx` switches to one |
| Renoise | BPM, lines per beat and ticks per line, each changeable by a pattern command; a length per pattern | Lines are the positions | Global swing amounts |
| LSDj, M8 | Groove tables of ticks per row, picked per phrase by a command | Rows are the positions | The groove table is the tempo |
| Vortex Tracker, Pro Tracker 3 | Speed in frames per row, changed by a command on any row; a length per pattern | Rows are the positions | Alternating speeds by commands |

What they show:

- **Trackers have no tempo map.** Tempo is the row length in ticks, a groove is a cycle of row lengths, and a section is a pattern with its own length and speed. Meter is only how rows are highlighted. Nothing ever lands between frames.
- **DAWs keep tempo and meter apart**, as Tracktion does, and put meter changes on bar lines. Reaper's single marker is the closest to "a section starts here with this tempo and this meter".
- **Whether content follows beats or time is a per-item choice** in every DAW. PSG data and notes need different answers here too.
- **Waveform's flow for a short odd bar is long**: insert space in a marked region on the selected tracks, then add a signature change at the bar and another after it. Users on the KVR forum call it complicated. A section as one object avoids this.
- **OpenMPT's per-pattern swing and Furnace's groove list** are the nearest models for a groove that changes per section.

## What follows for µZX

### 1. A frame-exact tempo map

Tracktion's sequence can hold frame-based sections as it is, under three rules that µZX has to enforce on every write:

- Every tempo point has curve `1`: steps only.
- Every BPM gives a whole number of frames per beat: `bpm = 240 * fps / (framesPerBeat * denominator)`.
- Every section starts on a bar line of the section before it.

Then each section is a whole number of frames long and each bar, beat and groove row starts on a frame. The BPM stays the only stored tempo; frames per beat stays derived, as today.

"Speeding up" becomes a run of steps: sections of one bar or one beat whose frames per beat fall, such as 8, 7, 7, 6. That is what a tracker does with speed commands. A smooth Tracktion ramp cannot be used, because its beats fall between frames.

Tempos between two whole frame counts (130 BPM at 50 Hz is 23.08 frames) need beats of unequal length, 23 23 23 24. In Tracktion that is a tempo point per beat. It works for a short passage; a whole tune of it means hundreds of points. Left out until it is asked for.

### 2. Groove per section

`gridSubdivision` moves from the edit to the section: an attribute on the section's `TEMPO` node. Tracktion keeps attributes it does not know. A section without one gets the default split of its frames per beat, as the edit does today.

`SnapLadder` is built for one frames-per-beat value and one pattern. The grid, snapping and the ruler need the ladder of the section a time falls in, and ticks drawn across a boundary need two ladders. `TimelineGrid::makeTicks`, the zoom levels and video export read "the tempo at the start of the view" or "at the start of the tune" today; each becomes "of the section at that time".

Tracktion's `GrooveTemplate` is not used: it shifts notes at playback by fractions that do not land on frames, lives outside the edit, and draws nothing.

### 3. What a section change moves

Three kinds of content, three rules:

- **Section starts, clip starts, markers on beats, the loop range**: stay on their bars and beats. Tracktion's remap does this. Everything after a changed section moves earlier or later by whole frames.
- **Notes**: stay on their rows. A note is on bar, beat, groove row and frame within the row. Tracktion stores it as a beat fraction, which keeps it in place only while the frames per beat and the groove stay the same. Going from 24 frames (6 6 6 6) to 26 (6 7 6 7), a note on the second row must stay on frame 6, and its beat fraction of 0.25 puts it on frame 6.5. So a change of a section's frames per beat or groove rewrites the notes under it, row boundary to row boundary. This is one function over the section's clips, and it needs the note editor of [midi-editor.md](midi-editor.md) to exist first.
- **PSG frames**: stay on the machine's clock. One `framesPerBeat` per clip cannot describe a clip that crosses a section boundary: its frames would play faster in a faster section. The clip start stays on its beat, as other clips do, and frame `i` plays `i / frameRate` seconds after it. [psg-timing.md](psg-timing.md) names this as true time-locking and does not plan it; a tempo map makes it necessary. It also removes the preserve-timing option and the single-ratio scaling in `PsgTiming`: PSG timing is always preserved, and playing a dump faster is the clip's frame rate.

### 4. Where it is edited

A section is one object to the user: a start on a bar line, a meter, frames per beat (shown with its BPM) and a groove. In the edit it is a `TEMPO` node, plus a `TIMESIG` node at the same beat when the meter differs from the section before.

- **A tempo lane** above the ruler, next to the marker lane of [timeline-markers.md](timeline-markers.md). Each section is a block from its start to the next, labelled `3/4 · 125 · 6 6 6 6`. Clicking a block selects the section and puts the playhead in it.
- **The transport bar's BPM, Groove and meter boxes** edit the section at the playhead. They already read the tempo at the transport position, so this is mostly a matter of the writes going to the right node.
- **Commands**: start a section at the playhead (it snaps to the nearest bar line and copies the section it splits), delete a section start, drag a start by bars, insert a bar with its own meter in one step.
- **Speed up or slow down**: a dialog on a range gives the first and last frames per beat and the step length (beat or bar), and writes the run of sections.

## Proposal in short

Keep Tracktion's `TempoSequence` as the one tempo map. Put every write behind one µZX module that keeps the three rules of a frame-exact map. Store the groove on the section. Time-lock PSG frames. Show sections as blocks in a lane and edit them through the transport boxes.

## Order of work

Each step stands on its own.

1. **One writer for the tempo map.** Extend `PsgTiming` into the module every tempo and meter write goes through, addressed by section, with the three rules checked. Tests: sections of different meters and frames per beat keep every bar and beat on a frame over a long tune, at 50 and 200 Hz. The test decides the `float` patch in the engine fork. This also closes the TODO entry on tempo changes that bypass `setTempoBpmRetimingFrames`.
2. **Time-locked PSG frames.** `PsgList` positions frames by frame rate from the clip start; `framesPerBeat` and preserve-timing go away. Old edits convert in `BaseController::setEdit`. An older app would misplace the frames of an edit with several tempos, so the `.uzx` format version goes up.
3. **Groove per section.** The attribute on the `TEMPO` node, the edit-wide one converted on load; a ladder per section in `TimelineGrid`, the ruler, zoom levels and video export.
4. **The lane and the commands.** Blocks, selection, start, delete and drag; the transport boxes on the section at the playhead; user guide and CHANGELOG.
5. **Speed-up dialog.**
6. **Notes stay on rows.** With the note editor.

## Open questions

Each has the assumption this note uses.

1. **Is a section one object** (start, meter, tempo, groove) or separate tempo and signature points as in Waveform? Assumed: one object.
2. **Speeding up**: stepped whole-frame beats, or smooth curves? Assumed: stepped.
3. **Beats of unequal length** for tempos between whole frame counts, and grooves longer than a beat: needed? Assumed: no.
4. **Later sections** when one changes: move with the bars, or keep their time? Assumed: move.
5. **PSG clips time-locked**, with preserve-timing removed? Assumed: yes.
6. **Section starts only on bar lines?** Assumed: yes.
7. **Denominators** other than 4, with frames per beat counted per denominator unit? Assumed: yes, as the transport box does now.
8. **The lane**: its own lane, the ruler, or a property of markers? Assumed: its own lane.
9. **Beats shorter than 10 frames in x/4** (above 300 BPM at 50 Hz)? Assumed: use x/8 instead of raising the engine's limit.
10. **Notes stay on rows** when a section's frames per beat or groove changes? Assumed: yes.
