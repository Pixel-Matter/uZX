# µZX Development TODO

Granular tasks and implementation notes for µZX Studio and the shared engine. Player priorities are in [ROADMAP.md](ROADMAP.md); finished work goes to [CHANGELOG.md](../CHANGELOG.md).

## Refactoring

See the [review scope and implementation order](refactoring.md#implementation-order). Numbers below match the detailed findings, not execution order.

- [ ] 1. [Centralize edit replacement lifecycle](refactoring.md#1-centralize-edit-replacement-lifecycle) shared by Studio and Player, preserving app-specific policy and teardown order.
- [ ] 2. [Consolidate AY plugin setup during PSG import](refactoring.md#2-consolidate-ay-plugin-setup-during-psg-import), using the destination track's owning edit.
- [ ] 3. [Remove the parameter display's unsupported editing contract](refactoring.md#3-remove-the-parameter-displays-unsupported-editing-contract) after checking inherited selection, zoom, and input behavior.
- [ ] 4. [Move timecode menu construction into the GUI layer](refactoring.md#4-move-timecode-menu-construction-into-the-gui-layer), shared by ruler and main-menu callers.
- [ ] 5. [Separate persistence results and recent-file ownership](refactoring.md#5-separate-persistence-results-and-recent-file-ownership), including failed-save reporting and newest-first history limits.
- [ ] 6. [Establish audio-thread ownership of mutable runtime state](refactoring.md#6-establish-audio-thread-ownership-of-mutable-runtime-state), reviewing render locks and the existing mapper race together.
- [ ] 7. [Make clock coordination between plugins explicit](refactoring.md#7-make-clock-coordination-between-plugins-explicit), including chain order and persisted settings.
- [ ] 8. [Separate PSG metadata, clip timing, and project policy](refactoring.md#8-separate-psg-metadata-clip-timing-and-project-policy), extending the existing import-timing tasks.
- [ ] 9. [Extract tuning export formatting](refactoring.md#9-extract-tuning-export-formatting), preserving CSV output and filename behavior.
- [ ] 10. [Give PSG mutations one derived-state invalidation owner](refactoring.md#10-give-psg-mutations-one-derived-state-invalidation-owner), covering removal, property edits, and undo/redo.
- [ ] 11. [Include timing context in tuning cache dependencies](refactoring.md#11-include-timing-context-in-tuning-cache-dependencies), preserving explicit choices while refreshing automatic results.
- [ ] 12. [Make widget bindings own callback teardown](refactoring.md#12-make-widget-bindings-own-callback-teardown), including slider text conversion and widget reuse.
- [ ] 13. [Centralize persisted PSG parameter mapping](refactoring.md#13-centralize-persisted-psg-parameter-mapping), preserving property names, sparse masks, and legacy normalization.
- [ ] 14. [Give deferred preview requests cancellation semantics](refactoring.md#14-give-deferred-preview-requests-cancellation-semantics), rejecting starts superseded by Stop or a newer preview.
- [ ] 15. [Give track rendering a single commit and rollback boundary](refactoring.md#15-give-track-rendering-a-single-commit-and-rollback-boundary), preserving source state on failure and cancellation.
- [ ] 16. [Move scope menu policy out of generic device UI](refactoring.md#16-move-scope-menu-policy-out-of-generic-device-ui), retaining one helper for AY and Scope callers.
- [ ] 17. [Make MIDI sample-range semantics consistent](refactoring.md#17-make-midi-sample-range-semantics-consistent), checking nonzero starts and final render spans.
- [ ] 18. [Align command registration with supported app capabilities](refactoring.md#18-align-command-registration-with-supported-app-capabilities), including unhandled clipboard commands and Player save shortcuts.
- [ ] 19. [Make scope sample publication data-race-free](refactoring.md#19-make-scope-sample-publication-data-race-free), protecting sample slots as well as the write index.
- [ ] 20. [Let scope displays own their settings binding](refactoring.md#20-let-scope-displays-own-their-settings-binding), removing duplicated AY and Scope parent bindings.

## Arranger mode

- [x] Bug with playhead in uZX Studio (not in uZX Player) — root cause: failed audio device freezes audible time; now detected with an error alert + Audio Settings
- [x] Timecode switching (frames, seconds, bars/beats)
- [x] Ruler respects timecode format
- [x] Grid respect new timecode format with beats and frames
- [x] FPS editing of timecode or of a separate edit FPS setting
- [x] Ruler timecode is saved with the edit
- [x] Grid level 0 subdivisions are strange
- [x] Playhead snaps to grid
- [ ] Pos readout/editing in different timecode formats (always HH:MM:SS:CC now)
- [ ] Double timecode
- [ ] Double ruler (second is below the timeline)
- [x] Better BPM editing
- [ ] Clip names are visible on audio and MIDI clips (PSG clips show them)
- [ ] Virtual MIDI keyboard (see audio plugin host)
- [ ] Loop region and looped playback (Loop toggle exists, no range UI)
- [ ] Track level knob/slider
- [ ] Track devices intermediate level meters
- [ ] Zoom slider
- [ ] Moving clips around
- [ ] Moving tracks around
- [ ] BPM automation
- [ ] Time sig editing (SIG is read-only)
- [ ] Time sig automation

## Global

- [ ] Icons (fontaudio, Font Awesome, octicons, nerd-fonts)
- [ ] CPU usage ticker in the status bar
- [ ] Hide the MIDI Logger debug plugin from the plugin list in release builds
- [ ] Reopening the project that is already open (Open Recent item 0, drop of the current .uzx) loses unsaved changes: `setEdit` saves the old edit, then force-saves the stale freshly loaded copy over it; both edits share a project ID, so deleting the old temp folder also wipes the new one's
- [ ] A second Player instance fully initialises and saves the shared startup edit before quitting (`moreThanOneInstanceAllowed` is checked after the `MoToolApp` constructor)
- [ ] Crash audit leftover, not exercised live: quit while a render progress dialog is up (Studio)

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
- [ ] Out-of-window MIDI is passed through with its timestamp and then merged again with an offset, roughly doubling it (`NotesToPsgMapper.cpp:298-299, 346, 352`); the AY plugin now clamps, but the mapper is still wrong

## PSG Clip / PSG data

- [x] Send initial PSG state on play start (noise-on bug when not in PSG file)
- [ ] PSG export
- [ ] Route every tempo change through `PsgTiming::setTempoBpmRetimingFrames` — today only `PsgTiming::setFramesPerBeat` / `setBpmSnappedToFrames` (the transport bar's FPB and BPM boxes) rescale `framesPerBeat`; tempo edits from any other path (Tracktion tempo editing, future tempo track/automation) skip preserve-timing compensation
- [ ] `PsgList::loadFrom` should not write edit-wide project fps (`Helpers::setProjectFps`) — move that decision to the import caller (clip/controller)
- [ ] `PsgList::loadFrom` derives `framesPerBeat` from the beat length at 0, while migration uses the clip start — use the clip start in both
- [ ] PSG pause runs (`FE xx`, up to 1,020 empty frames per 2 bytes) can exhaust memory on a crafted file of a few MB; past 2³¹ frames `static_cast<int>(i)` in `PsgList.cpp:533` overflows — cap total frames

## AY Chip Emulator plugin

- [ ] Consider keeping chip emulator state for every keyframe

## Tuning System

- [ ] Snap notes to envelope periods (round periods to nearest integer divisible by 16)
- [ ] Tuning system store in ValueTree with name and id
- [ ] Add score points for TuningSystem (scale notes and all notes)
- [ ] Integrate to Edit value tree, global and per-clip
- [ ] Show defined notes in the grid
- [ ] Show VT tracker notes range
- [ ] Auto-tune to note (adjust A4 or clock frequency)

## PSG Param Editor

- [ ] Do not capture keyboard in PSG param list component (list focus is off since 2cc7588; recheck)
- [ ] Actual editing of PSG parameters
- [ ] Multiple selection of param points
- [ ] Copy/paste param curves
