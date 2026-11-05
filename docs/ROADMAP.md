# µZX Roadmap

Open work only, highest priority first. Finished work goes to [CHANGELOG.md](../CHANGELOG.md).

## Goal

µZX is a modern tool for composing and playing back chiptune music, and the music subsystem of [MoTool](Vision.md).

**Done** means: I can write a whole track in µZX Studio, from MIDI to a PSG export, and play it on real hardware; and play live from a MIDI controller through the AY emulator or even a real AY on a Spectrum (MIDI → PC → AY MIDI → ZX → audio). That is a far goal, so work goes milestone by milestone; each milestone below states its own goal.

**Out of scope:** anything not on this roadmap. For the current milestone (Player alpha for testers) that includes other chips, video, the node system, tracker UI and mobile.

## µZX Player

Current release: `v0.5.0-alpha` (unsigned builds for macOS, Windows and Linux).

### Next up

1. **Tab bars** — the remaining P1 below

### Alpha for testers

**Goal:** a build you can hand to a few people without explaining workarounds.

#### P0 — Blockers

None.

#### P1 — Important

- [ ] Fix **tab bars** look and feel to match design system (after plugins panes)

#### P2 — Nice to have

- [x] **BPM, FPB, GRV, SIG** detection — set on PSG import from the tracker row grid and bar-level repetition (3/4 or 4/4); dumps without a grid (speed 1, heavy volume ornaments) keep the current tempo
- [ ] **Remembered stereo** — the stereo layout and width last chosen in the Player apply to every PSG file opened next, as the master volume already does; a saved `.uzx` keeps its own. Clock and chip type stay per tune (detected on open), and mutes don't carry over
- [ ] **Solo** per AY channel (mute exists)

---

### v1.0 — First public release (macOS)

**Goal:** a macOS app anyone can download from GitHub and open with one documented first-launch step. No paid Apple Developer Program, so no Developer ID signing or notarization.

#### P0 — Blockers

- [ ] **Ad-hoc signing in CI** — sign the whole bundle with `codesign --sign -`, so the first launch is **Open Anyway** in System Settings → Privacy & Security rather than `xattr` in Terminal; README and the user guide show that step
- [ ] **DMG packaging** with an Applications symlink
- [ ] **Tracktion Engine** — move from v3.2.0 + fork patches to the latest stable, or decide to stay
- [ ] **Code quality review** and refactoring, UI separation, design tokens and constants, simple LaF-based design system
- [ ] **UX quality review** review and tweak all paddings and margins (Player, Studio, Tuning, dialogs), fonts, colours, controls, etc...
- [ ] **Release notes** — CHANGELOG entry for the tag

#### P1 — Important

- [ ] **About dialog** — GitHub link (only pixelmatter.org now), greets scroller
- [ ] **Separate visualizer window** — oscilloscopes with horizontal and vertical layouts
- [x] **Time signature editing** — SIG is editable in the transport bar; PSG clips keep their timing
- [ ] **Loop range** on the ruler
- [ ] **Playhead stepping** — ←/→ by one frame or one grid step (only Home/End now)
- [ ] **Nicer transport icons**
- [ ] **UI themes** lignt, dark, neutral, customizable (ableton-like)

#### P2 — Nice to have

- [ ] **DMG background image**
- [ ] **Visualizer mode** — fullscreen oscilloscopes without controls (for streaming)
- [ ] **Channel split mode** — each AY channel in its own lane
- [ ] **Minimap** on the horizontal scroller
- [ ] **Clock-mode frame grid** — frame ticks restart every second, so at 24 fps 5-frame ticks run …20, 24, 29; the frame field widens to three digits at 100/200 fps

---

### After 1.0

Grouped by theme, not ordered.

**Platforms**

- [ ] Windows installer (NSIS or WiX) and code signing — CI ships a zip. Signing through [SignPath Foundation](https://signpath.org), free for open-source projects: needs a published code signing policy, MFA for everyone, and a manual approval of each release
- [ ] Linux AppImage — CI ships a tar.gz
- [ ] iOS and Android — JUCE supports mobile platforms; the Player is a natural fit

**Sound**

- [ ] **Three-channel AY rendering** — ayumi renders channels A, B and C as three separate outputs, with options to turn off its filter and to remove DC

**Export**

- [ ] WAV export with a save dialog (Studio's internal Render to audio exists)
- [ ] MP3/OGG export
- [ ] PSG export and optimization
- [ ] **Rendering to video** — a view mode for recording or exporting a visualization clip, also from the command line: resolution, frame rate (common rates or custom), frame format (4:3, 16:10, 16:9), what to show (lanes, keys, ruler, oscilloscopes with their width and layout, note decorations, colours, background colour, the PSG base name), zoom (a set level or the current view)

**Import**

- [ ] TAYM (needs Timer fx, see [TODO.md](TODO.md))
- [ ] **Tracker modules**, converted to PSG or TAYM on open: ProTracker 3 / Vortex Tracker II (PT3/VT2), Bitphase, ChipNomad (CN); conversion by zxtune123, its sources added as a submodule
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
- [ ] Tracktion timecode (MMC, hosted plugins) follows the project FPS — it is always 24 now

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

- [ ] **Cut, Copy and Paste** of clips — the Edit menu lists them with `Cmd+X`, `Cmd+C` and `Cmd+V`, but they do nothing yet
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
