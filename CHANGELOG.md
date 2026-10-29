# Changelog

All notable changes to µZX are documented here. µZX currently supports AY-3-8910/YM2149 platforms (ZX Spectrum, Amstrad CPC, MSX, Atari ST, etc.).

## [Unreleased]

### Added
- PSG frame hover: rest the mouse on a PSG clip for 2 s to see the frame's full state as a table — period, Hz (from the AY clock), volume and tone/noise/envelope flags per channel, envelope shape, period and Hz, noise period. Values this frame changed are bright, the rest dimmed. Optional hex periods and volumes (clip context menu, saved globally)
- Clip view menu: Notes / Registers
- Beats+frames timeline: grid and ruler follow the timecode format, one zoom-level ladder, subdivision patterns (including swing such as 6-7-6-7) in the DIV control, FPB control
- Playhead snaps to the visible grid (toggle in the View and ruler menus)
- PSG timing is preserved across tempo changes (toggle in the ruler menu)
- PSG clip header: channel legend A B C E N, clip name, and a warning chip when the clip's fps differs from the project's
- Illustrated user guide (`docs/user-guide.md`)

- Playback stops at the end of the project (unless recording or looping); Play from there starts over

### Changed
- Registers view: centred bold uppercase hex in channel colours
- Noise is drawn only for channels where it is audible, with per-channel ownership marks; shared envelopes show which channels use them; envelopes too slow to have a pitch get a marker at the bottom
- Ruler labels no longer overlap
- Look and feel refresh
- Tracktion Engine updated to v3.2.0

### Fixed
- Crash on quit while the Audio/MIDI Settings or Plugins window was open (a cause of the intermittent crash on exit)
- Crash when opening a damaged project, including on launch when the last project was damaged; an error is shown instead
- Memory corruption when loading PSG files that write the AY I/O port registers (R14/R15)
- Studio: possible crash when importing a file after switching projects while the file dialog was open
- Wrong chip state when playback starts or seeks mid-file: channels B, C, noise and envelope kept stale registers (patched in our Tracktion Engine fork)
- Paint-timing debug box no longer shows on PSG clips in release builds
- Opening a file that isn't a PSG (or has no music) shows an error instead of creating an empty track or project
- Playback stopping when MIDI devices are rescanned
- Playhead freezing when the audio input device differs from the output device; the audio settings dialog now prevents this setup
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
