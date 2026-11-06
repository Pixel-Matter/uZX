# Changelog

All notable changes to µZX are documented here. µZX currently supports AY-3-8910/YM2149 platforms (ZX Spectrum, Amstrad CPC, MSX, Atari ST, etc.).

## [Unreleased]

### Added
- Solo AY channels: `Alt`-click (`Option`-click on a Mac) an A, B or C button to solo or unsolo that channel; its button turns emerald. While any channel is soloed, on any chip, only soloed channels play; the others keep their mute buttons as they were, and a soloed channel plays even if muted. A click on a soloed button unsolos it. A chip with a soloed channel solos its track. Solo is saved with the file and, like track solo, isn't a step in Undo
- The Player remembers the stereo layout and width last chosen and applies them to every PSG file it opens next, as it does the master volume; a saved `.uzx` keeps its own, and clock, chip type and mutes stay per tune
- Stereo level meter of the master output in the transport bar, right of the master volume knob
- Tuning table and key detection for imported PSG clips: the clip header shows both (for example `PT #1  E minor`, with `?` on a close call), the Notes view draws the table's notes as piano-roll lanes, and **Tuning and key…** in the clip menu shows the ranked candidates and lets you choose; View > Note Lanes turns the lanes off
- Tempo detection on PSG import into an empty project: BPM, frames per beat, swings and grooves (tracker speed tables such as 4-4-3-3 frame rows) and 3/4 or 4/4 when the bar repetition is clear. A dump that starts a few frames before its first row is trimmed, or moved right when those frames sound, so that row starts a bar
- The time signature (SIG) in the transport bar is editable: type `3/4`, `6/8` or a bare numerator; PSG clips keep playing at the same times
- View > Piano Keys (also in the PSG clip menu): a short piano keyboard over the note lanes at the view's left edge, so the groups of black keys show where C is
- Tuning detection fits just intonations (5-limit, 5-limit 45:64, Pythagorean) on any tonic alongside 12TET, and names the chip clock (ZX Spectrum 128 or Pentagon; the Atari ST and Amstrad only by hand) at which a fitted A4 is within 4 cents of 440 Hz, for example `12TET 440 Hz Pentagon`. Detection matches only tracker tables and otherwise fits; the 12TET and just-intonation presets are left out. The **Tuning and key…** window lists the fitted tunings after the tracker tables and has a **Chip clock** choice for them, which renames the notes. The Player plays a PSG file it opens at the clock of its fitted tuning, and the clip header warns when the track's AY clock differs from it, for example `⚠ Pentagon @ ZX Spectrum +23¢`
- Several PSG files at once: the Player opens them together, each on its own track and AY chip (Open, Import PSG, or drop them on the window), named after their shared name; Studio's Import PSG puts several on new tracks, and PSG files dropped on Studio are added on new tracks. A single file named like one chip of a Turbo Sound set (`song_1.psg`, `song.1.psg`, `song-1.psg`, `song 1.psg`) whose sibling chips have as many frames and the same header brings up a question whether to open the whole set
- Tuning detection knows the earlier ProTracker tables #0 and #2 that PT 3.3 and older play, so tunes written with them are named on their own table instead of a nearby one. The Tuning window measures each tracker table against the clock and A4 its authors documented, such as 1.625 MHz for ProTracker #0 and 1.52064 MHz for Vortex Tracker #4

- Save and Save As in the Player (`Cmd+S`, `Cmd+Shift+S`): Save writes the open `.uzx` file; for an opened PSG file, which has none yet, it asks where to save one. Reveal in Finder shows a saved file
- An Edit menu in the Player with Undo and Redo, for changes such as AY chip settings or a clip's tuning
- The Pos readout in the transport bar shows the position in the ruler's timecode format, such as `2.3.05`, `01:05:12` or `3262`, and takes a typed position in that format; parts can be left off, so `5` is bar 5

