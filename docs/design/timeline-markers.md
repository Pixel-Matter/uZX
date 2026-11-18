# Timeline markers

> **Status: proposal, not implemented.** Options for each open choice, with a recommendation. Settled so far: the Player shows markers and jumps between them but does not edit them; `M` adds a marker, `Shift+Left` and `Shift+Right` go to the previous and next one; a tracker module opened as PSG gets a marker at each order position, named by its pattern.

Named points on the timeline: the user's own, and the pattern starts of an imported tracker module. Code it touches: `src/gui/timeline/` (`Ruler`, `EditComponent`, `LoopLinesComponent`, `TimelineGrid`), `src/controllers/MainCommands.h`, a new model helper in `src/models/`, `src/util/Helpers.cpp` (`importPsgFiles`), and the zxtune fork's `uzx/ay_module.*`.

## What the code has today

| Need | Today |
| --- | --- |
| Marker storage | Tracktion's `MarkerTrack` with `MarkerClip`s, reached through `Edit::getMarkerManager()`: create, find by ID, next and previous from a time. `Edit::ensureMarkerTrack()` creates the track. µZX uses none of it |
| Section storage | Tracktion's `ArrangerTrack` with `ArrangerClip`s: a named range with a colour. Every edit has one (`Edit` calls `ensureArrangerTrack()` on load). The engine stores it and nothing more; moving sections is Waveform's own UI |
| Hidden global tracks | `TracksContainerComponent::buildTracks` skips the marker, arranger, chord, tempo and master tracks |
| Ruler | `RulerComponent`, 24 px: the upper half seeks, scrolls and zooms; the lower half is the loop strip |
| Lines down the tracks | `LoopLinesComponent` draws the loop range's ends over the tracks and lets the mouse through |
| Snapping | `RulerComponent::snappedTimeAt`: the visible grid, or whole frames while `Cmd` is held. Private to the ruler |
| Pattern positions of a module | Not exposed. The fork's `PlayToLoop` sees `TrackState::Position` and `Pattern` on every row and keeps only `RowStarts` and `LoopFrame`. [module-import.md](module-import.md) already plans `Tune::Positions` (pattern number and first frame of each order position) for the same purpose |
| Shortcut direction | [ux.md](../ux.md): a Bitwig-like scheme for new shortcuts; "marker navigation should be sequential" |

## Other software

From general knowledge of these applications, not re-checked for this note.

| Application | Where markers sit | How they look | Sections |
| --- | --- | --- | --- |
| Reaper | A lane at the top of the ruler | Red flag with number and name; dashed line down the tracks. `M` adds one, digits jump to one | Regions: coloured bars in a lane of their own |
| Pro Tools | A Markers ruler among the rulers | Small yellow tag, numbered as a memory location | Memory locations with a range |
| Ableton Live | The scrub area under the beat ruler | Small triangle with a name; previous and next buttons | None |
| Bitwig | The arranger's timeline header | Small tag with a name (cue markers) | None |
| Logic | Marker track, a global track at the top | Coloured bar from the marker to the next one, name inside | Arrangement track |
| Cubase, Studio One | Marker track, a global track at the top | Numbered flag or label with a line | Arranger track |
| Waveform | Marker track, a global track at the top | Labelled clip | Arranger track |

What they share:

- Markers sit in a thin lane next to the ruler, usually above the loop or cycle area, or on a global track at the top of the track list.
- A marker is a small tag or flag pointing at its time, with a number or a name; many add a thin line down the tracks.
- Points and ranges are kept apart: markers in one lane, sections in another.
- Adding a marker at the playhead and stepping between markers have keys; most put a marker list somewhere as well.

## Options

### 1. Where the markers go

**A. A marker lane above the ruler (recommended).** 14–16 px over the ruler, in the timeline's column, hidden in Viz mode like the ruler.

```
┌──────────────────────────────────────────────────────────────┐
│ ▼00          ▼01              ▼02 Chorus                     │  marker lane
├──────────────────────────────────────────────────────────────┤
│ 1        2        3        4        5        6        7      │  ruler: seek, scroll, zoom
│ ┃▶════════════════════════◀┃                                 │  loop strip
├──────────────────────────────────────────────────────────────┤
│ ┊            ┊                ┊                              │  tracks, faint marker lines
```

- The ruler keeps its two halves as they are; markers get gestures of their own without guessing which one a click meant.
- Costs 14–16 px of height in every window, the Player's too.

**B. Inside the ruler's upper half.** Tags drawn over the time labels.

- No extra height.
- The upper half already seeks, scrolls and zooms on a drag. A press on a tag would have to win over those, labels and tags collide, and a 12 px half is tight for a name.

**C. A global marker track in the track list.** A row like Logic's or Cubase's marker track, under the ruler.

- Room for long names and for ranges later.
- Scrolls with the tracks unless pinned, needs a header in the track column, and is the heaviest to build. Out of place in the Player.

### 2. How a marker looks

**A. A tag pointing down, with its name (recommended).** A small filled pentagon in the marker colour, its point on the marker's time, the name to its right in the lane. A name that would run into the next tag is cut with an ellipsis; the full name, the time and, for an imported marker, the position number show in a tooltip.

```
▼ Intro       ▼ 05      ▼ 05      ▼ 07
```

