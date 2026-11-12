# µZX User Guide

µZX is a suite of applications for working with PSG chip music (AY-3-8910 / YM2149), targeting ZX Spectrum demoscene production.

The screenshots show the macOS build using the [Guide demo project](../tests/fixtures/Guide%20demo.uzx).

---

## Applications

| App | Description |
|-----|-------------|
| **µZX Studio** | Full-featured PSG music editor with timeline, instruments, and automation (Work in progress) |
| **µZX Player** | Lightweight player with visualization for `.psg` files, tracker modules (`.pt3`, `.stc`, …) and `.uzx` files |
| **µZX Tuning** | Tuning table (note table) editor/viewer. Supports equal temperament and just-intonation |

---

## µZX Player

A minimal player for listening to PSG chip music files.

![µZX Player](uzx-player.gif)

### Opening Files

- **Drag and drop** a `.psg` file, a tracker module or a `.uzx` file onto the window.
- **File menu → Open** (`Cmd+O` / `Ctrl+O`).
- **Double-click** a file in Finder (macOS). µZX Player is the default app for `.psg` files. Tracker modules and `.uzx` files open in it when no other app claims them, such as a zxtune player for modules, or µZX Studio, which opens `.uzx` files when it is installed. To pick the Player anyway, use **Open With**, or **Get Info → Open with → Change All…** for every file of that type. Early Vortex Tracker's `.txt` modules are not registered.
- On Windows and Linux the Player registers no file types: choose it in the file manager's **Open with**, which passes the file on the command line.
- Pass a file path as a command-line argument.

Drop or open several `.psg` files at once to play them together, each on its own track with its own AY chip, as a Turbo Sound tune has one file per chip. They replace the open edit, which is named after the start their names share: `song_1.psg` and `song_2.psg` open as `song`.

A PSG file plays at 50 frames a second, or at the rate its header gives when the file is version 10 or later. The project takes the frame rate of the file opened last. Most files don't say their rate, so a tune written for 100 or 200 Hz opens two or four times too slow: pick its rate under **FPS** in the transport bar, or under **Frame Rate** in the clip's right-click menu. The clips play at the new rate. When they are the only clips in the project, the whole timeline scales: the clips start as many frames in, and the tempo scales with the rate, so a beat keeps its frames and the grid its groove, and a module's own row timing is kept: 41 BPM at 50 Hz becomes 164 BPM at 200 Hz. Past 300 BPM, the most the tempo goes to, a beat takes twice the frames. Otherwise the clips keep where they start and the tempo stays.

