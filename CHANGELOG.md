# Changelog

All notable changes to µZX are documented here. µZX currently supports AY-3-8910/YM2149 platforms (ZX Spectrum, Amstrad CPC, MSX, Atari ST, etc.).

## [Unreleased]

### Audio settings
- Player: the audio settings show only the output, without the audio inputs and MIDI ports it never uses
- Without a working audio device at startup, a message says what is wrong and offers **Open Audio Settings**, where the settings opened on their own without a word

### Opening files
- Fixed: Studio, the app for `.uzx` files, opened on its last project when a project was opened from Finder or passed on the command line
- Player: with no tune open, the window says how to open one, where it showed an empty grid and the colour legend; a file dragged over the window outlines it
- Player: **File → Import PSG** (`Cmd+I`) is gone; **Open** does the same
- Player: each tune keeps its stereo, mutes and other changes. Opening it again brings them back, and they survive a restart, where the copies lived in the temp folder and opening a tune again reset it. Open Recent lists the tunes
- Fixed: several files selected in Finder and opened in the Player, such as the files of a Turbo Sound set, did nothing; so did several paths on the command line. They open together, and a file that can't be found or opened is named in a message
- Fixed: the Open dialog listed `.ay` files and a drop accepted them, though µZX can't play them: an AYEmul file holds Z80 code. They are no longer offered
- Fixed: Windows: a file opened while the Player ran started a whole second Player, which loaded and saved the last project before handing the file over. It hands the file over at once
- Fixed: Linux: a file opened from the file manager while the Player ran did nothing. It opens in a new Player window
- Fixed: opening a `.uzx` project rewrote it on disk, even when it was only played. It is written when it has changes: on Save, when another file is opened, or on quit

### Timeline
- Dragging the ruler, or the tracks with the middle button, keeps to the direction it starts in: sideways only scrolls, up or down only zooms, and a diagonal start does both
- The loop range's ends show as lines down the tracks: in the loop's colour while looping, grey while not
- Double-click the loop strip to loop what is in view; zoomed to fit the project, that loops all of it again
- A loop range set anew with the mouse turns looping on
- **Clear Loop** in the ruler's right-click menu puts the loop range back over the whole project and turns looping off
- Fixed: a loop edge under the playhead can be dragged: on the loop strip a press is the loop's, and the playhead line is grabbed on the upper half of the ruler and on the tracks
- Fixed: the loop range on the ruler starts and ends on its grid lines' pixels, where it stopped a pixel short of its end and blurred its start
- Fixed: a drag on the loop strip moved the loop range instead of setting a new one while the range was the whole project, as in a new project, or ran out of view

### Notes view
- A note's opacity follows its volume as the chip's DAC does, halving every three steps, so notes that sound far apart look far apart; the noise band follows it too
- Fixed: channels a few cents apart that share a lane, such as a detuned bass doubled on two channels, keep their order at every zoom, the higher one on top
- Fixed: the envelope's stripes keep one slope along a note, also through a slide or while other channels change; they no longer steepen and flatten from frame to frame
- Fixed: a channel's noise is dotted around the note of a timer effect that plays it, such as a sync-buzzer, instead of in the noise band
- Zoomed out until a frame is narrower than a pixel, a short note is drawn a whole pixel wide in its own colour, where it faded into the background
- Painting is faster where noise sounds: a noise held over several frames is drawn as one band, where each frame drew its own

### Levels
- Level meters show how close the sound is to clipping: −54 to +6 dBFS, green up to −6 dB, yellow up to 0 dB, red only above 0 dB, where the output distorts; a peak falls back smoothly instead of flickering
- The transport's meter keeps its last LED red once the output has clipped; click it to clear
- A chip's **Volume** knob reads in dB, 0 dB where it starts and +6 dB at the end, and turns like the master volume knob; saved projects sound as before
- Double-click a knob or slider to return it to its default
- Player: a track's level meter shows the level alone, over its whole height, without the MIDI activity LEDs
- The chips of a Turbo Sound tune start lower, −6 dB each for two and −9.5 dB for three, so the tune no longer clips
- Render Video: **Normalize audio**, on by default, brings the loudest peak to −1 dB
- Fixed: the sound of a tune of several chips clipped in a rendered video
- Fixed: with two displays, a knob's value bubble could be drawn past the edge of the screen

