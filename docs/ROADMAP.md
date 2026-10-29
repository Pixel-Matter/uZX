# µZX Roadmap

Open work only, highest priority first. Finished work goes to [CHANGELOG.md](../CHANGELOG.md).

## µZX Player

Current release: `v0.4.9-alpha` (unsigned builds for macOS, Windows and Linux).

### Next up

1. Stop at project end
2. Crash audit, then tag the next alpha

### Alpha for testers

**Goal:** a build you can hand to a few people without explaining workarounds.

#### P0 — Blockers

- [ ] **Stop at project end** if not in record mode
- [ ] **Crash audit** — open, drop, seek, switch files, quit; the intermittent crash on exit is only partly fixed
- [ ] **Shortcut docs match the app** — the user guide lists 5 shortcuts and a scrollbar that doesn't exist; add zoom (`=` `-` `Z`), Cmd+Wheel, Cmd+L, Cmd+I, Cmd+,

#### P1 — Important

- [ ] **Zoom around the playhead while playing** — Cmd+Wheel zooms around the mouse; keyboard zoom already centres on the playhead
- [ ] **Horizontal scrollbar** — disabled in `EditComponent`
- [ ] **Envelope period 0** — draws neither a note nor the low-envelope marker
- [ ] **Global master volume** — save in app settings and apply to every opened file (stored per edit now)
- [ ] **Last opened edit** — reopening works in code; reproduce the "not always" report or close it
- [ ] **Save / Save As** in the Player (Cmd+S does nothing)
- [ ] **Middle-mouse drag to pan** the timeline
- [ ] **Loop mode** for project scope (wole project span is loop region on default)
- [ ] **Shortcut consistency** across macOS, Windows and Linux
- [ ] **Turbo-sound** load 2-3 PSGs at the same time
- [ ] **Level meter** in transport bar to the left or right of global volume, stereo

#### P2 — Nice to have

- [ ] **Global default AY settings** (clock, stereo, channels) for PSG files opened from Finder or the command line
- [ ] **Solo** per AY channel (mute exists)
- [ ] **Loop** in the Player menu (Cmd+L already works)
- [ ] **BPM, FPB, DIV** detection
- [ ] **Note lanes** and tuning table detection

---

### v1.0 — First public release (macOS)

**Goal:** a signed macOS app anyone can download from GitHub and open without a Gatekeeper workaround.

#### P0 — Blockers

- [ ] **Code signing + notarization in CI** — release automation exists; the macOS build is an unsigned zip
- [ ] **DMG packaging** with an Applications symlink
- [ ] **Tracktion Engine** — move from v3.2.0 + fork patches to the latest stable, or decide to stay
- [ ] **Release notes** — CHANGELOG entry for the tag
- [ ] **Code quality review** and refactoring, UI separation, design tokens and constants, simple LaF-based design system

#### P1 — Important

- [ ] **About dialog** — GitHub link (only pixelmatter.org now), greets scroller
- [ ] **Turbosound** — open or drop 2–3 PSG files at once (Import PSG → New Track adds them one at a time)
- [ ] **Separate visualizer window** — oscilloscopes with horizontal and vertical layouts
- [ ] **Time signature editing** — BPM, FPS, FPB and DIV are editable; SIG is read-only
- [ ] **Loop range** on the ruler
- [ ] **Nicer transport icons**
- [ ] **UI themes** lignt, dark, neutral, customizable (ableton-like)

#### P2 — Nice to have

- [ ] **DMG background image**
- [ ] **Visualizer mode** — fullscreen oscilloscopes without controls (for streaming)
- [ ] **Channel split mode** — each AY channel in its own lane
- [ ] **Touchpad gestures** — pinch to zoom
- [ ] **Minimap** on the horizontal scroller

---

### After 1.0

Grouped by theme, not ordered.

**Platforms**

- [ ] Windows installer (NSIS or WiX) and code signing — CI ships a zip
- [ ] Linux AppImage — CI ships a tar.gz
- [ ] iOS and Android — JUCE supports mobile platforms; the Player is a natural fit

**Export**

- [ ] WAV export with a save dialog (Studio's internal Render to audio exists)
- [ ] MP3/OGG export
- [ ] PSG export and optimization
- [ ] Rendering to video

**Import**

- [ ] ChipNomad (CN)
- [ ] ProTracker 3 / Vortex Tracker II (PT3/VT2)
- [ ] AY and SNDH containers

**Views**

- [ ] Playlist mode — open several files, sequential play, queue
- [ ] PSG params clip view (Notes and Registers views exist)
- [ ] Live AY register monitor (the frame hover shows stored clip data, not chip state)
- [ ] Spectrum display
- [ ] Pianomania-like scrolling piano roll
- [ ] Tracker-like scrolling pattern view
- [ ] Skins/themes

**Analysis**

- [ ] Tuning analyzer — detect the tuning system of a PSG file
- [ ] Beat detection — estimate BPM and groove

**Quality**

- [ ] Smooth 60 FPS on modest hardware: PSG clip paint cache, async PSG loading with progress
- [ ] Crash reports and UX issues from 1.0 feedback

---

## µZX Studio

### Instrument & synthesis

- [ ] **LFO** for pitch, amplitude, noise, and shape in ChipInstrument
- [ ] **Portamento and legato** modes for ChipInstrument
- [ ] **Modifiers system** — ADSR, LFO, Random, MIDI CC, Keyfollow, Sidechain
- [ ] **Velocity/pressure sensitivity** module (offset, gain, curve)
- [ ] **Drum rack track** — map each MIDI note to a separate instrument
- [ ] **Chip mixing track** — downmix instrument tracks to PSG register values

### Editors

- [ ] **MIDI note editor** — piano roll note editing
- [ ] **MIDI CC editor**
- [ ] **Automation editor**
- [ ] **PSG parameter editor** — full editing of PSG parameters with multi-select and copy/paste

### UI modes

- [ ] **Vertical piano roll** UI mode
- [ ] **Classic tracker** UI mode

### Video & FX (MoTool demotool features)

- [ ] **Video/FX track** — video clips (mp4, avi, apng, animated GIF), images (png, jpg, ZX .scr), shapes, text on the timeline with transforms, alpha, rotation, and animation
- [ ] **Machine emulator track** — retro machine emulation as a timeline clip with state caching at keyframes; step, rewind, and fast-forward emulation; asm/binary code clips with memory patches and automated variables
- [ ] **Script FX clips** — ChaiScript/Python code snippets for custom effects with automatable parameters
- [ ] **Viewport** — video editor-like preview display with frame stepping and gizmo manipulation
- [ ] **Graphics conversion plugins** — convert rendered frames to machine-specific formats (ZX screen, etc.)

### Advanced features

- [ ] **Live mode** — scenes and clips to launch
- [ ] **Node system** — tracks, sequencer clips, launch clips, and internal plugin host; nodes linked 1:1 with timeline clips for params, data buffers, and image streams

See [docs/TODO.md](TODO.md) for granular implementation tasks.

### Full MoTool vision

µZX is the music subsystem of the larger MoTool project — a demotool for retrocomputer demoscene production. The full vision includes video/FX composing tracks, machine emulation tracks, and a node system for linking music, graphics, and code. See [docs/Vision.md](Vision.md) for details.