**Tracker modules.** µZX opens ZX Spectrum tracker modules as well: Pro Tracker 1, 2 and 3 (`.pt1`, `.pt2`, `.pt3`, Turbo Sound PT3 files included), Vortex Tracker II text modules (`.vt2`, or `.txt` from its early versions; Turbo Sound files included), Sound Tracker (`.stc`, `.st1`, `.st3`), Sound Tracker Pro (`.stp`), ASC Sound Master (`.asc`, `.as0`), SQ-Tracker (`.sqt`), Pro Sound Creator (`.psc`), Pro Sound Maker (`.psm`), Global Tracker (`.gtr`) and Fast Tracker (`.ftc`). The format is recognised from the file's content. µZX plays the module with [zxtune](https://github.com/vitamin-caig/zxtune)'s players up to where the tune loops and opens the result like a PSG file: each AY chip on its own track, two or three for Turbo Sound, named after the module's title, or after the file when the module has none. The module also brings what a PSG file lacks: its note table becomes the tune's tuning table when µZX has that table (see **Tuning table and key** under µZX Studio's Timeline below), and its rows set the tempo (see the BPM control under [Transport Bar](#transport-bar)). A Vortex Tracker module also brings the chip clock and the frame rate it was written for, such as a Pentagon's 1.75 MHz and 48.828 Hz; the clock is set on the tracks the module gets to itself, and the frame rate on the project. Other tracker modules store neither, and none stores the chip type or stereo layout, so those stay as they are.

**Turbo Sound sets.** When a single `.psg` file's name ends in a chip number 1 to 3 after `_`, `-`, `.` or a space (`song_1.psg`, or `song.1.psg` as Vortex Tracker exports them), and files with the other numbers lie next to it with as many frames (give or take one, where the recording stopped mid-frame) and the same PSG header, µZX asks whether to open the whole set or only that file. This applies however a single file comes in: opening, dropping, Finder / Explorer, and **Import PSG** in both apps.

### Saving

The Player keeps your changes to the open edit, such as tempo, AY chip settings and mutes, and opens it again on the next launch. **File → Save** (`Cmd+S`) writes them to the `.uzx` file. A PSG file you opened has no `.uzx` file yet, so Save asks where to put one, as **File → Save As** (`Cmd+Shift+S`) always does. **File → Reveal in Finder** (**Show in Explorer** on Windows, **Show in Folder** on Linux) shows the saved file; it stays off until the edit has one.

### Transport Bar

Located at the top of the window.

<!-- guide-figure:player-overview -->
![Player transport and timeline](images/user-guide/player-overview.png)

The transport sits above the timeline; each track ends in its AY chip and level meter.
<!-- /guide-figure:player-overview -->

| Control | Description |
|---------|-------------|
| Rewind (⏮) | Jump to the beginning |
| Play/Pause (▶/⏸) | Start or pause playback |
| Position readout (Pos) | Current time in the ruler's timecode format: Bars.Beats.Frames (`1.2.05`), Minutes:Seconds:Frames (`01:05:12`), or Frames (`3262`). Click and type a position to seek there. You can leave parts off: in Bars.Beats.Frames `5` is bar 5; in Minutes:Seconds:Frames `5:20` is 5 seconds 20 frames and `1.5` is 1.5 seconds |
| BPM control | Tempo in BPM (inc/dec buttons or direct entry). Importing a PSG into an empty project sets it from the detected tracker speed, together with the frames-per-beat grid; dumps without a clear row grid keep the current tempo. A tracker module needs no detection of its rows: the tempo follows the speed most of its rows play at, or the repeating pattern of speeds when the module alternates them for a swing, and its first row starts bar 1. How many rows make a beat, and the meter, are still detected. Files imported together, such as the chips of a Turbo Sound tune, are detected as one tune. If a dump starts a few frames before its first tracker row, the clip is trimmed so that row starts bar 1, or, when those frames are not silent, moved right so it starts bar 2; the clips of a tune move alike |
| Groove (GRV) | Frame counts of the grid groups within one beat, such as 6 6 6 6, or 7 6 7 6 for a swing. Click and type your own. Importing a PSG into an empty project sets it from the detected rows, grouped four to a beat (three or five where the beat divides that way) |
| Time signature (SIG) | Click and type a new one, such as 3/4 or 6/8; a bare number keeps the denominator. PSG clips keep playing at the same times. Importing a PSG into an empty project sets 3/4 when its drum and bass patterns clearly repeat every three beats, 4/4 when they clearly repeat every four, and otherwise leaves it as is |
| Frame rate (FPS) | Frames a second of the project's timecode and grid. Click it to pick another rate: the PSG clips that played at the old one play at the new one (see [Opening Files](#opening-files)), and clips at their own rate keep it. A clip's own rate is under **Frame Rate** in its right-click menu, which changes all the clips of its tune; a clip at another rate than the project's shows a warning such as `⚠ 50 @ 200 fps` |
| Master volume knob | Output level; each app remembers it across files and launches |
| Level meter | Stereo output level, after the master volume |
| CPU meter | Left of the master volume knob. DSP: how much of each audio buffer's time the sound engine needs; near full, the sound can crackle. GUI: how busy the app's main thread is, which draws the window; near full, the timeline and scopes can stutter |

For keys, see [Keyboard and Mouse](#keyboard-and-mouse).

### Timeline

The timeline shows the loaded file as a clip on a track. Click anywhere on the ruler or timeline to seek to that position, or drag the playhead line to scrub.

Clips in the Player have no header: the notes take the whole track. A clip's warnings, such as an fps or chip clock that differs from the project's, sit over the top-left corner of its notes. The legend of the colours (channels A, B and C, the envelope E and noise N) appears once, at the top right of the tracks. The tuning table and key are in the corner above the AY chips; for a Turbo Sound tune they are detected from all its chips together, since each chip plays only part of the music, so the tune gets one key.

Scroll with the scrollbar under the tracks, the wheel, or by dragging the ruler; zoom with the wheel, the ruler, or `=` and `-` (see [Keyboard and Mouse](#keyboard-and-mouse)). During playback the view follows the playhead, except while you drag the ruler, the scrollbar, the playhead, or the tracks with the middle button, and for a second after you scroll with the wheel.

**View → Zoom**, or **Zoom** in the ruler or PSG clip context menu, offers **Frames**, **Groove Rows**, **Beats** and **Bars**, plus fit, previous zoom and zoom in/out. The four levels give one frame, an average groove row, a beat or a bar 16 pixels, using the tempo and meter at the playhead. Without a subdivision pattern, Groove Rows uses a quarter beat. The scale is the same in musical, seconds and frames timecode formats; the visible grid and snap spacing can differ.

**Previous Zoom** restores the previous horizontal position and scale without moving playback or undoing edits. Each zoom command or drag is one step; a continuous wheel or pinch gesture, or a held `=` or `-`, is one step. Scrolling and automatic playback following do not add steps. During playback, returning to a previous zoom pauses following for one second. History holds the last 64 views for the session and is not saved with the project.

### Viz Mode

**View → Viz Mode** (`F`) fills the screen with the tune alone: each track's notes, and at the right of each row its chip's A, B and C scopes. The notes sit on the scopes' dark background, so each row reads as one screen; drag the gap between the notes and the scopes to make the scopes wider or narrower. The transport, ruler, scrollbar and chip controls are hidden, and so are tracks without clips. `Space` still plays and pauses, and the view follows the playhead. `F` again or `Esc` leaves Viz mode. It stays on for the next file you open, until you leave it or quit.

Each chip has a row of its own, in track order, and two keys arrange the rows:

- `V` switches what each row shows: **Notes and Scopes**, then **Notes** alone across the whole row, then **Scopes** alone across the whole row.
- `S` switches how a chip's A, B and C scopes are tiled: **Stacked**, A over B over C, in a column at the right of the notes; or **Side by Side**, A | B | C, in a band under the notes, whose gap above you drag to make the band taller or shorter. With the scopes alone, Stacked lists every channel down the screen and Side by Side makes a grid, a chip per row. `S` does nothing while the notes are shown alone.

For a moment after you enter Viz mode or press either key, the top of the screen shows both choices, each with the current one lit, and the key to leave. **View → Viz Options** picks a layout and a tiling directly, and a colour scheme: **Slate**, as above, or **Classic Black and White**, black throughout with white scopes and thin grey lines between them; the notes keep their A, B and C colours. **Note Decorations**, the noise band, the envelope's stripes on its notes and the markers of an envelope too low to show as a note, are drawn **Auto**, as on the timeline, once a frame is 6 pixels wide, or **Always**, at any zoom. µZX remembers these options, and **File → Render Video...** draws them too.

### Rendering a Video

**File → Render Video...** renders the tune as a video, for YouTube and the like. Each frame shows what Viz mode shows, each track's notes and its chip's A, B and C scopes or what **View → Viz Options** chooses, and the view follows the playhead from the start of the tune to 3 seconds after its end. The render runs faster than real time, with a progress bar and **Cancel**; when it is done, µZX offers to show the file. The sound is the tune at full level: the master volume knob doesn't change it.

- **Show**, **Scopes**, **Colours** and **Decorations** are Viz mode's options, as in **View → Viz Options**: notes and scopes, notes or scopes; the scopes stacked or side by side; Slate or Classic Black and White; the notes' decorations Auto, once a frame is 6 pixels wide, or Always, which keeps them in a video zoomed out to beats or bars. Changing them here changes Viz mode too; in the Player, the view behind the dialog shows each choice.
- **Zoom** is the timeline zoom of the video: **As on screen**, the zoom of the view you see, or **Frames**, **Groove Rows**, **Beats** or **Bars**, the **View → Zoom** levels, which give one frame, row, beat or bar 16 pixels of the **Render** size, using the tempo and meter at the start of the tune.
- **ffmpeg** encodes the video. µZX looks for it on the `PATH` and in `/opt/homebrew/bin` and `/usr/local/bin`. If it isn't found, install it (`brew install ffmpeg`, or from [ffmpeg.org](https://ffmpeg.org/download.html)), or click **Choose...** to point to it.
- **Render** is the size the frames are drawn at, from 1280 × 720 to 3840 × 2160: the notes, scopes and labels are laid out as in a window that size. **Output** scales the frames up by a whole number, up to 8K, each drawn pixel becoming a sharp square block: 1280 × 720 at ×3 makes a 4K video of a 720p view.
- **Frame rate**: 60, 50, 48, 30, 25 or 24 fps. At 60 the notes scroll most smoothly on most screens.
- **Video**: H.264 plays everywhere; HEVC (H.265) and AV1 make smaller files, for 4K and 8K. **Quality** is High, Very high or Near lossless. A codec your ffmpeg can't encode is greyed out.
- **Audio**: AAC 384 kbps, Opus 256 kbps or ALAC (lossless), 48 kHz stereo.
- **Render...** asks where to save the `.mp4` file.

µZX remembers these settings, and the folder, for the next render.

### AY Chip and Level Meter

Each track ends on the right in its own AY chip and level meter, so a Turbo Sound tune shows one chip per track. Drag the gap to the left of the chips to make them wider, for wider scopes, or back down to their narrowest. Here you can adjust:

- **Chip clock frequency** — affects pitch of the whole output
- **Stereo mode** — mono, ABC, ACB, or custom channel panning. The Player remembers the layout and width last chosen and gives them to every PSG file it opens next; a saved `.uzx` file keeps its own
- Channel-level output settings: T, N and E turn a channel's tone, noise and envelope on or off. Each of A, B and C is on, muted (red) or soloed (emerald): a click mutes a channel that is on and turns a muted or soloed one back on; `Option`+click (`Alt`+click on Windows and Linux) solos it, a muted one too, or unsolos it. Several channels can be soloed, on any chips: while any is, only the soloed channels play. Soloing doesn't change the other buttons, and it isn't a step in Undo

The A, B and C scopes show each channel as the chip outputs it, before the stereo mix and the volume. Each scope centres its channel on half the level the channel's volume reaches, or half the full scale while the envelope drives it, so the waveform holds still as it plays; at Gain 1 a channel at volume 15 fills the scope's height, and a silent one is a line across the middle. When playback stops, the scopes keep the last frame that played. If a refresh overlaps a sample update, the scopes hold the previous frame until the next successful refresh. Their trigger is Chip Sync unless you pick another in the scope menu: each picture is centred on a rise of its channel's tone, or of its envelope when the tone is off, so noise and the envelope don't shake it; a channel with only noise runs free.

AY and standalone Scope displays start at 0.5× gain when the project has no saved gain.

The chip's title bar turns it on or off (`⏻`) and has a menu (`☰`) with scope settings; in Studio the menu also sets the chip's MIDI channels. Chips can't be removed or added in the Player, and its controls take no MIDI mapping.

A track is never shorter than its AY chip needs to show every control. When the tracks need more height than the window has, they scroll up and down.

<!-- guide-figure:player-ay-panel -->
![Player track devices](images/user-guide/player-ay-panel.png)

Chip type and clock, stereo controls, the three channel strips, and the track's level meter.
<!-- /guide-figure:player-ay-panel -->

---

## µZX Studio

A DAW-style editor for composing PSG chip music.

### Layout

<!-- guide-figure:studio-layout -->
![Studio layout with a selected PSG clip](images/user-guide/studio-layout.png)

Track headers are on the left, clips above, and the selected clip's parameter editor below.
<!-- /guide-figure:studio-layout -->

### Transport Bar

Same controls as µZX Player, plus:

| Control | Description |
|---------|-------------|
| Record (⏺) | Arm recording |
| Automation Read | Read automation from recorded data |
| Automation Write | Write automation from control movements |

### Importing PSG Files

**File → Import PSG** (`Cmd+I`) picks one or more `.psg` files or tracker modules (see [Opening Files](#opening-files)). One file goes on the selected track, or a new track when no track is selected; several files go on new tracks, one each, as do the other chips of a Turbo Sound module. Dropping `.psg` files or tracker modules onto the window adds each on a new track. Clips start at the beginning of the project. A file of a Turbo Sound set brings up the same question as in the Player (see [Opening Files](#opening-files)).

### Rendering a Video

**File → Render Video...** works as in the Player: see [Rendering a Video](#rendering-a-video). Every track with clips is drawn as Viz mode shows it, a chip's scopes at the right of its row; tracks without clips are left out.

### Timeline

The timeline contains **tracks** stacked vertically. Each track has a **header** on the left and a **body** on the right.

#### Track Header

| Control | Description |
|---------|-------------|
| Track name | Displays the track name; double-click to rename |
| R (Arm) | Arm track for recording |
| M (Mute) | Mute track output |
| S (Solo) | Solo this track (mutes all others) |
| I (Input) | Show/configure MIDI input |

#### Clips

Clips appear in the track body as colored blocks. PSG clips visualize chip register activity as miniature graphics inside each block.

Along the top of each clip runs its **header**, in the clip's colour. A selected clip is lighter, header and body, with a ring in a pale tint of its colour around it. It shows the clip's name, then warnings, such as a PSG clip whose fps or chip clock differs from the project's, then details such as the tuning table and key. A selected PSG clip's header also shows the legend of its colours at the right edge of the view: channels A, B and C, the envelope E and noise N. The Player has no clip headers (see [Timeline](#timeline)). When a header is too short for everything, the legend goes first, then the details, then the warnings, and the name is shortened last. In Studio, click the header to select the clip; a click on a header never moves the playhead.

See [Reading PSG clips](psg-visualization.md) for an illustrated reference to tone, envelope, noise, channel ownership, and matching pitches.

**Tuning table and key.** When a PSG file is imported, µZX detects which tuning table the tracker used (ProTracker and Vortex Tracker tables, including the earlier versions of ProTracker tables #0 and #2 that PT 3.3 and older play; when none fits, a tuning fitted to the tune: equal temperament, or a just intonation on a tonic when that fits clearly better) and the key of the tune. A tracker module names its table, so nothing is detected: when µZX has that table, it is set on the clip as if picked in **Tuning and key…**, and only the key is detected on it. µZX has the tables of Pro Tracker 2 and 3 (#0 to #3, with the PT 3.3 versions of #0 and #2), Vortex Tracker's #4 and Sound Tracker's own (`ST`, used by `.stc` and `.stp`, which Pro Tracker's #1 copies with two notes changed); for other trackers' tables, such as ASC Sound Master's, the table is detected as for a PSG file. Detection never picks `ST`: from the notes alone it can rarely be told from PT #1, so it names PT #1. The clip header shows both, for example `PT #1  E minor`; a clip with no audible notes shows neither. Register values only fix the ratio of chip clock to pitch, so a fitted tuning names the clock too when the ZX Spectrum 128 or Pentagon clock brings A4 within a few cents of 440 Hz, for example `12TET 440 Hz Pentagon` or `5LJI E 440 Hz Pentagon`; the clock also decides the note names. The Atari ST and Amstrad clocks are never guessed, since a ZX table two semitones low looks like 440 Hz on the Atari ST; choose them in **Tuning and key…**. Otherwise A4 is given at the ZX Spectrum clock, for example `12TET 436 Hz`. When the Player opens a PSG file, from **Open** or from Finder or Explorer, its AY chip plays at that clock, so a tune detected as `12TET 440 Hz Pentagon` plays at 1.75 MHz; a tune on a tracker table keeps the default clock. If the track's AY chip then runs at another clock than the fitted tuning's, the header shows a warning such as `⚠ Pentagon @ ZX Spectrum +23¢`: the tune was written for a Pentagon, the chip plays it at the ZX Spectrum clock, and every note sounds 23 cents higher than its name. Set the chip's clock in the AY panel, or the tuning's in **Tuning and key…**, to make them agree. In the Notes view, the table's notes become piano-roll lanes: a line between neighbouring notes, so each note sits inside its lane, in the colour of the timeline's finest grid lines, a brighter line below each C to mark the octaves, and the tonic's lane filled in the lines' colour. The timeline grid runs under the notes, and where its beat and bar lines cross the lanes the brighter line shows. Note names appear inside the lanes when the clip is tall enough; in a shorter clip only the tonic of each octave is named and only the octave lines are drawn, and in a very short clip the lanes disappear. Notes are drawn where the chip plays them and never snapped to a lane, so drift, bends and portamento stay visible. Turn the lanes off or on with **View → Note Lanes** or **Note Lanes** in the clip's right-click menu. **Piano Keys**, in the same two menus, adds a short piano keyboard over the lanes at the left edge of the view, one key per lane, so the groups of two and three black keys show where C is; the notes of the tune's key (C major when the clip has no key) are named on their keys, a black key's name at its left end and a white key's right of the black keys; a clip too short to name every lane leaves the keys unnamed. When the keys are too short to tell apart, the keyboard is hidden and only the lanes remain. Both choices apply to every clip and are remembered across files.

A `?` after the label means the detection was a close call. Right-click the clip and choose **Tuning and key…** to see the ranked tracker tables, then the fitted tunings (12TET, 5-limit and Pythagorean just intonations, each with its tonic and A4), and the keys, all with their scores, and the clip's pitch classes at sub-semitone resolution on the selected grid: a wrong grid shows as peaks between the lines, and the key's own pitch classes have brighter lines and labels than the others. For a fitted tuning, **Chip clock** chooses the machine the tune was written for and shows the A4 that gives: the score stays the same, but the notes are named for that clock. It is unavailable for tracker tables, which play the same register values on any clock. Picking a tuning, clock or key stores the choice with the clip; **Use detected** removes it. Files imported together, such as the chips of a Turbo Sound tune, are one tune: their tuning and key are detected from all of them, and a choice on one clip applies to all. Unless you picked a key yourself, the key is worked out again on the table you pick, since another table can name every note differently. The window follows the clip as you edit it and closes when the clip is deleted or another file is opened. ProTracker tables #0 and #3 differ only by a semitone, so a tune written for #0 is shown on #3 with every note named a semitone higher; pick the table to correct it.

Click a clip to select it. The Details Panel updates to show clip-specific editors.

#### Ruler

A time ruler runs along the top of the timeline. The display format switches between bars/beats/frames and mm:ss depending on settings.

#### Zoom and Scroll

- **Wheel** over the tracks — scroll tracks vertically when they don't fit
- Horizontal scroll and zoom work as in the Player — see [Keyboard and Mouse](#keyboard-and-mouse).
- Drag the resizable edge on the track header area to resize the header column.
- Drag the bottom edge of a track row to resize track height.

### Details Panel

The panel below the timeline is tabbed. Available tabs depend on what is selected.

#### PSG Parameter Editor

When a PSG clip is selected, the parameter editor appears. It shows an automation curve for a chosen PSG parameter over time.

- **Parameter list** — left sidebar lists available parameters (Tone Period A/B/C, Noise Period, Envelope Period, Volume, etc.). Click to switch.
- **Curve area** — shows the value of the parameter across the clip duration. Click to add points; drag points to edit.
- The curve aligns with the timeline grid.
- On the envelope's rows (**Envelope: Period**, **Envelope: Shape**, **Envelope: Retrigger**) a line down the row marks each restart of the envelope, including one with the same shape, where the curve stays flat.

<!-- guide-figure:studio-psg-editor -->
![PSG parameter editor](images/user-guide/studio-psg-editor.png)

Choose a register parameter in the list to inspect its values against the timeline grid.
<!-- /guide-figure:studio-psg-editor -->

#### Track Devices / Plugins

When a track is selected, the Devices panel shows the plugin chain on that track (e.g., the AY chip instrument plugin). Click **+** between device slots to insert a new plugin.

**MIDI to PSG** turns MIDI notes into notes for the first AY chip after it on the track, at that chip's clock: set the clock in the AY chip, never in MIDI to PSG. Its **Tuning preset** picks the notes. Equal temperament and the just intonations are computed for the chip's clock, so they stay in tune on any clock. A tracker table plays its own periods, made for one clock; when the chip runs at another, the panel shows how far every note sounds off, such as `⚠ Pentagon @ ZX Spectrum +23¢`. Set the chip's clock to the table's to play the table as written.

MIDI to PSG listens on four MIDI channels, counted from its **MIDI** channel. Notes on the first three play the chip's channels A, B and C: velocity and polyphonic aftertouch set a note's volume, the pitch wheel bends it up to two semitones, and controllers 80, 81 and 82 turn its tone, noise and envelope on (any value but 0) or off. A note on the fourth channel sets the pitch of the chip's envelope and lets it run on; controller 83 on that channel (any value but 0) restarts the envelope, with a note or inside one. Controller 70 on that channel sets the envelope shape, restarting the envelope when the shape is another, and controller 2 the noise period. MIDI on other channels passes through unchanged.

**Track → Render to audio** renders the selected track to a WAV. When the render succeeds, Studio adds and selects a new audio track and mutes the source. **Undo** removes the new track and restores the source's previous mute state. Choose **Cancel** in the progress window to discard the render and keep the source as it was.

<!-- guide-figure:studio-devices -->
![Studio track device chain](images/user-guide/studio-devices.png)

Selecting the track reveals its devices, including the AY chip emulator.
<!-- /guide-figure:studio-devices -->

---

## µZX Tuning

A standalone editor for generating and previewing AY chip tuning tables.

### Layout

The Tuning window is also available from **Settings → Tuning Tables** in Studio.

<!-- guide-figure:tuning-layout -->
![Tuning tables, controls, and note grid](images/user-guide/tuning-layout.png)

Choose a table on the left; pitch and preview controls sit above the note grid, with CSV export below.
<!-- /guide-figure:tuning-layout -->

### Controls

#### Tuning Table List

Lists all available built-in tuning tables (equal temperament, just intonation, historical scales, etc.). Click a row to load and preview that tuning.

#### Chip Clock

Sets the AY chip clock frequency in Hz. This affects how register period values map to musical pitches. Common values:

| Platform | Clock |
|----------|-------|
| ZX Spectrum (PAL) | 1,773,400 Hz |
| ZX Spectrum (NTSC) | 1,789,773 Hz |
| Amstrad CPC | 1,000,000 Hz |

Select a preset from the dropdown or type a custom value.

#### A4 Frequency

Sets the concert pitch reference for A4 in Hz (default 440 Hz). Drag the slider or double-click to type a value.

#### Reference Tuning

Selects the base tuning system (e.g., equal temperament, Pythagorean, various just intonation variants). This determines the interval ratios used when computing the tuning grid.

#### Key / Scale

- **Key** — root note (C, C#, D … B)
- **Scale** — scale type (chromatic, major, minor, pentatonic, various microtonal scales)

The tuning grid updates to show only the notes in the selected scale.

#### Play Controls

Allows auditioning notes directly through the AY chip emulator.

| Control | Description |
|---------|-------------|
| Play Chords | Play selected note as a chord |
| Play Tone | Enable tone generator for preview |
| Retrigger Tone | Retrigger note on each click |
| Play Envelope | Enable AY envelope generator |
| Envelope Shape | Select AY envelope shape (0–15) |
| Modulation Mode | Choose modulation behavior |

Click a cell in the tuning grid to hear the corresponding note. Starting another preview replaces the current one. Changing the tuning stops the preview and clears its note highlights.

#### Export Button

Click **Export to CSV** to save the current tuning table as a `.csv` file for use in a spreadsheet or further conversion.

### Tuning Grid

The main grid displays the computed AY period register values for each note across multiple octaves. Columns are notes, rows are octaves. The cell color indicates the tuning error relative to the ideal frequency (cents deviation).

Click a cell to preview the note through the AY emulator using the current play control settings.

---

## Keyboard and Mouse

`Cmd` is `Ctrl` and `Option` is `Alt` on Windows and Linux; on a Mac, `Ctrl`+click works as a right-click. Keys are ignored while a text field is being edited.

### Keyboard

| Key | Action | Apps |
|-----|--------|------|
| `Space` | Play / Pause | Player, Studio |
| `Home` or `Cmd+Left` | Rewind to start | Player, Studio |
| `End` or `Cmd+Right` | Jump to end | Player, Studio |
| `Cmd+L` | Loop the whole project on / off | Player, Studio |
| `=` or `+` | Zoom in around the playhead | Player, Studio |
| `-` | Zoom out around the playhead | Player, Studio |
| `Cmd+1` | Zoom to Frames (16 px per frame) | Player, Studio |
| `Cmd+2` | Zoom to Groove Rows (16 px per average row; a quarter beat without subdivisions) | Player, Studio |
| `Cmd+3` | Zoom to Beats (16 px per beat) | Player, Studio |
| `Cmd+4` | Zoom to Bars (16 px per bar) | Player, Studio |
| `X` | Previous zoom: restore horizontal position and scale | Player, Studio |
| `Shift+Z` | Zoom to fit the project | Player, Studio |
| `Z` | Zoom to selection | Studio |
| `Cmd+O` | Open file | Player, Studio |
| `Cmd+I` | Import PSG files or tracker modules — Studio adds one to the selected track, several on new tracks; Player asks whether to add them on new tracks or open them in a new edit | Player, Studio |
| `Cmd+N` | New edit | Studio |
| `Cmd+S` | Save | Player, Studio |
| `Cmd+Shift+S` | Save As | Player, Studio |
| `Cmd+Z` / `Cmd+Shift+Z` | Undo / Redo, such as AY chip settings or a clip's tuning; `Ctrl+Y` also redoes on Windows and Linux | Player, Studio |
| `Delete` or `Backspace` | Delete the selected clip, track, or device | Studio |
| `Cmd+T` | Add track | Studio |
| `Cmd+Shift+T` | Open Tuning Tables | Studio |
| `Cmd+,` | Audio/MIDI settings; on macOS, **Settings...** in the app menu, next to **About** | Player, Studio |
| `F` | Viz mode on / off: full screen, only the notes and each chip's scopes | Player |
| `V` | In Viz mode, show notes and scopes, then notes, then scopes | Player |
| `S` | In Viz mode, stack each chip's scopes or set them side by side | Player |
| `Esc` | Close the Settings, Plugins, or About window; in the Player, leave Viz mode | Player, Studio |
| `Cmd+Q` | Quit | Player, Studio |

### Mouse and trackpad (timeline)

| Gesture | Where | Action |
|---------|-------|--------|
| Click | Ruler or tracks, outside clip headers | Move the playhead (snaps to the grid when **View → Snap Playhead to Grid** is on) |
| Click | Clip header (Studio) | Select the clip, leaving the playhead where it is |
| Drag the playhead line | Ruler or tracks | Scrub; hold past either edge to scroll |
| Wheel or swipe | Ruler or scrollbar | Scroll horizontally |
| Wheel or swipe | Tracks (Player) | Scroll horizontally; when the tracks don't fit the window, the wheel scrolls them vertically and a sideways swipe scrolls the timeline |
| Sideways swipe | Tracks | Scroll horizontally |
| Drag | Ruler | Sideways scrolls, up/down zooms (down zooms out) |
| Middle-drag | Tracks | Same as dragging the ruler |
| `Shift+Wheel` | Ruler or tracks | Scroll horizontally |
| Drag | Scrollbar under the tracks | Scroll horizontally |
| Drag | Gap left of the AY chips, or of the stacked scopes in Viz mode (Player) | Make them wider or narrower |
| Drag | Gap above the side-by-side scopes in Viz mode (Player) | Make them taller or shorter |
| `Cmd+Wheel` | Ruler, tracks or scrollbar | Zoom around the mouse pointer, or around the playhead while it is followed |
| Pinch | Ruler, tracks or scrollbar | Same as `Cmd+Wheel` |
| Wheel | Tracks (Studio) | Scroll tracks vertically |
| Right-click | Ruler | Zoom, timecode format, and snap options |
| `Option`+click | A, B or C button of an AY chip | Solo the channel, a muted one too, or unsolo it (emerald when soloed); while any channel is soloed, on any chip, only soloed ones play. A plain click turns a soloed or muted channel back on |
| Right-click | Knob, slider, or switch (Studio) | MIDI mapping: learn a controller, pick a CC, or clear it |
| Right-click | PSG clip | Notes / Registers view, note lanes, piano keys, hex values on hover, the tune's frame rate, tuning table and key picker |
| Click | FPS in the transport bar | Pick the project's frame rate; the PSG clips at the old one follow |
| Double-click | Track name (Studio) | Rename |
| Drop `.psg` files | Anywhere in the window | Player: open them together, each on its own track (or drop a `.uzx` project to open it); Studio: add each on a new track |

---

## File Formats

| Extension | Description |
|-----------|-------------|
| `.psg` | Raw PSG register dump — standard format for AY chip music |
| `.pt3`, `.pt2`, `.pt1`, `.vt2`, `.txt`, `.stc`, `.st1`, `.st3`, `.stp`, `.asc`, `.as0`, `.sqt`, `.psc`, `.psm`, `.gtr`, `.ftc` | ZX Spectrum tracker modules, opened and imported as PSG register dumps with their title, note table and rows (see [Opening Files](#opening-files)) |
| `.uzx` | µZX project format — timeline edit with multiple tracks and metadata |

---

## See Also

- [ROADMAP.md](ROADMAP.md) — planned features and release milestones
- [Tuning systems](design/tuning.md) — background on tuning theory used in µZX Tuning
- [Vision](motool/vision.md) — long-term project vision