### Transport and timeline
- Scrubbing: press on the playhead line and the chips sound the frame under it and hold it while the button is down; drag, and the sound follows the playhead from frame to frame. The scopes show the held frame
- Player: `Left` / `Right` step the playhead a grid line, `Option+Left` / `Option+Right` one frame; stopped, each step sounds the frame it lands on while the key is down
- Grabbing the playhead during playback pauses it
- Resting the pointer on a transport readout (bpm, fps, fpb, groove, sig, pos) says what it is and how to change it
- Fixed: tooltips never showed in the Player's and Studio's windows, such as the Follow button's and the level meter's

### Menus
- **Help → Check for Updates...** (on macOS, in the app menu) tells whether a newer release of the app is out on GitHub, and links to its page, or to all releases when yours is the latest; the Player looks for Player releases and Studio for Studio's
- Player: the Transport menu has **End**, **Step Back** / **Step Forward** and **Frame Back** / **Frame Forward**, each with its key, which no menu showed before
- Menu items are in title case, and those that open a window end in "...": **Play/Pause**, **Zoom to Project**, **Zoom In**, **Render to Audio**, **Audio Settings...**, **Plugins...**, **Tuning Tables...**, and **Show Hex Values on Hover**, **Tuning and Key...** and **Tempo Inspector...** in the clip menu

### Tempo detection
- Importing a PSG finds the tracker's rows from more than loud note starts: a legato note, a soft attack and a drum change count too, each weighed by how often it starts a row in tracker modules. More tunes get a tempo, and the grid follows the tracker's own rows more often, such as speed 3 where it showed 6
- A PSG whose rows hold no one grid, with a row a frame longer now and then or a groove that varies, gets its tempo from the beat it repeats at; such tunes had none
- Such a PSG also gets the rows inside its beat as its groove, where stretches of the tune agree on them: `7 8 8 8` for rows of 4-4-4-4-4-4-4-3 frames, or a swing as wide as 4-7, which the row grid search does not take
- Such a PSG is also placed by its pulse when that is clear: a tune that starts a few frames before a bar line has those frames at the end of bar 1, where the bar lines used to start with the file and every beat line was that much early
- **Tempo inspector…** in a PSG clip's right-click menu shows what the tempo detection reads in the tune and why it answers as it does: where a row grid holds over the tune, the row evidence frame by frame, how it repeats, and the beats that were weighed. A beat picked there can be made the project's tempo and judged against a metronome, which clicks louder on bar lines; on the detected beat, the detected groove and meter go to the project too, which repairs a project saved with a wrong detection. The project's own beat lines are drawn through the evidence, and a **Project** fact says how far they are from the detected beats. Its **Phase map** folds the tune at that beat: the rows inside the beat are bright lines, their gaps the groove, and a slip is a line that steps
- Fixed: a tempo a third too fast or too slow where the beat is 7 rows, or where plain rows with a stray note between them were read as a groove (speed 4 as 4-2-2-4)
- Fixed: no tempo for a tune whose rows go round in a number of frames no multiple of its pulse reaches, such as rows of 4-4-4-4-4-4-4-3 frames (31 to the beat, where 8 × 4 is 32): the beat now follows where the tune repeats

