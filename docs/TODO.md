# µZX Development TODO

Granular open tasks for µZX Studio and the shared engine. Player priorities are in [ROADMAP.md](ROADMAP.md). A finished task is deleted here; what it changed for users goes to [CHANGELOG.md](../CHANGELOG.md).

## Refactoring

See the [review scope and implementation order](refactoring.md#implementation-order). Numbers are the findings' stable IDs, not execution order; finished findings are deleted.

- [ ] 1. [Centralize edit replacement lifecycle](refactoring.md#1-centralize-edit-replacement-lifecycle) shared by Studio and Player, preserving app-specific policy and teardown order.
- [ ] 2. [Consolidate AY plugin setup during PSG import](refactoring.md#2-consolidate-ay-plugin-setup-during-psg-import), using the destination track's owning edit.
- [ ] 3. [Remove the parameter display's unsupported editing contract](refactoring.md#3-remove-the-parameter-displays-unsupported-editing-contract) after checking inherited selection, zoom, and input behavior.
- [ ] 4. [Move timecode menu construction into the GUI layer](refactoring.md#4-move-timecode-menu-construction-into-the-gui-layer), shared by ruler and main-menu callers.
- [ ] 6. [Establish audio-thread ownership of mutable runtime state](refactoring.md#6-establish-audio-thread-ownership-of-mutable-runtime-state), reviewing render locks and the existing mapper race together.
- [ ] 7. [Make clock coordination between plugins explicit](refactoring.md#7-make-clock-coordination-between-plugins-explicit), including chain order and persisted settings.
- [ ] 8. [Separate PSG metadata, clip timing, and project policy](refactoring.md#8-separate-psg-metadata-clip-timing-and-project-policy), extending the existing import-timing tasks.
- [ ] 9. [Extract tuning export formatting](refactoring.md#9-extract-tuning-export-formatting), preserving CSV output and filename behavior.
- [ ] 10. [Give PSG mutations one derived-state invalidation owner](refactoring.md#10-give-psg-mutations-one-derived-state-invalidation-owner), covering removal, property edits, and undo/redo.
- [ ] 11. [Include timing context in tuning cache dependencies](refactoring.md#11-include-timing-context-in-tuning-cache-dependencies), preserving explicit choices while refreshing automatic results.
- [x] 12. [Make widget bindings own callback teardown](refactoring.md#12-make-widget-bindings-own-callback-teardown), including slider text conversion and widget reuse.
- [ ] 13. [Centralize persisted PSG parameter mapping](refactoring.md#13-centralize-persisted-psg-parameter-mapping), preserving property names, sparse masks, and legacy normalization.
- [ ] 17. [Make MIDI sample-range semantics consistent](refactoring.md#17-make-midi-sample-range-semantics-consistent), checking nonzero starts and final render spans.
- [ ] 18. [Align command registration with supported app capabilities](refactoring.md#18-align-command-registration-with-supported-app-capabilities).

## Arranger mode

- [ ] Double timecode
- [ ] Double ruler (second is below the timeline)
- [ ] Virtual MIDI keyboard (see audio plugin host)
- [ ] Loop region and looped playback (Loop toggle exists, no range UI)
- [ ] Track level knob/slider
- [ ] Track devices intermediate level meters
- [ ] Zoom slider
- [ ] Moving clips around
- [ ] Moving tracks around
- [ ] BPM automation
- [ ] Time sig automation

## Global

- [ ] Icons (fontaudio, Font Awesome, octicons, nerd-fonts)
- [ ] A second Player instance fully initialises and saves the shared startup edit before quitting (`moreThanOneInstanceAllowed` is checked after the `MoToolApp` constructor)
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
- [ ] MPE on MIDI output: send pitch bend on note channel via MPEChannelAssigner
- [ ] Tests on ChipInstrumentVoice and everything else
- [ ] Velocity and pressure sensitivity module (offset, gain, curve with plot)
- [ ] Render MIDI to another track
- [ ] Modifiers: ADSR, LFO, Random, MIDI CC, Keyfollow, Sidechain
- [ ] Keyfollow modifier for pitchBendRange (chord starts out of tune, then goes to tune)

## MIDI to PSG Plugin

- [ ] UI for MIDI to PSG plugin with tuning editor button
- [ ] Sync tuning scale with Edit scales and keys
- [ ] Thread safety (crash risk): `NotesToPsgMapper::setTuningSystem` / `reset` / `setBaseChannel` run on the message thread without a lock while the audio thread uses `tuning_`, `midiBuffer_` and `voices_[channel - baseChannel_]` — use-after-free of the old `TuningSystem`, concurrent `juce::Array` writes, out-of-range voice index. Easiest to hit in uZXTuning by changing root/A4/clock/table while a scale plays

## PSG Clip / PSG data

- [ ] PSG export
- [ ] Route every tempo change through `PsgTiming::setTempoBpmRetimingFrames` — today only `PsgTiming::setFramesPerBeat` / `setBpmSnappedToFrames` (the transport bar's FPB and BPM boxes) rescale `framesPerBeat`; tempo edits from any other path (Tracktion tempo editing, future tempo track/automation) skip preserve-timing compensation
- [ ] `PsgList::loadFrom` should not write edit-wide project fps (`Helpers::setProjectFps`) — move that decision to the import caller (clip/controller)
- [ ] `PsgList::loadFrom` derives `framesPerBeat` from the beat length at 0, while migration uses the clip start — use the clip start in both

## AY Chip Emulator plugin

- [ ] Timer fx support (for TAYM import/export); design in [timer-fx.md](design/timer-fx.md)

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
