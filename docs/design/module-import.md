# Tracker module import as notes

> **Status: plan, not implemented.** [Decisions](#decisions) says what is settled and what is a recommendation. Today a tracker module opens as a PSG clip only (`Helpers::importPsgFiles`).

How µZX Studio opens a tracker module as notes: each AY channel a MIDI track with one MPE clip per order-list position, each note naming its sample and ornament through its colour, played through µZX Instrument, MIDI to PSG and one AY chip. It builds on [midi-editor.md](midi-editor.md) (an instrument per note as its colour) and [midi-ay-pipeline.md](midi-ay-pipeline.md). Code it touches: the zxtune fork's `uzx/`, `src/formats/tracker/`, `src/plugins/uZX/instrument/`, `src/plugins/uZX/midi_effects/`, `src/plugins/uZX/notes_to_psg/`, `src/models/Behavior.h`, `src/util/Helpers.*`.

The PSG import stays as it is, and stays the only one in the Player.

PT3 and Vortex Tracker II come first. They share one parser and one player in zxtune, and the instrument's step model follows their rules. Other formats are added one at a time afterwards.

About two thirds of the work is prerequisites the two documents above already call for: an instrument per palette slot, the selector controller, and several tracks feeding one chip. The import itself is a pure conversion on top of them.

## What the code has today

| Need | Today |
| --- | --- |
| The module's patterns, samples, ornaments | Not exposed. `UzxAY::Play` in the fork's `uzx/ay_module.cpp` returns registers per frame, `RowStarts` and `LoopFrame`. `PlayToLoop` sees position and pattern per row and drops them |
| A parser to read them with | zxtune's `Formats::Chiptune::ProTracker3::Builder`: `Parse` calls it with every sample, ornament, position and cell. PT3 and VT2 text share it. Each of the other 13 formats has its own builder and its own sample layout; the pattern model (`Cell`, `Command`) is common |
| The player's rules | `Module::Vortex::DataRenderer` in `vortex.cpp`: about 200 lines, the reference for the instrument |
| Import entry | `Helpers::importPsgFiles` in `src/util/Helpers.cpp`: reads, makes tracks, sets the chip, the tuning, the tempo grid and the loop in one function |
| Instrument | `ChipInstrumentVoice`: two ADSRs. No timbre, no steps, one patch per plugin, fixed 50 Hz (`noteFrameRate`) |
| Instrument per note | Nothing. Palette and controller 102 are designed in `midi-editor.md` |
| Note wire | `NoteVoice`: pitch in semitones, level 0..1, timbre switches, noise period. `NoteEncoder` always sends on channels 2 to 16 |
| Mixer | `ChipMixer::render`: period from `tuning.midiNoteToPeriod(pitch)`, volume `round(level * 15)` |
| Tracker tables | `TuningTable::midiNoteToPeriodRaw` rounds the pitch to a whole note, so a bend does nothing under a table tuning |
| Several tracks into one chip | Not used. Tracktion sums the tracks whose output is another track before that track's plugins (`getDirectInputTracks` in `tracktion_EditNodeBuilder.cpp`); that their MIDI is merged there is read from the source, not tested |
| Per-note values in a clip | `tracktion::MidiNote` colour 0 to 127; bend, pressure and timbre curves in an MPE clip (`MidiExpression`) |
| A note longer than its clip | Not possible: the engine sends note offs at a clip's end (`LoopingMidiNode`) |
| Repeated patterns | `Clip::setLinkGroupID`: linked clips copy each other's edits |
| A reference to compare with | `readTrackerModule` frames, zxtune's `regression/*.debugay`, `PsgRecorder`, one PT3 in `tests/fixtures/modules` |

## Decisions

Decisions 3 and 4 are settled. The others are recommendations the rest of the plan assumes.

1. **Tracker effects become note curves and controllers, not effect plugins.** An effect that belongs to one note becomes that note's MPE curve (pitch slide as bend, volume change as pressure); state that belongs to the channel or the chip (envelope on, noise base, envelope period and shape) becomes controllers and notes on the tracks. Chip effects do not exist yet, and an automation lane cannot address one slot's instrument (open question in `midi-editor.md`); a chip-effect plugin per tracker effect, driven by automation, needs both first.
2. **Fidelity target.** The imported project, rendered through the pipeline, gives the same registers as `UzxAY::Play`, frame by frame. Allowed difference: a tone or envelope period one unit off while a slide runs. Anything else the import cannot express is counted and shown after the import.
3. **Sample tone offsets travel as a period offset**: a whole-number field of a voice, sent on the note wire by a controller of its own and added by the mixer after the tuning. Only the PT3-like step instrument writes it. Converting period units to semitones inside the instrument would need the tuning and the clock there, and loses exactness. Slides stay bends, because a bend is what the editor will show; for that `TuningTable` has to interpolate between its periods.
4. **A note that sounds across a pattern boundary stays in the clip it starts in.** That clip runs on past its pattern's end until the note ends, so it overlaps the next clip on the track. Tracktion plays overlapping MIDI clips together, so nothing is flagged and the engine is not patched. A clip cut at the pattern's end cannot hold the note, and a note split there restarts its sample at each pattern, which is heard on every held bass or pad. If overlaps prove awkward on the timeline, the fallback is a tie mark on a continuation note in the next clip, carried as the top value of the note's timbre start, which Tracktion already stores for an MPE note.
5. **The envelope has a track of its own.** Its period and shape are chip state set from any channel, and the pipeline already has the voice for it: one that drives the envelope and takes no chip channel.
6. **A palette slot is a sample with an ornament**, the cheapest answer `midi-editor.md` names. Slots are numbered by use, most used first; slot 0 stays the plugin's own patch.
7. **Clips are per order position, linked when equal.** A pattern played at two positions gives two clips. They get one link group only if their converted content is the same, since what they inherit from the pattern before, and how the pattern after ends their tails, can differ.
8. **Studio only.** A separate command, File → Import Module as Notes. The Player keeps opening modules as PSG.

## Target shape

The import is three stages, each with one kind of work, and only the last touches an edit.

```
file bytes
  │  UzxAY::Read            zxtune fork, uzx/: parse, no playing
  ▼
TrackerSong                 src/formats/tracker: positions, patterns, cells, samples, ornaments
  │  arrangeSong            pure: walks the song in play order, resolves channel state
  ▼
NoteArrangement             per chip: palette, and per track per position its notes,
  │                         curves and controllers, all in frames
  │  insertArrangement      the only stage that knows tracktion::Edit
  ▼
tracks, clips, palette, chip settings, tempo grid, loop, markers
```

What one chip of a module becomes in the edit:

```
"Tune A"   MPE clips, one per position ─► µZX Instrument (voices on ch 2–5)  ─┐
"Tune B"   MPE clips                   ─► µZX Instrument (voices on ch 6–9)  ─┤ output: Tune
"Tune C"   MPE clips                   ─► µZX Instrument (voices on ch 10–13)─┤
"Tune Env" plain clips on ch 16, envelope-driver notes, no devices          ─┘
"Tune"     no clips ─► MIDI to PSG (the module's table and clock) ─► AY chip
```

- **Positions come from frames, not rows.** A note starts at `frame / framesPerBeat` beats, with the frame taken from `RowStarts`. Speed changes and swing then need no tempo changes: the edit keeps one tempo, set as today by `PsgTempo`.
- **A clip states what it needs at its start**: the channel's envelope switch and noise base, as a PSG clip restates the registers. A clip can then be moved or played alone.
- **A Turbo Sound module** gives this group once per chip.
- **The palette is in the project**, one for all three instruments, as `midi-editor.md` decided. For an import a slot holds its patch itself; the library and its browser are not needed yet.
- **Markers at position starts** use the same list of positions from the fork, so that fork change is made once for both.

## Mapping

PT3 and VT2. "Channel controller" means a controller on channel 1 of the channel's MPE clip, which the instrument applies to every note of its track.

| In the module | In the edit | Exact |
| --- | --- | --- |
| Note | A note; its key is the table entry's MIDI note in the tuning preset | Yes |
| Sample and ornament in force | The note's colour: a palette slot | Yes |
| Rest | The note before it ends | Yes |
| Note held past the pattern's end | Its clip runs on until the note ends, over the next clip | Yes |
| Volume column | Velocity of the notes that follow; under a sounding note, a pressure point | Yes, except volume 0 with the envelope on |
| Sample lines | The slot's patch steps: level, tone, noise and envelope switches, tone offset, noise offset, their accumulate flags, volume slide, loop | Yes |
| Ornament lines | The patch's semitone steps and loop | Yes |
| Volume times sample level | The instrument's volume law: PT 3.3–3.4 table, PT 3.5 table, by the module's version | Yes |
| Glissando | Bend points on the note, one per slide step | Within 1 |
| Slide to note | A note at the target key with a bend from the old pitch to zero | Within 1 |
| Sample offset | The note's timbre start value: where the patch starts | Yes |
| Ornament offset | Not expressed | Counted |
| On/off vibrato | Pressure switching between zero and the level | Approximate: the sample runs on while off |
| Envelope shape and period | A note on the Env track: key and bend from the period, controller 70, a restart on 83 | Within 1 on long periods |
| Envelope on, envelope off for the channel | Channel controller 82 | Yes |
| Envelope slide | Bend points on the Env note | Within 1 |
| Envelope offset in a sample line | Not expressed in the first version | Counted |
| Noise base | Controller 2 on all three channel tracks; zero at each clip start | Yes |
| Noise offset in a sample line | Added by the instrument to the voice's noise period | Yes |
| Which channel's noise wins (the last) | Priority C over B over A | Yes |
| Speed and speed changes | Row lengths in frames; nothing stored | Yes |
| Order list, loop position | Clips, the transport's loop range, markers | Yes |
| Title, note table, ChipFreq, IntFreq | Track names, MIDI to PSG's tuning preset, the chip's clock, the project's frame rate | Yes |

## Order of work

Refactor first, then change behaviour.

### Refactoring

No step here changes what a user sees or hears. Each is its own commit with the existing tests passing.

1. **Split `importPsgFiles`.** Move it out of `src/util/Helpers.cpp` into `src/models/import/TuneImport.*` as separate functions: read a file, apply a chip's clock and type, choose the tuning, apply the tempo grid, set the loop range. The notes import calls the last four as they are. `Helpers.test.cpp`'s import tests move with it.
2. **Voice logic out of the MPE template.** `ChipInstrumentVoice` mixes ADSR math with JUCE's MPE note. Extract a plain function: patch, note state and frame number in, `NoteVoice` fields out. The ADSR patch is its first implementation. This is the shape `midi-ay-pipeline.md` asks of every stage.
3. **Frame rate as a value.** Replace the `noteFrameRate` constant with the project's frame rate, passed to the instrument and the mixer. Modules at 48.8 Hz or a VT2 `IntFreq` need it. At 50 fps nothing changes.
4. **`NoteEncoder` takes its channel range.** Default 2 to 16, as now.
5. **`createPlaybackMidiSequence` for note clips** moves from `ExtEngineBehaviour` in `Behavior.h` into a function of its own, `NoteClipSequence`, still returning Tracktion's default sequence. The selector is added there later.
6. **Fork: `PlayToLoop` keeps what it sees.** `Tune` gains `Positions` (pattern number and first frame of each order position). `RowStarts` and `LoopFrame` stay.

### Behaviour

In order. Steps 2 to 4 are useful without the import: they are step 3 of `midi-editor.md`.

1. **Prove four engine assumptions with tests**, before anything is built on them.
    - Tracks whose output is another track reach that track's plugins with their MIDI merged.
    - In an MPE clip's playback sequence a controller put before a note on stays with that note, on its channel (open question in `midi-editor.md`).
    - The instrument's frames fall on the timeline's frame grid, so a note placed on frame N starts in frame N.
    - Two MPE clips that overlap on one track both play, and a note off from one at the time of the other's note on, same key and channel, does not cut the new note.

    If the first fails, the fallback is one track per chip with overlapping clips and an instrument that reads four zones; the plan after step 4 does not change.
2. **Read the song.** Fork: `UzxAY::Read(data, size)` returns plain structs, no zxtune types: positions, patterns as rows of three cells (note or rest, sample, ornament, volume, commands), samples, ornaments, version, initial speed. It implements `ProTracker3::Builder` inside `uzx/`, so zxtune's own sources are not patched. µZX: `readTrackerSong` in `src/formats/tracker/TrackerSong.*` converts it to `TrackerSong`. Empty for formats without a reader, and the caller falls back to PSG.
3. **The step patch.** `ChipPatch` (sample steps, ornament steps, loops, volume law) and `PatchPlayer`, the second implementation of the voice function from refactoring step 2, following `Vortex::DataRenderer`. With it:
    - `NoteVoice::periodOffset`, its controller on the note wire, and `ChipMixer` adding it after the tuning, wrapped to 12 bits as PT3 does.
    - `TuningTable` interpolates between neighbouring periods for a fractional pitch.
    - The voice's noise period includes the patch's offset.
4. **Palette and selector.** The palette as a tree in the edit, slots holding patches. `NoteClipSequence` sends controller 102 from the note's colour. The instrument latches the slot at note on, reads channel 1's controllers for its whole zone, and has a priority and an output channel range as parameters. Decide the `.uzx` format version here: an older app would play every note with one patch, so a bump is likely.
5. **Arrange and insert, without effects.** `arrangeSong` walks the positions keeping each channel's state as the player does (sample, ornament, volume, envelope switch, sounding note) and gives notes, each clip's length with its tail, velocities, the Env track's notes and the channel controllers. `insertArrangement` makes the tracks, routes them, fills the palette and calls the functions split out of `importPsgFiles`. File → Import Module as Notes. After this step a module without effect commands plays exactly.
6. **Effects.** Glissando, slide to note and envelope slide as bend points; volume changes as pressure; sample offset as the timbre start; on/off vibrato. `arrangeSong` returns the count of what it could not express, by kind, and the import shows it once.
7. **What the user sees.** Notes painted in their slot's colour and the slot legend in the clip header (both from `midi-editor.md`), clip names "Pattern N", `user-guide.md`, `CHANGELOG.md`, and the questions this work settles removed from `midi-editor.md`.
8. **Linked clips** for positions whose content is equal.
9. **Envelope offset of sample lines**: a voice field the mixer sums into the envelope period. Only if step 6's counts show modules need it.
10. **Other formats**, one at a time: a `Read` for its builder, its sample converted to patch steps. A format whose sample cannot be written as steps stays PSG-only.

## Tests

One measure runs through the steps: **the share of frames whose registers equal `UzxAY::Play`'s**. A `TrackerImportFidelity` test imports a fixture, plays the chip track offline with a `PsgRecorder` on its mixer, as Render to PSG does, and compares frame by frame. Each step states the share it must reach.

| Step | Test | Passes when |
| --- | --- | --- |
| 1 | `TrackRouting`, `NoteClipSequence`, `InstrumentFrameGrid`, `OverlappingClips` | The four assumptions hold |
| 2 | `TrackerSong` on `nq - Frontiers (2016).pt3` and zxtune's `hypergy.pt3`, `;-).pt3` and the VT2 text | Position and pattern counts, a known cell, a known sample's lines and loop |
| 3 | `PatchPlayer`: one channel's notes fed by hand, mixed by `ChipMixer` | That channel's registers equal zxtune's on rows without effect commands |
| 3 | `TuningTable`, `NoteFrame`, `NotesToPsgMapper` additions | Interpolation between periods; the period offset survives the wire |
| 4 | `ChipInstrument`: selector, zone controllers | A note plays its slot's patch; channel 1's controllers reach every note |
| 5 | `SongArrangement` (pure, no edit) | Notes, clip tails and slots of a small hand-written song |
| 5 | `TrackerImportFidelity` on a new fixture without effect commands | 100% of frames |
| 6 | `TrackerImportFidelity` on the four modules above | 100% within the allowed one-unit difference, apart from frames under a counted effect |

Two fixtures are needed that the repository lacks: a short PT3 without effect commands, and one that uses each command once. Both can be written in Vortex Tracker's text format.

## Known gaps and risks

- **Notes cannot be edited yet.** Studio shows a MIDI clip's notes and has no note gestures (`midi-editor.md` step 1). The import gives a project that plays and can be looked at; editing it is that other work.
- **The instrument follows PT3's rules.** Version quirks in `Vortex::DataRenderer` (the slide counter from version 7, the slide carried over from version 6) go into the patch player or the arrangement as they turn up in the fidelity test. Other formats will sound right only as far as their samples and commands fit these rules.
- **More than 127 sample and ornament pairs** in one module: the least used share a slot with ornament 0, and the import counts them. `midi-editor.md` measured 53 pairs at the ninetieth percentile.
- **Volume 0 with the envelope on** still sounds in PT3. A pressure of zero cannot be told from no pressure in an MPE clip, so this case is counted, not played.
- **Clips overlap where a note is held over a pattern's end.** The engine plays that; whether µZX's timeline draws and selects overlapping clips well is not checked yet. A clip's right edge is then its last note's end, not the pattern's, and a pad held over four patterns gives a clip five patterns long.
- **Fifteen voices on the note wire** are shared by three instruments, four channels each, plus the Env track. Enough for an import; a chord of more than four notes on one track would steal.
- **Envelope offsets in sample lines** are left out until behaviour step 9. How many modules use them is unknown; step 6's counts on a set of real modules answer it.
- **The clips are not tied to the file.** After the import the project stands alone: opening the module again imports it again.