### Tracker modules and TAYM
- A TAYM that loops back past its start sets the loop range from its loop to its end, as a tracker module does
- A TAYM exported by Bitphase opens on its own tuning table (or its A4, for a custom 12TET table) and with the tempo and groove of the rows it mostly repeats, instead of detected ones
- Fixed: note slides in Pro Tracker 2 and 3, Vortex Tracker, ASC Sound Master and Fast Tracker modules play as in the trackers (zxtune updated)
- Fixed: a module whose groove starts after an intro, or varies now and then (3-5-3-3 with a 4-4-3-3), opened at the tempo of its commonest row length instead of its groove's
- A tune that starts before its first bar line, with a pickup or a few frames ahead of its first row, keeps its clip on the bar line: the clip begins with rests, where it used to be moved right to start in the middle of bar 1. Projects saved by this version can't be opened by older ones
- A tracker module with a pickup, or an intro of an odd length, is placed by its patterns: the bar lines fall where most of them start, so the first full pattern starts bar 2 instead of the first row starting bar 1
- A tracker module's patterns are bar lines: its beat and its meter fill them. A beat of 3, 6 or 7 rows no longer comes out of a module with 64-row patterns, and the time signature is set whenever only bars of 3 or only bars of 4 beats fill the patterns
- Fixed: a module whose rows follow no pattern, such as rows of 4-4-4-4-4-4-4-3 frames with a longer round now and then, opened at eight times its commonest row (32 frames to the beat) instead of what its rows usually make (31)
- Fixed: the Player played a TAYM at the clock its tuning detection guessed, not at the one the file states; a fitted tuning is now named at the file's clock, for example `12TET 440 Hz Atari ST`

## [v0.7.1-alpha, player-v0.7.1-alpha] - 2026-11-15

### Transport
- An odd number of frames per beat gets a default groove of 3, 5 or 7 equal groups, such as `5 5 5 5 5` at 25
- Fixed: the groove readout was empty when the beat has no groove, as at a prime fpb; it shows the beat as one group, such as `23`

### Projects
- Fixed: opening a project, from Open, Open Recent or Finder, or importing a tune no longer shrinks or moves the Player's or Studio's window

## [v0.7.0-alpha, player-v0.7.0-alpha] - 2026-11-15

### Download
- macOS: a disk image, ad-hoc signed: drag the app to Applications; the first launch takes **Open Anyway** in System Settings → Privacy & Security instead of a Terminal command
- The Player has its own releases, tagged `player-v…`; `v…` releases ship Studio

### Tracker modules and TAYM
- Open tracker modules like PSG files: Pro Tracker 1–3, Vortex Tracker II, Sound Tracker and Sound Tracker Pro, ASC Sound Master, SQ-Tracker, Pro Sound Creator, Pro Sound Maker, Global Tracker and Fast Tracker, played by zxtune; Turbo Sound modules open on a track per chip
- A module brings its title, note table, tempo, chip clock and frame rate; one that loops back after an intro sets the loop range
- Open `.taym` files, as Bitphase exports them: timer effects (pulse-width, sync-buzzer, samples) play in the AY chip, are drawn in the clip, and hold still on the scope
- Tunes at 100 or 200 Hz play at their rate: **FPS** in the transport bar, or **Frame Rate** in a clip's menu
- macOS: the Player is the default app for `.psg`, `.taym` and tracker modules; `.uzx` files open in it from Finder when no other app claims them
- Fixed: undoing an import removes only its tracks; Turbo Sound files that end mid-frame open as a set; a damaged PSG file can't fill the memory

### Transport and timeline
- **Loop range** on the ruler: drag to set it, Loop button or `Cmd+L` to loop it; playback starts at the playhead
- **Follow** button (`Cmd+Shift+F`), by Page or by Scroll; scrolling by hand pauses following until playback starts, stops or jumps
- Zoom levels: `Cmd/Ctrl+1…4` for Frames, Groove Rows, Beats or Bars; `X` returns to the previous position and zoom
- Larger transport buttons with drawn icons that look the same on every system, instead of colour emoji on Windows; quieter labels, taller meters
- Fixed: frame ticks are evenly spaced at 24, 25 and 48 fps; 200 fps labels keep their width; a click in a clip's body only moves the playhead

