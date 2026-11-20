# μZX Development TODO

Granular open tasks for μZX Studio and the shared engine. Player priorities are in [ROADMAP.md](ROADMAP.md). A finished task is deleted here; what it changed for users goes to [CHANGELOG.md](../CHANGELOG.md).

## Refactoring

No open findings. [refactoring.md](refactoring.md) holds the review's scope and its deferred notes; new findings are listed here by their number there.

## Design tokens

The Player draws with `Colors`, `Fonts` and `Space` from `src/gui/common/LookAndFeel.h`: colours by role, a type scale of point heights, gaps named by what they separate. Studio-only views and the dialogs still set their own numbers.

- [ ] Spacing in Studio's views to `Space`: Tempo Inspector, the tuning picker (`TuningPickerComponent`, about 15 layout numbers), the PSG parameter editor and details panel, the NotesToPsg, ChipInstrument and Scope plugin UIs, the generic plugin adapters
- [ ] Spacing and type in the dialogs and windows: Render Video, Audio Settings, Tuning (`TuningPreview`, `TuningPreviewGrid`), About, Update, Progress
- [ ] Labels and text editors that set no font get JUCE's height 15 (em 12.4, between `body` and `menu`): give them `Fonts::body` through `MoLookAndFeel`
- [ ] Fonts sized by layout instead of the scale: `LabeledSlider` and `ComboBox` labels (`withPointHeight(labelHeight)`), note-lane names (`jmin(11, noteHeight - 2)`, or 9), the progress bar's text (`height * 0.6`); snap them with `Fonts::fit` or give each a step
- [ ] Studio's track header: the name row's `8 + font height` and the 20 px buttons, on the grid
- [ ] Control heights: rows and buttons are 16, 18, 20, 24, 28, 32 or 40, chosen per file; name the few the UI needs next to `Space`
- [ ] `gui/nodes`, not compiled, draws with JUCE's named colours and literal sizes: move it to the tokens when it comes back

## Arranger mode

- [ ] Double timecode
- [ ] Double ruler (second is below the timeline)
- [ ] Virtual MIDI keyboard (see audio plugin host)
- [ ] Track level knob/slider
- [ ] Track devices intermediate level meters
- [ ] Zoom slider
- [ ] Moving clips around
- [ ] Moving tracks around
- [ ] BPM automation
- [ ] Time sig automation

## Global

- [ ] Icons (fontaudio, Font Awesome, octicons, nerd-fonts)
- [ ] Undoing a plugin deletion loses its MIDI CC mappings: the deletion removes them from `ParameterControlMappings` in memory, the restored plugin is a new object that is not mapped again, and the next save drops them from the project
- [ ] Crash audit leftover, not exercised live: quit while a render progress dialog is up (Studio)
- [ ] Device UIs register themselves from static objects (`REGISTER_PLUGIN_UI_ADAPTER`), which the linker drops from the `motool_common` static library unless each platform links all of it (`-force_load`, `/WHOLEARCHIVE`, `--whole-archive`; Linux lacked it until the level meter showed as a generic plugin). Replace the three flags with one of:
  - an OBJECT library: every object is always linked, no platform flags
  - an explicit `registerDeviceUIs()` called at startup: nothing depends on the linker, at the price of one central list

## ChipInstrument plugin

- [ ] UI
  - [ ] MIDI learn to parameters
  - [ ] Labels for parameter groups
  - [ ] PlayRate (tempo sync, 1/4, 1/8., etc, frame sync, 25, 50, 100, 150, 200, 400Hz)
  - [ ] Shape params (tone, noise, env shapes)
  - [ ] Envelope visualization/editing (ADSR, Steps, LFO)
  - [ ] ampVelocity
  - [ ] Snap to envelope period (tune2env)
  - [ ] LFO for pitch, amp, noise, shape
- [ ] Tests on ChipInstrumentVoice and everything else
- [ ] Velocity and pressure sensitivity module (offset, gain, curve with plot)
- [ ] Render MIDI to another track
- [ ] Modifiers: ADSR, LFO, Random, MIDI CC, Keyfollow, Sidechain
- [ ] Keyfollow modifier for pitchBendRange (chord starts out of tune, then goes to tune)

## MIDI to PSG Plugin

- [ ] UI for MIDI to PSG plugin with tuning editor button
- [ ] Sync tuning scale with Edit scales and keys

## PSG Clip / PSG data

- [ ] PSG export of PSG clips and whole projects (Track → Render to PSG covers a track with MIDI to PSG)
- [ ] Route every tempo change through `PsgTiming::setTempoBpmRetimingFrames` — today only `PsgTiming::setFramesPerBeat` / `setBpmSnappedToFrames` (the transport bar's FPB and BPM boxes) rescale `framesPerBeat`; tempo edits from any other path (Tracktion tempo editing, future tempo track/automation) skip preserve-timing compensation

## AY Chip Emulator plugin

- [ ] TAYM export, and timer effects as an instrument voice; what exists is in [timer-fx.md](design/timer-fx.md)

## Tuning System

- [ ] Snap notes to envelope periods (round periods to nearest integer divisible by 16)
- [ ] Tuning system store in ValueTree with name and id
- [ ] Add score points for TuningSystem (scale notes and all notes)
- [ ] Integrate to Edit value tree, global and per-clip
- [ ] Show defined notes in the grid
- [ ] Show VT tracker notes range
- [ ] Auto-tune to note (adjust A4 or clock frequency)
- [ ] Custom note tables from Vortex Tracker modules: a `.vt2` can carry its own table (`CustomNoteTable=`, 96 periods, as Bitphase reads it; Vortex loads custom tables in its options). `TuningTable` holds any periods, but an edit stores only a `BuiltinTuningType` name, so a clip's table must be serialised with the edit first; then import sets it as the clip's table instead of detecting one.

## PSG Param Editor

- [ ] Do not capture keyboard in PSG param list component (list focus is off since 2cc7588; recheck)
- [ ] Actual editing of PSG parameters
- [ ] Multiple selection of param points
- [ ] Copy/paste param curves
