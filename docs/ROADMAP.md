# µZX Roadmap

## µZX Player

### v0.9 -- Alpha (Internal Testing)

**Goal:** A build you can hand to a few people to try. Fix showstoppers.

#### P0 -- Blockers

- [ ] Some bug with pure env bass viz on B channel in SkyTrainFunk close to the start
- [ ] Playhead disappears while dragging fast enough
- [ ] **Fix** sometimes stopping after 2 seconds of playback
- [x] **Drag-and-drop** -- accept `.psg` and `.uzx` files dropped onto the window
- [x] **Open from Finder** -- handle `anotherInstanceStarted()` / command-line args
- [x] **File association** -- `.psg` and `.uzx` macOS file types
- [x] **Window title** -- show filename in title bar
- [x] **Seek by clicking** -- click on the timeline/ruler to seek
- [x] **App icon**
- [x] Timeline is seconds-based until ruler supports custom beats+frames divisions
- [x] **Embed fonts** as binary resource data
- [ ] **Crash audit** -- test common scenarios and fix any crashes

#### P1 -- Important

- [ ] **Add left padding for player**
- [ ] **Global master volume** saved in app settings, not per-edit
- [ ] Beat+frames-based timeline
- [ ] Different rendering in seconds-only and beat-based timeline
- [x] **PSG** file icons in Finder
- [x] **Keyboard shortcuts** -- Space, Cmd+O, Cmd+Q, Home
- [x] **Copyright year** -- 2025-2026
- [x] **Separate version numbering** per target
- [x] **macOS universal build** (x86_64 + arm64)
- [ ] **Cleanup debug artifacts**
- [ ] **Zooming** -- zoom around playhead position while playing
- [ ] **AY reset** at the start of playback
- [x] **Shift-wheel** horizontal scrolling
- [x] **Mouse gestures** work over the playhead (transparent hit zone)
- [ ] **Last opened edit** not always working

#### P2 -- Nice to have

- [ ] **Mute/solo buttons** -- per-channel mute in AY side panel

---

### v1.0 -- First Public Release (macOS)

**Goal:** A polished macOS app anyone can download from GitHub and use.

#### P0 -- Blockers

- [ ] **Update Tracktion** engine to latest stable
- [ ] **macOS code signing + notarization** -- Apple Developer ID signing and notarytool
- [ ] **DMG packaging** -- app with Applications symlink
- [ ] **GitHub Actions CI** -- build, sign, notarize, DMG, upload as release artifact
- [ ] **GitHub Release** -- tag `v1.0.0`, DMG attached, release notes
- [ ] **Seek snapping** -- snap to frame boundaries
- [ ] **BPM and timesig** editing

#### P1 -- Important

- [ ] **Release branch strategy** -- tagging `main` recommended
- [ ] **About dialog** -- working links to GitHub repo, scroller for greets
- [ ] **Error handling** -- graceful message for corrupt/invalid PSG files
- [ ] **Audio device fallback** -- handle "no audio device" cleanly
- [ ] **Horizontal scrollbar** for timeline
- [ ] **Turbosound** -- open/import 2-3 PSG files at once
- [ ] **Separate visualizer window** -- oscilloscope with horizontal and vertical layouts
- [ ] **Nicer transport icons**

#### P2 -- Nice to have

- [ ] **DMG background image**
- [ ] **Screenshot/GIF** for README
- [ ] **Visualizer mode** -- fullscreen oscilloscopes without controls (for streaming)
- [ ] **Channel split mode** -- each AY channel in a separate lane
- [ ] **Minimap** on horizontal scroller
- [ ] **Touchpad gestures** for scrolling and zooming

---

### v1.1 -- Windows + Quality of Life

- [ ] **Windows build** -- GitHub Actions, NSIS or WiX installer
- [ ] **Windows code signing**
- [ ] **WAV export** -- render PSG to WAV
- [ ] **Looping** -- toggle loop playback
- [ ] Address crash reports and UX issues from v1.0 feedback
- [ ] Performance profiling -- smooth 60FPS on modest hardware

---

### v1.2 -- Linux + Format Support

- [ ] **Linux build** -- GitHub Actions, AppImage packaging
- [ ] **CN import** -- ChipNomad format
- [ ] **PT3/VT2 import** -- ProTracker 3, Vortex Tracker II
- [ ] **AY file import** -- AY container format
- [ ] **MP3/OGG export**

---

### v2.0 -- Major Feature Release

- [ ] **Playlist mode** -- open multiple files, sequential play, queue management
- [ ] **Channel views** -- pseudonotes / PSG params / AY regs switching
- [ ] **Frequency visualization** -- spectrum analyzer-like display
- [ ] **Tuning analyzer** -- detect tuning system used in a PSG file
- [ ] **Beat detection** -- estimate BPM and groove patterns
- [ ] **Pianomania-like view** -- scrolling piano roll visualization
- [ ] **Tracker-like view** -- scrolling tracker pattern view
- [ ] Rendering to video
- [ ] PSG file optimization and export
- [ ] Skins/themes
- [ ] **iOS and Android** -- JUCE supports mobile platforms; µZX Player is a natural fit

---

## µZX Studio

### Instrument & synthesis

- [ ] **LFO** for pitch, amplitude, noise, and shape in ChipInstrument
- [ ] **Portamento and legato** modes for ChipInstrument
- [ ] **Modifiers system** -- ADSR, LFO, Random, MIDI CC, Keyfollow, Sidechain
- [ ] **Velocity/pressure sensitivity** module (offset, gain, curve)
- [ ] **Drum rack track** -- map each MIDI note to a separate instrument
- [ ] **Chip mixing track** -- downmix instrument tracks to PSG register values

### Editors

- [ ] **MIDI note editor** -- piano roll note editing
- [ ] **MIDI CC editor**
- [ ] **Automation editor**
- [ ] **PSG parameter editor** -- full editing of PSG parameters with multi-select and copy/paste

### UI modes

- [ ] **Vertical piano roll** UI mode
- [ ] **Classic tracker** UI mode

### Video & FX (MoTool demotool features)

- [ ] **Video/FX track** -- video clips (mp4, avi, apng, animated GIF), images (png, jpg, ZX .scr), shapes, text on the timeline with transforms, alpha, rotation, and animation
- [ ] **Machine emulator track** -- retro machine emulation as a timeline clip with state caching at keyframes; step, rewind, and fast-forward emulation; asm/binary code clips with memory patches and automated variables
- [ ] **Script FX clips** -- ChaiScript/Python code snippets for custom effects with automatable parameters
- [ ] **Viewport** -- video editor-like preview display with frame stepping and gizmo manipulation
- [ ] **Graphics conversion plugins** -- convert rendered frames to machine-specific formats (ZX screen, etc.)

### Advanced features

- [ ] **Live mode** -- scenes and clips to launch
- [ ] **Node system** -- tracks, sequencer clips, launch clips, and internal plugin host; nodes linked 1:1 with timeline clips for params, data buffers, and image streams

See [docs/TODO.md](TODO.md) for granular implementation tasks.

### Full MoTool vision

µZX is the music subsystem of the larger MoTool project -- a demotool for retrocomputer demoscene production. The full vision includes video/FX composing tracks, machine emulation tracks, and a node system for linking music, graphics, and code. See [docs/Vision.md](Vision.md) for details.