### Notes view
- The envelope is a filled, hatched note, in a channel's colour when that channel plays it alone; its restarts are marked
- Note decorations show from 3 pixels per frame
- The frame hover shows time, frame and bar, and each pitch's nearest note with the cents it is off
- Fixed: a note lasts through frames that change nothing; held notes have no seams between frames

### Video and Viz mode
- Render Video: **Zoom** and **Loops** choices
- Notes and scopes, notes or scopes alone (`V`); scopes stacked beside the notes or side by side under them (`S`)
- **Classic Black and White** colours and a **Note Decorations** choice
- Tracks without clips are left out; the progress window shows the stage, such as Encoding

### AY chip
- Fixed: changing the chip type or clock during playback, or unmuting a channel, keeps it sounding instead of silent until the tune sets it again
- Fixed: on Windows, an envelope that ends in silence (shapes 0–7, 9 and 15) no longer jumps back to full volume and stays there
- Fixed: a tone or envelope period that passes 0 while being written plays at its value
- Scopes and the AY chip use 0.5× display gain by default

### Player
- AY chips span the track's height and have no title bar: right-click a scope for its settings
- Fixed: clicking a control during playback no longer freezes the playhead and then makes it jump
- Fixed: opening the project that is already open keeps unsaved changes; `Delete` and `Backspace` no longer beep

### Studio
- MIDI to PSG plays chords on any channel, gives the chip's three channels to the notes with the highest priority, and reads MPE pitch bend and pressure; its chip controllers follow MIDI conventions, so knobs and buttons work on them
- μZX Instrument plays each note on its own MIDI channel, up to fifteen at once
- **Track → Render to PSG**; **Render to audio** can be cancelled, and one Undo removes a render
- Flat Clip Parameters and Track Devices tabs; the Edit menu no longer lists Cut, Copy and Paste
- Fixed: changing the tuning while notes play no longer crashes; detected tuning follows changes to clips and tempo; MIDI to PSG no longer resets a chip clock set by hand

### Projects and engine
- Projects saved by this version don't open in older ones, which say so instead of saving over them; older projects open and are converted, except MIDI clips that set the old chip controller values
- Tracktion Engine and JUCE 8.0.17, with hardened reading of XML, WAV, AIFF, PNG and ZIP files; audio device settings are converted on first launch, and older builds don't read them back

## [v0.6.0-alpha] - 2026-11-08

### Video and Viz mode
- **File → Render Video...** (Player and Studio) renders Viz mode to MP4 through ffmpeg: 720p to 4K, pixel-sharp upscaling to 8K, 24–60 fps, H.264, HEVC or AV1 with AAC, Opus or ALAC
- **View → Viz Mode** (`F`) in the Player: full screen with only the notes and scopes; `F` or `Esc` leaves it

### PSG import and detection
- Open several PSG files at once, each on its own track and AY chip; a file from a Turbo Sound set (`song_1.psg`, …) offers to open the whole set
- Tuning table and key detection, shown in the clip header and editable in **Tuning and key…**; knows PT 3.3's older tables #0 and #2
- Fitted tunings (12TET, just intonations) with a guessed chip clock (ZX Spectrum 128 or Pentagon); the Player plays at that clock and warns on a mismatch
- Tempo detection on import: BPM, frames per beat, groove and 3/4 or 4/4; lead-in frames are trimmed so the first row starts a bar
- Files imported together, such as a Turbo Sound set, are detected as one tune
- Fixed: pause markers no longer shift later frames; files with a `0xFF` version byte open

### Notes view
- Note lanes from the detected table (**View → Note Lanes**) and **View → Piano Keys** with the key's notes named on the keys
- Every clip has a header in its colour: name, warnings, tuning and key. The Player shows them over the notes instead
- Fixed: bar and beat lines no longer cross notes and keys; clips line up with the ruler to the pixel

