# µZX UX

Shared interaction decisions, then the yardstick for polishing the Player: a change is worth making when it takes a step out of one of these tasks or makes what the task needs easier to see.

## Tasks

In order of how often people do them.

1. **Open a PSG file (or a Turbo Sound set) and listen to it.**
   Now: double-click it in Finder, then `Space` (2 actions). A Turbo Sound set adds one question.
   Later: TAYM files and tracker modules (PT3, Vortex Tracker, Bitphase, ChipNomad, …) the same way, each converted to PSG or TAYM on open; the formats are under **Import** in [ROADMAP.md](ROADMAP.md).
2. **See what each AY channel is doing while it plays** — notes, envelope, level.
   Now: no action; the view follows the playhead.
3. **Change the chip settings (clock, stereo, mute) and save the result.**
   Now: one click per setting, then `Cmd+S`; an edit opened from a PSG file adds a save dialog.
4. **Record or export a visualization video clip** — for some users.
   Now: **File → Render Video...**, then **Render...** in its one-page dialog; it shows Viz mode as on screen. More options are under **Render Video options** in [ROADMAP.md](ROADMAP.md).

## Always visible

In the Player window at any size; nothing below gets hidden or squeezed out. Viz mode shows what its options choose.

- Play / pause and the position
- Each track's notes around the playhead
- Each chip's level meter
- The master volume knob and the master level meter
- The file name, in the window title

## Keyboard and timeline zoom

Use a Bitwig-like scheme as the reference for new editing, tool and grid shortcuts, rather than mixing defaults from several editors. This is a direction, not full Bitwig compatibility: existing shortcuts are not all migrated. Reserve plain digits for tools and Cmd/Ctrl+1…9 for zoom; only the first four zoom commands are assigned. Marker navigation should be sequential when implemented. `X` for previous zoom is an intentional exception inspired by Live. The implemented bindings live in [Keyboard and Mouse](user-guide.md#keyboard-and-mouse).

Player and Studio share **View → Zoom** and the timeline context menus. Frames, Groove Rows, Beats and Bars each give their reference interval 16 pixels. Use the tempo and meter at the playhead and the average groove row, falling back to a quarter beat without subdivisions. Calculate these intervals with the musical snap ladder even when the ruler shows seconds or frames: changing the timecode format changes the grid and labels, not the preset scale. The actual visible snap interval still comes from that format's grid.

Previous Zoom restores horizontal position and scale without seeking or undoing edits. Each command or drag is one history step; wheel, pinch and zoom in/out events less than 500 ms apart form one step. Scrolling and automatic playback following do not add steps. History keeps up to 64 views for the current view session and is not saved in the project. No zoom toolbar yet.
