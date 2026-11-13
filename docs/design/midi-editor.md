# Chiptune MIDI editor

> **Status: proposal, not implemented.** [Decided](#decided) lists what is settled. Everything else is a recommendation, and [Open questions](#open-questions) lists what still needs an answer. Today Studio shows a MIDI clip's notes and cannot create a clip or edit a note.

How notes are written and edited in µZX Studio, and how a note names its instrument. It builds on [midi-ay-pipeline.md](midi-ay-pipeline.md): the editor writes performance MIDI, the input of the chip instrument. Code it touches: `src/gui/timeline/`, `src/models/Behavior.h`, `src/plugins/uZX/instrument/`.

## What makes it a chiptune editor

- **A tune is instrument-heavy.** In 193 of the top-rated PT3 modules on zxart.ee a module uses 12 samples at the median, a channel uses 7, and 61% of notes switch their channel's sample. One instrument per track, as a DAW has it, would spread a three-channel tune over tens of tracks with a few notes each.
- **The devices matter while notes are written.** A note's sound is its instrument's knobs, so the instrument has to stay on screen next to the notes. An editor in the bottom panel takes the place of the devices.
- **A tune has few tracks.** The chip has three channels, so a track row can be tall. The Player already fills its window with a few rows of notes.
- **The grid is the frame.** Notes sit on frames and groove rows, which the timeline's snap ladder already has.

## Decided

- An instrument is chosen **per note**, not per track.
- A note's instrument is stored as its **colour**, which `tracktion::MidiNote` already has, saves and undoes. No new note property and no engine patch.
- The instrument goes on the wire as a **controller**, not a program change.
- A note's **timbre** (controller 74) is one designated knob of its instrument. Other instrument knobs and effects are **automation lanes**.
- The numbered palette a colour indexes is **per project**. It points into an instrument library that every instrument plugin shares: builtin folders, then the project's.
- The library has a **browser panel** of its own. How it is organized (folders, tags, facets) is a separate design.
- The scheme stays close to what other DAWs can produce (articulations, selector devices, note colours), since the plugins talk only MIDI and may ship as a pack.

## What a note holds

Everything is already in `tracktion::MidiNote` and `tracktion::MidiList`.

| A note's | Stored as | Played as |
| --- | --- | --- |
| Key, start, length | The note | Note on, note off |
| Level at its start | Velocity | Note-on velocity |
| Instrument | Colour, 0 to 127: a slot of the project's palette | Controller 102 on the note's channel, before its note on |
| Designated knob | Timbre, a start value and points along the note | Controller 74 on the note's channel |
| Pitch along the note | Bend, as above | The pitch wheel of the note's channel |
| Level along the note | Pressure, as above | Channel pressure |
| Mute | Mute | Not played |

Timbre, bend and pressure exist only in a clip in MPE mode (`MidiClip::setMPEMode`), where each note plays on a channel of its own from 2 up. The editor creates every clip in MPE mode, so two notes of a chord can differ in all three.

## The palette and the selector

**The palette** is up to 128 numbered slots, stored in the project. A slot has a name, a display colour and a reference to a library instrument. **Slot 0 is the instrument plugin's own patch**: a clip that never sets a colour plays as a track with one instrument, which is what an imported MIDI file or a recorded take is.

**The selector** is controller 102 with the slot's number as its value. The instrument latches it at note on: a note keeps the instrument it started with. `MoTool::Behavior` already overrides `createPlaybackMidiSequence` for PSG clips; for a MIDI clip it adds the selector before each note on of Tracktion's default sequence, on that note's channel. It is sent before every note, slot 0 included, because an MPE clip hands channels out in turn and a channel still holds the last note's selector.

The selector is part of the instrument's input only. The note wire after the instrument does not change: the instrument has already turned a slot into pitch, level and timbre.

Controller 102 is free in `MidiCCType`, `NoteCC` and `PsgCC`, and in the knob tables of `PsgCC.h`. Controller 103 is kept free for a second per-note selector.

**From another DAW** the same input is a controller lane, or an articulation map that sends a controller per articulation. A selector on channel 1 of an MPE zone applies to the whole zone, so a host that cannot address member channels can still switch instruments.

## Where the editor is

| Option | For | Against |
| --- | --- | --- |
| **In the clip, on the timeline** (recommended) | The bottom panel keeps the devices. Notes line up with the other tracks and the ruler. Neighbouring clips are editable without opening anything | One horizontal zoom for arrangement and notes. The edited row needs most of the window's height |
| The centre, in place of the arrangement | The whole window for notes, devices still below | The other tracks are gone while editing. A second view to build and to switch |
| A tab of the bottom panel | Arrangement and notes at once, each at its own zoom. What most DAWs do | Hides the devices, which is the thing to avoid |

Separate editors win in general DAWs for reasons that are weaker here. A session of sixty short tracks cannot give one of them four octaves of height; a tune of three or four tracks can. Clip gestures and note gestures collide in a clip body; here a clip already has a header that selects it, and later moves and trims it, so the body is free for notes. Two zoom levels are a real loss, and the zoom presets (`Cmd+1` to `Cmd+4`) with Previous Zoom (`X`) are the answer to it.

**No edit mode.** A row tall enough to draw its note lanes is an editor; a shorter row shows its notes and is not edited. What can be edited is what can be seen, and nothing has to be entered or left. Double-clicking a short MIDI clip's body makes its row tall and fits the pitch range to the clip's notes.

```
┌ transport ─────────────────────────────────────────────┬ Instruments ─┐
│ ruler                                                  │ Palette      │
├─────────┬──────────────────────────────────────────────┤  1 ■ Bass    │
│ Lead    │▒▒ clip header ▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒ ■1 ■2 ■5 │  2 ■ Lead    │
│ R M S I │C5│    ■■■■       ■■                          │  5 ■ Snare   │
│         │  │         ■■■■      ■■■■■■                  │ Library      │
│         │C4│ ■■                         ■■■            │  ▸ Builtin   │
│         ├──┴───────────────────────────────────────────┤  ▸ Project   │
│         │ velocity  ▌   ▌    ▌  ▌     ▌                │              │
├─────────┼──────────────────────────────────────────────┤              │
│ Bass    │ ▬▬ ▬  ▬▬▬   ▬      a short row: notes shown  │              │
├─────────┴──────────────────────────────────────────────┴──────────────┤
│ Track Devices   µZX Instrument: 2 Lead  →  MIDI to PSG  →  AY chip    │
└───────────────────────────────────────────────────────────────────────┘
```

## The editor

**Pitch axis.** One lane per MIDI key, all the same height, drawn as the Notes view of a PSG clip draws its lanes and piano keys, and named by the tuning preset of the track's MIDI to PSG. The row's pitch range is the track's `getMidiVisibleProportion()` and `getMidiVerticalOffset()`, which `MidiClipComponent` reads already and Tracktion saves. Dragging on the piano keys moves the range as dragging the ruler moves time: up and down scrolls, sideways zooms.

**Time axis.** The timeline's own: its zoom, its grid and its snap interval. A note's start and length snap to the grid the ruler shows.

**Notes.** A note is filled with its slot's colour. The clip header's legend, which a PSG clip uses for A B C E N, lists the slots the clip uses.

| Gesture | Where | Action |
| --- | --- | --- |
| Click | A note | Select it; the devices panel shows its instrument. It sounds, through the track's devices |
| Click | An empty lane | Move the playhead, as anywhere on the tracks |
| Double-click | An empty lane | Add a note: the grid's length, the last velocity, the slot picked in the palette |
| Double-click | A note | Delete it |
| Drag | A note | Move it, with the rest of the selection; `Option` copies |
| Drag | A note's end | Change its length |
| Drag | An empty lane | Select the notes in the rectangle |
| `Delete` | | Delete the selected notes |
| Arrow keys | | Move the selection a grid step or a semitone; with `Shift`, a bar or an octave |
| `Option`+click | A note | Pick its slot up: new notes get it |

Every change is one undo step. Tools on the digit keys, which [ux.md](../ux.md) reserves for them, wait until the pointer alone proves too slow.

**Lanes under the notes**, inside the row and collapsible, as Viz mode puts scopes under the notes:

- **Per-note values**: velocity first, then timbre, then the bend and pressure curves.
- **Automation**: a parameter of one of the track's devices, as a Tracktion automation curve on the track.

**The instruments panel** is a column at the right of the timeline: the project's palette on top, the library under it.

- Click a slot: new notes get it.
- Drag a slot onto the selected notes: they change to it.
- Drag a library instrument onto the palette: it takes the next free slot.

**The bottom panel** shows the devices of the track whose note or MIDI clip is selected (`DetailsPanelComponent::updateTabVisibility` flips to them only for a selected track today). The instrument's device shows the slot of the selected note, so its knobs are edited with the notes in view.

## What the code has

| Need | Today |
| --- | --- |
| A MIDI clip's notes on the timeline | `MidiClipComponent`: paints them, takes no note gestures |
| Note lanes, names, piano keys | `src/gui/timeline/NoteLanes.*`: `paintNoteLanes` draws a list of `NoteLane` for a `NotesLayout`. Only `PsgClipComponent` uses it, with lanes from its tuning's `PeriodGrid` |
| A clip handle apart from its body | `ClipHeader`, with `getLegendWidth` and `paintLegend` for the slots |
| A band under the notes of a row | `TrackRowComponent`'s scopes band in Viz mode |
| Selected notes | `tracktion::SelectedMidiEvents` |
| Creating a MIDI clip | `addMidiClip` in `MainCommands.h`, commented out |
| Emitting the selector | `createPlaybackMidiSequence` in `src/models/Behavior.h` |
| An instrument per slot | Nothing: `ChipInstrumentPlugin` has one set of parameters |
| Controller lanes of an MPE clip | A clip sends them on its own channel, 1, where the instrument reads them for channel 1's notes only |

## Order of work

Refactor first, then change behaviour. Each step leaves something a user can do.

1. **Notes in place**: insert a MIDI clip; a MIDI clip draws its lanes and keys with `paintNoteLanes`; add, move, resize, delete and select notes; undo; a note sounds when clicked; the bottom panel keeps the devices.
2. **Velocity lane.**
3. **Palette and selector**: the palette in the project, an instrument per slot in the instrument plugin, controller 102 from note colour, notes in their slot's colour, the palette half of the instruments panel.
4. **Timbre lane**, and the instrument reading channel 1's controllers for its whole zone.
5. **Library browser and automation lanes.**

Steps 1 and 2 are worth having with the one-patch instrument that exists. Step 3 is where the instrument itself has to grow, and it decides whether the `.uzx` format version is bumped (see **Edit File Format Version** in `AGENTS.md`).

## Open questions

- **Placement.** In the clip is this document's recommendation; the choice between it and a centre view has not been made.
- **Row height while editing.** Whether the other rows shrink to their minimum by themselves when one is made tall, or the user sizes them.
- **Ornaments.** A colour holds one number, and the instrument takes it. The cheapest answer is that a slot is an instrument with its ornament: the same PT3 modules use 27 sample and ornament pairs at the median and 53 at the ninetieth percentile, which fits 128 slots. The other answer is controller 103 with a store per note that does not exist.
- **What an automation lane addresses** when a track plays many instruments. A plugin's parameters are a fixed list, so a knob of one slot's instrument is not one of them. A fixed set of macro knobs that each instrument maps is the likely shape.
- **The timbre controllers on the instrument's input** (80 to 86, 2, 70). Once a slot's instrument sets tone, noise and envelope, they are either overrides or nothing.
- **The selector's number.** 102 is a candidate, not final.
- **Matching the selector to its note.** The sequence Tracktion returns has note ons, not notes; a groove or quantize setting can move them. Needs a test before it is trusted.
- **A plugin pack in another host** has no project to keep the palette in: each plugin instance would save a copy.