### AY chip
- Solo channels with `Alt`-click (`Option`-click on a Mac) on A, B or C; several at once, saved with the file
- Scopes show the chip's raw channel levels, centred, triggered by **Chip Sync**, and keep the last frame when stopped
- Player: every track shows its own chip and level meter; drag the gap to their left to widen them
- Player: the stereo layout and width carry over to the next PSG file
- Fixed: Undo and Redo of chip settings change the sound again; showing a level meter is no longer an Undo step; Linux shows the level meter's LEDs

### Transport and timeline
- Master level meter, and DSP and GUI CPU meters
- Editable SIG; GRV (groove) replaces the subdivision readout
- The Pos readout shows and takes positions in the ruler's timecode format
- Timecode formats: Bars.Beats.Frames, `MM:SS:FF` and Frames; switching is no longer an Undo step. The Player uses bars | beats | frames too
- Master volume is an app setting, remembered across launches
- Fixed: clip FPS warnings update as soon as the project FPS changes

### Player
- Save and Save As (`Cmd+S`, `Cmd+Shift+S`), Reveal in Finder, and an Edit menu with Undo and Redo
- No MIDI learn or CC mapping
- Fixed: `Delete` and `Backspace` no longer remove a track with its chip

### Menus and shortcuts
- macOS: About and Settings... (`Cmd+,`) are in the app menu
- Windows and Linux: `Ctrl+Y` redoes; Reveal in Finder reads Show in Explorer or Show in Folder
- Fixed: zoom in works where `=` needs Shift, and `+` zooms in too; `Ctrl`+click opens the MIDI mapping menu on a Mac
- Fixed: Player and Studio reopen the last edit on launch

### Look
- Clip, channel and accent colours share one OKLCH lightness and chroma, so no hue outshines another
- A selected clip is lighter with a ring; clip bodies use the track's slate

### Smoothness
- The timeline moves once per display frame and holds 60 fps while playing, from cached clip painting
- Scopes update once per display frame
- macOS 14+: frames are drawn in step with the screen
- Fixed: a late frame no longer throws the view ahead and holds it

## [v0.5.0-alpha] - 2026-10-29

### Added
- PSG frame hover: rest the mouse on a PSG clip for 2 s to see the frame's full state as a table — period, Hz (from the AY clock), volume and tone/noise/envelope flags per channel, envelope shape, period and Hz, noise period. Values this frame changed are bright, the rest dimmed. Optional hex periods and volumes (clip context menu, saved globally)
- Clip view menu: Notes / Registers
- Beats+frames timeline: grid and ruler follow the timecode format, one zoom-level ladder, subdivision patterns (including swing such as 6-7-6-7) in the DIV control, FPB control
- Playhead snaps to the visible grid (toggle in the View and ruler menus)
- PSG timing is preserved across tempo changes (toggle in the ruler menu)
- PSG clip header: channel legend A B C E N, clip name, and a warning chip when the clip's fps differs from the project's
- Illustrated user guide (`docs/user-guide.md`)
- User guide lists every keyboard shortcut and timeline mouse gesture

- Horizontal scrollbar under the timeline
- Middle-drag over the tracks scrolls and zooms like dragging the ruler
- While you drag the ruler, the scrollbar, the playhead or the tracks during playback, the view stops following the playhead until release, and for a second after a wheel scroll
- During playback, Cmd+Wheel zooms around the playhead, so the view no longer jumps back to it
- Pinch to zoom on the ruler and tracks
- Dragging the playhead past either edge of the timeline scrolls the view
- A sideways trackpad swipe over the ruler or the tracks scrolls the timeline; in the Player, so does the plain wheel over the tracks
- Wheel, swipe and pinch over the scrollbar act as on the ruler; pinch also works over the playhead line
- Playback stops at the end of the project (unless recording or looping); Play from there starts over
- Loop in the Player's Transport menu

