# μZX Roadmap

Open work only, highest priority first. Finished work goes to [CHANGELOG.md](../CHANGELOG.md).

## Goal

μZX is a modern tool for composing and playing back chiptune music, and the music subsystem of [MoTool](motool/vision.md).

**Done** means: I can write a whole track in μZX Studio, from MIDI to a PSG export, and play it on real hardware; and play live from a MIDI controller through the AY emulator or even a real AY on a Spectrum (MIDI → PC → AY MIDI → ZX → audio). That is a far goal, so work goes milestone by milestone; each milestone below states its own goal.

**Out of scope:** anything not on this roadmap. For the current milestone (v1.0) that includes other chips, the node system, tracker UI and mobile.

## μZX Player

Current release: `player-v0.7.1-alpha`, the alpha for testers (ad-hoc signed builds for macOS, unsigned for Windows and Linux).

### Next up

1. **Alpha feedback** — crashes and UX issues testers report
2. **v1.0 P0** below

### v1.0 — First public release (macOS)

**Goal:** a macOS app anyone can download from GitHub and open with one documented first-launch step. No paid Apple Developer Program, so no Developer ID signing or notarization.

#### P0 — Blockers

- [ ] **Code quality review** and refactoring, UI separation; Studio's design tokens are in [TODO.md](TODO.md)
- [ ] **UX quality review** — every Player window, dialog, menu and popup against [ux.md](ux.md) and the design tokens; findings in [ux-review.md](ux-review.md)
- [ ] Good human proof-read **user guide** and readme, including new animated gif
- [ ] **Release notes** — CHANGELOG entry for the tag

#### P1 — Important

- [ ] Piano keys/octave lanes lables should be drawn in **its own track gutters**, not in clips.

#### P2 — Nice to have

- [ ] **DMG background image**, note in background how to pass the gatekeeper with "Open anyway"
- [ ] **PDF user guide**

---

### After 1.0

Grouped by theme, not ordered.

**Platforms**

- [ ] Windows installer (NSIS or WiX) and code signing — CI ships a zip. Signing through [SignPath Foundation](https://signpath.org), free for open-source projects: needs a published code signing policy, MFA for everyone, and a manual approval of each release
- [ ] Linux AppImage — CI ships a tar.gz
- [ ] iOS and Android — JUCE supports mobile platforms; the Player is a natural fit

**Sound**

- [ ] **Three-channel AY output** — A, B and C as three separate outputs, filtered
- [ ] **Quick soloing/muting** of ABC abd TNE channels, with keyboard (player only). Muted channels are grayed out.

**Export**

- [ ] WAV export with a save dialog (Studio's internal Render to audio exists)
- [ ] MP3/OGG export
- [ ] PSG export of a whole project, and optimization (Track → Render to PSG writes one track's MIDI to PSG output)
- [ ] **Render Video options** — File → Render Video renders Viz mode as on screen, in its layout (notes and scopes, notes, or scopes), scopes (stacked beside the notes, or side by side under them) and colours (Slate or Classic Black and White); next: what else to show (note lanes, piano keys, grid, timecode, scopes before the notes), a zoom picker, vertical 9:16 video with vertical scrolling, and rendering from the command line

**Import**

- [ ] **Rest tracker modules**, converted to PSG or TAYM on open: Bitphase, ChipNomad (CN)
- [ ] AY and SNDH containers

**Views**

- [ ] Player playlist mode — open several files, sequential play, queue
- [ ] PSG params clip view (Notes and Registers views exist)
- [ ] Live AY register monitor (the frame hover shows stored clip data, not chip state)
- [ ] Spectrum display
- [ ] Pianomania-like scrolling piano roll
- [ ] Tracker-like scrolling pattern view
- [ ] **Skins/themes**: lignt, dark, neutral, customizable (ableton-like)
- [ ] **Channel split mode** — each AY channel in its own lane
- [ ] **Minimap** on the horizontal scroller

**Quality**

- [ ] Smooth 60 FPS on modest hardware: async PSG loading with progress (PSG clips already paint from a cache)
- [ ] Crash reports and UX issues from 1.0 feedback
- [ ] Tracktion timecode (MMC, hosted plugins) follows the project FPS — it is always 24 now

---

## μZX Studio

### Instrument & synthesis

- [ ] **LFO** for pitch, amplitude, noise, and shape in ChipInstrument
- [ ] **Portamento and legato** modes for ChipInstrument
- [ ] **Modifiers system** — ADSR, LFO, Random, MIDI CC, Keyfollow, Sidechain
- [ ] **Velocity/pressure sensitivity** module (offset, gain, curve)
- [ ] **Drum rack track** — map each MIDI note to a separate instrument
- [ ] **Chip mixing track** — downmix instrument tracks to PSG register values

### Editors

- [ ] **Cut, Copy and Paste** of clips, in the Edit menu with `Cmd+X`, `Cmd+C` and `Cmd+V`
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

See [TODO.md](TODO.md) for granular implementation tasks.

### Full MoTool vision

μZX is the music subsystem of the larger MoTool project — a demotool for retrocomputer demoscene production. The full vision includes video/FX composing tracks, machine emulation tracks, and a node system for linking music, graphics, and code. See [motool/vision.md](motool/vision.md) for details; it is the initial vision, and only the items listed under **Video & FX** above are tracked.
