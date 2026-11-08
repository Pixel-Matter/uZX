# Changelog

All notable changes to µZX are documented here. µZX currently supports AY-3-8910/YM2149 platforms (ZX Spectrum, Amstrad CPC, MSX, Atari ST, etc.).

## [Unreleased]

### Video and Viz mode
- Viz mode and Render Video show notes and scopes, notes alone, or scopes alone: `V` in Viz mode switches, **View → Viz Options** and the Render Video dialog choose
- **Classic Black and White** colours for Viz mode and video: black, white scopes, thin grey lines between them
- Entering Viz mode, or pressing `V`, names the layouts for a moment, with the keys to switch and to leave

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