### Changed
- Timecode Format has three formats, named as they read: Bars.Beats.Frames, Minutes:Seconds:Frames (`MM:SS:FF`, which snaps to frames instead of milliseconds) and Frames (an absolute frame count instead of `m:ss:ff`). Bars | Beats without frames is gone; files saved with it open in Bars.Beats.Frames
- Switching the timecode format is no longer a step in Undo and doesn't count as an unsaved change; the file still keeps it
- The Player shows each track's AY chip and level meter at the right end of its row, instead of one side panel for the selected track, so a Turbo Sound tune shows every chip. The chip's title bar turns it on or off and has its menu; chips can't be removed or added in the Player. A track is never shorter than its AY chip needs; when the tracks don't fit the window, they scroll
- Unselected clip headers keep the clip's full colour instead of a dimmed one; a selected clip is lighter and has a ring in a pale tint of its colour. Clip bodies are the track's own slate, a little lighter, instead of a dark tint of the clip colour that turned muddy over the slate
- New PSG clips get track colours of equal lightness and colourfulness: the hue steps around OKLCH instead of HSV, so yellow and cyan clips no longer outshine blue and violet ones
- The AY channel colours A, B and C (orange, green, blue) and the envelope's purple share one lightness and colourfulness, so channel A no longer looks brightest and C darkest. The E swatch in the clip header shows its stripes in the full envelope colour, as envelope notes do, instead of faded
- Accent colours across the UI share the PSG channels' lightness and colourfulness: automation Write, Record, mute and arm are a softer red instead of pure `#FF0000`, and solo, warnings, highlights, the level meter, MIDI-mapped markers and notes two channels play at once follow the same level
- Every clip has a header in its colour, bright when the clip is selected, instead of a white outline: its name, then warnings and the tuning and key. The A B C E N legend sits at the right edge of the view, on the selected clip. Click the header to select the clip. The Player has no clip headers: warnings sit over the notes, the legend appears once at the top right of the tracks, the tuning and key once above the AY chips, detected from all the chips of a Turbo Sound tune together
- The Player's timeline shows bars | beats | frames like Studio's, instead of always switching to seconds; a `.uzx` file keeps the format it was saved with
- Master volume is an app setting instead of a per-file one: Player and Studio each remember it across launches and apply it to every opened file
- macOS: About and Settings... (`Cmd+,`) are in the app menu, File has no second Quit, and the Player has no Settings and Help menus
- Windows and Linux: `Ctrl+Y` also redoes, and Reveal in Finder reads Show in Explorer or Show in Folder
- The Player has no MIDI learn or CC mapping on its controls and no MIDI channel range in the chip menu; Chip debug to console is in Debug builds only
- The transport's subdivision readout is now GRV (groove), and import fills it with four groups per beat, so a 4-3-3-3 row groove reads 7 6 7 6

### Fixed
- Undo and Redo of AY chip settings (channel on/off, stereo layout and width, volume) change the sound and the buttons again, not only the saved file
- `Delete` and `Backspace` in the Player no longer remove the last clicked track with its AY chip
- Zoom in works on keyboard layouts where `=` needs Shift, such as German; `+` zooms in too
- `Ctrl`+click opens the MIDI mapping menu on a Mac, as right-click does
- Player and Studio reopen the last edit on launch again: once the recent list was full, a newly opened or imported edit dropped out of it, so the previous edit opened instead and File > Open Recent never listed the newest one
- PSG clip FPS warnings update as soon as the project FPS changes, instead of on the next redraw
- Showing a track's level meter, in Studio's track devices or the Player, no longer adds a step to Undo
- The timeline scrolls smoothly during playback: the playhead, ruler, PSG clips and parameter chart move together once per display frame, and the playhead keeps up with the sound instead of trailing it by a frame or two
- PSG clips line up with the ruler to the pixel, and note lane names no longer shift by a pixel while the view scrolls
- PSG files with pause markers no longer shift every later frame by one per pause, and files written with a `0xFF` version byte open

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