### Changed
- Registers view: centred bold uppercase hex in channel colours
- Noise is drawn only for channels where it is audible, with per-channel ownership marks; shared envelopes show which channels use them; envelopes too slow to have a pitch get a marker at the bottom
- Ruler labels no longer overlap
- Look and feel refresh
- New app icons for μZX Player and μZX Studio (Studio had none)
- Tracktion Engine updated to v3.2.0

### Fixed
- Studio showed no track headers for a project last saved by the Player
- Resizing the window or a track, and toggling snap or PSG-timing options, no longer add undo steps
- Scrolling and zooming, including the auto-scroll during playback, no longer fill the undo history
- Stale chip registers after a seek or play start landing just before a PSG frame (patched in our Tracktion Engine fork)
- Crash on quit while the Audio/MIDI Settings or Plugins window was open (a cause of the intermittent crash on exit)
- Envelope period 0 drew nothing; it now draws as period 1, the way the chip plays it
- Loop (`Cmd+L`) refused to play, asking for in/out markers that cannot be set; it now loops the whole project, following clip changes
- Crash when opening a damaged project, including on launch when the last project was damaged; an error is shown instead
- Memory corruption when loading PSG files that write the AY I/O port registers (R14/R15)
- Studio: possible crash when importing a file after switching projects while the file dialog was open
- Wrong chip state when playback starts or seeks mid-file: channels B, C, noise and envelope kept stale registers (patched in our Tracktion Engine fork)
- Paint-timing debug box no longer shows on PSG clips in release builds
- Opening a file that isn't a PSG (or has no music) shows an error instead of creating an empty track or project
- Playback stopping when MIDI devices are rescanned
- Playhead freezing when the audio input device differs from the output device; the audio settings dialog now prevents this setup
- Playhead running ahead of the sound by the audio device's output latency (large buffers, Bluetooth headphones); after Play or a seek it now waits at the new position until it is heard
- Playhead disappearing while dragged fast
- Wrong chip state at the start of a PSG file: the first frame now sends every register
- PSG import is a single undo step
- Ruler timecode format is saved with the project

## [v0.4.0-alpha] - 2026-03-04

### Added
- Embedded fonts
- Greetz updated
- First public pre-release of µZX Player

## [v0.3.1] - 2026-02-13

### Added
- Mouse gestures work over the playhead (transparent hit zone)
- Shift+wheel horizontal scrolling
- Universal binaries for macOS
- Timecode format switching
- New app icon
- Dirty state indication in About dialog version info

## [v0.3.0] - 2026-02-12

### Changed
- Separate target versioning per application
- Removed most "MoTool" naming from CMake files
- macOS deployment target set to 11.0

### Added
- Release plan and documentation for µZX Player

## [v0.2.0] - 2026-02-11

### Added
- **µZX Player**: lightweight playback-only variant of µZX Studio
  - File drag-and-drop and system file opening
  - Auto-fit track heights
  - Transparent clips with disabled selection
- PSG clip visualization overhaul:
  - Noise dots (random jittered pattern) and envelope stripes
  - Dynamic vertical scaling: auto-fit Y range to visible notes and envelope periods
  - Accumulated states in PSG painting
  - Note pitch scale display
  - Channel color legend
  - Envelope and noise modulation rendering
- AY plugin enhancements:
  - Channel and effect toggle buttons (Tone/Noise/Envelope per channel)
  - Configurable output channel count (stereo, separate channels)
  - Oscilloscope displays per channel
  - Chip clock presets combo box
  - Output mode configuration (stereo/mono/separate)
- Standalone ScopePlugin with sidechain support
- Tuning tool integrated into the main app as a separate window
- MIDI monitor/logger plugin
- Collapsible device UI panels
- Parameter binding system overhaul:
  - ParameterEndpoint abstraction
  - WidgetAutoParamBinding and SliderStaticParamBinding
  - BindedAutoParameter for automatable parameters
  - Comprehensive tests