**B. A triangle, the name only on hover.** As Live's locators. Takes the least room; names are not visible at a glance.

**C. A coloured bar to the next marker.** As Logic. Reads as sections, which suits imported patterns, but then a marker is a range and the user's own points look like sections too. Better kept for a sections lane if one comes (option 4.B).

Line down the tracks: a faint 1 px line in the marker colour, over the tracks and under the loop lines and the playhead. Recommended on; an alternative is to show it only for the marker under the mouse or the one last jumped to.

Colour: one colour for all markers at first (`Colors::Timeline::marker`, distinct from the loop colours). User colours per marker are left out until asked for.

### 3. Gestures (Studio)

| Gesture | Where | Does |
| --- | --- | --- |
| Click | Tag | Moves the playhead to the marker |
| Drag | Tag | Moves the marker; snaps to the grid, or to whole frames while `Cmd` is held, as the loop edges do |
| Double-click | Empty lane | Adds a marker there, snapped |
| Double-click | Tag | Renames it, in place |
| Right-click | Tag | Rename, Delete |
| Right-click | Empty lane | Add Marker Here, Delete All Markers |
| Click | Empty lane | Moves the playhead, as on the ruler |
| Wheel, pinch | Lane | As on the ruler |

In the Player the lane shows the same tags; click, wheel and pinch work, and nothing edits.

Commands, in the Transport menu of both apps (Add Marker in Studio only):

| Command | Key |
| --- | --- |
| Add Marker at the playhead | `M` |
| Previous Marker | `Shift+Left` |
| Next Marker | `Shift+Right` |

Previous and Next go from the playhead, past markers closer than a millisecond (as `MarkerManager::getPrevMarker` does), and also work while playing. With no marker that way they do nothing. `Shift+Left` and `Shift+Right` are free in `MainCommands.h` today.

Left out until asked for: digits jumping to marker N (plain digits are reserved for tools in [ux.md](../ux.md)), a marker list window, looping between two markers.

### 4. Pattern positions of an imported module

The fork change is the same in every option: `PlayToLoop` keeps, per order position, its number, its pattern and its first frame, as `Tune::Positions` (refactoring step 6 of [module-import.md](module-import.md)). `TrackerModule` gains the list; `importPsgFiles` uses it after the tempo grid is applied, so the frames keep their times.

Covers every format zxtune plays as a tracker (PT3, PT2, STC, ASC, SQT, VT2 and the others). A TAYM states only its row lengths, and a PSG file has no patterns, so neither gets markers.

**A. A marker per position (recommended).** Named by the pattern, two digits: `05`. A pattern played at three positions gives three markers named `05`; the tooltip says "Position 3, pattern 05". Cheapest, and the same lane serves the user's markers.

**B. An arranger clip per position.** A range from the position's first frame to the next one's, named the same way, shown as coloured bars in a sections lane. Fits "sections" better and leaves room for moving sections later, but needs a second lane and its own drawing and gestures. Worth it only if section editing is planned.

**C. Split the PSG clip at each position.** Not recommended. A PSG clip keeps all of its frames in its own state, so every piece would carry a copy of the whole tune; tuning detection and Turbo Sound linking work per clip; an edit across a boundary gets awkward. It is also the only option that changes what an older app would load.

Imported markers and the user's are the same kind of marker; the user can rename, move or delete them in Studio. If they ever need telling apart, a property on the `MarkerClip` state marks the imported ones.

How long the module played up to the loop decides how many markers there are: positions after the loop point are played once and marked once.

### 5. Sync to tempo

**A. Absolute time (recommended).** A marker stays on its frame when the tempo changes, as PSG data does.

**B. Bars and beats.** A marker moves with the music when the tempo changes. Tracktion picks between the two from an engine setting by default; µZX would pass the sync type itself.

## Edit file format

No bump. The marker track is a Tracktion track: an older µZX loads it, hides it, and saves it back unchanged. The user guide gets the lane, the gestures and the three commands; the CHANGELOG gets one line per app.

## Order of work

Refactor first.

1. **Snapping in `TimelineGrid`.** Move `RulerComponent::snappedTimeAt` there, so the ruler and the lane share it. No behaviour change.
2. **`EditMarkers`** in `src/models/`: add, move, rename, delete, previous and next, over `MarkerManager`, creating the marker track on first add. Unit tests.
3. **Marker lane and lines.** `MarkerLaneComponent` (option 1.A, look 2.A, gestures 3), and the marker lines over the tracks, drawn as `LoopLinesComponent` draws the loop's ends. `EditComponent` lays them out.
4. **Commands and menu items**, with the Player's subset.
5. **Fork `Positions`, `TrackerModule::positions`, markers on import** (option 4.A). A test that imports the PT3 fixture and checks the markers' names and frames.
6. **`user-guide.md`, `CHANGELOG.md`, figures** regenerated with `scripts/illustrate_user_guide.py`.

## Open questions

- Option 1: lane above the ruler, inside it, or a marker track.
- Option 2: tag with name, or triangle with the name on hover; lines down the tracks always, or only for one marker.
- Option 4: markers per position now, or arranger clips with a sections lane.
- Whether the Player should keep the lane when a file has no markers, or hide it until there is one.
