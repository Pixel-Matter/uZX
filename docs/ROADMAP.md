# µZX Roadmap

Open work only, highest priority first. Finished work goes to [CHANGELOG.md](../CHANGELOG.md).

## Goal

µZX is a modern tool for composing and playing back chiptune music, and the music subsystem of [MoTool](motool/vision.md).

**Done** means: I can write a whole track in µZX Studio, from MIDI to a PSG export, and play it on real hardware; and play live from a MIDI controller through the AY emulator or even a real AY on a Spectrum (MIDI → PC → AY MIDI → ZX → audio). That is a far goal, so work goes milestone by milestone; each milestone below states its own goal.

**Out of scope:** anything not on this roadmap. For the current milestone (v1.0) that includes other chips, the node system, tracker UI and mobile.

## µZX Player

Current release: `v0.6.0-alpha`, the alpha for testers (unsigned builds for macOS, Windows and Linux).

### Next up

1. **Alpha feedback** — crashes and UX issues testers report
2. **v1.0 P0** below

### v1.0 — First public release (macOS)

**Goal:** a macOS app anyone can download from GitHub and open with one documented first-launch step. No paid Apple Developer Program, so no Developer ID signing or notarization.

#### P0 — Blockers

- [ ] **Ad-hoc signing in CI** — sign the whole bundle with `codesign --sign -`, so the first launch is **Open Anyway** in System Settings → Privacy & Security rather than `xattr` in Terminal; README and the user guide show that step
- [ ] **DMG packaging** with an Applications symlink
- [ ] **Tracktion Engine** — move from v3.2.0 + fork patches to the latest stable, or decide to stay
- [ ] **Code quality review** and refactoring, UI separation, design tokens and constants, simple LaF-based design system
- [ ] **UX quality review** review and tweak all paddings and margins (Player, Studio, Tuning, dialogs), fonts, colours, controls, etc...
- [ ] **Edit file format version** — Player 1.0 promises to open what it saves, and Studio keeps changing the shared edit format. Write a format version integer to the edit file, separate from app versions (Tracktion's `appVersion` attribute holds the app version, not the format), and decide what an older app does with a newer file: refuse with a clear message, or open it and ignore unknown parts
- [ ] **Release notes** — CHANGELOG entry for the tag

#### P1 — Important

- [ ] **Loop range** on the ruler
- [ ] **Nicer transport icons**
- [ ] **Playhead stepping** — ←/→ by one frame or one grid step (only Home/End now)
- [ ] **TAYM** (needs Timer fx, see [design/timer-fx.md](design/timer-fx.md))

#### P2 — Nice to have

- [ ] **UI themes** lignt, dark, neutral, customizable (ableton-like)
- [ ] **DMG background image**
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
- [ ] **Render Video options** — File → Render Video renders Viz mode as on screen, in its layout (notes and scopes, notes, or scopes), scopes (stacked beside the notes, or side by side under them) and colours (Slate or Classic Black and White); next: what else to show (note lanes, piano keys, grid, timecode, scopes before the notes), a zoom picker, vertical 9:16 video with vertical scrolling, and rendering from the command line

**Import**

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

**Quality**

- [ ] Smooth 60 FPS on modest hardware: async PSG loading with progress (PSG clips already paint from a cache)
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

µZX is the music subsystem of the larger MoTool project — a demotool for retrocomputer demoscene production. The full vision includes video/FX composing tracks, machine emulation tracks, and a node system for linking music, graphics, and code. See [motool/vision.md](motool/vision.md) for details; it is the initial vision, and only the items listed under **Video & FX** above are tracked.
