# µZX Player UX

The yardstick for polishing the Player: a change is worth making when it takes a step out of one of these tasks or makes what the task needs easier to see.

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