- About dialog with version info, build metadata, and Git commit hash
- CMake tests integration with test discovery
- Audio import functionality
- Address Sanitizer support

### Fixed
- PSG importing critical bug: no retrigger info on consecutive writes of the same envelope type
- Snap-to-state parameter sync bug
- Crash when changing chip layout
- Audio device management clamps buffer size to avoid crash
- ChoiceButton bindings restored for parameter live reader
- Ayumi emulator no longer mis-initialises output on session load
- Crash when starting with WH headphones

### Changed
- Ayumi audio mixing refactored: separate channel mixing from stereo mixing
- Build metadata collected at build time with version.h generation
- MIDI to PSG Plugin: tuning preset selection, chip clock sync with Ayumi

## [v0.1.4] - 2025-10-09

### Added
- About dialog box
- Build metadata and version.h generation at build time
- CMake tests integration with test discovery
- BindedAutoParameter for automatable parameters with discrete choices
- AddressSanitizer support with minimum sample-rate guard

### Fixed
- Snap-to-state parameter sync bug
- Crash when changing chip layout
- Audio device management clamps buffer size to avoid crash
- ChoiceButton bindings restored for parameter live reader
- Ayumi emulator no longer mis-initialises output on session load
- Crash when starting with WH headphones

## [v0.1.2] - 2025-08-12

### Added
- MIDI to PSG Plugin: convert MIDI notes to PSG parameters using TuningSystem
- Complete parameter binding refactoring with ParameterAttachment
- Tuning table CSV export
- Reference tuning system (12-TET, 5-Limit Just Intonation)
- Just Intonation (5-limit) tuning system
- Scale degrees visualization with accidentals
- Scales grouping
- Tuning System: scales, keys, predefined tuning tables
- Tuning Preview MVP: grid showing tuning notes with tooltips
- Tuning notes prehear via MIDI clips (chords, scales)
- Envelope play mode with shape controls
- Mark notes with periods divisible by 16
- ChipInstrument MIDI plugin with playRate, aftertouch, pitch bend
- µZX Tuning as a separate standalone app target

### Fixed
- MidiToPsg MPEInstrument handling for pressure and pitchBend
- Various voices bugs
- Tuning grid bug when note replays instead of new note
- MIDI timing: convert all times in midiFX to samples

## [v0.1.1] - 2025-04-22

### Added
- Automation recording and playback
- MIDI mapping to parameters
- Edit file operations reworked; recent files menu; `.uZX` file extension
- Main window restores size and position
- Plugin enable/disable buttons
- Pitch ADSR sliders with skew factor and discreteness
- Device panel items with frame and in-place plugin UI editors
- Track level meter
- Device/plugin bottom panel (replaces track footers)
- Grid on PSG param editor, ruler, and tracks background
- ZoomViewState refactored to use viewTimePerPixel
- BPM editing with frame rate respect
- Track renaming and click-to-select
- Master volume
- Vertical tracks scrolling
- Track height resize and constrainer
- Resizable track header width
- PSG parameters playing via plugin
- PSG parameters representation and conversion to/from register values
- PSG import performance improvements
- AYChip plugin UI
- Auto-add AYChip plugin on PSG clip insertion
- PSG insertion creates new track
- Render to new audio track
- Frame rate in edit options
- Custom TimecodeFormat with getFPS
- Frame ticks at high zoom
- Better PSG data display: numbers in channels at high zoom
- Playhead following
- Zoom keyboard shortcuts
- PSG param curve viewing
- Smooth playback scrolling with stationary playhead
- AYChip plugin (Ayumi-based PSG playback)

### Fixed
- Edit component no longer loses edited clip when selection changes
- Labels justified to the left after resizing
- Bug in playhead: not redrawing when moved without zoom change
