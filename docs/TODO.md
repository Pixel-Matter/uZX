# µZX Development TODO

Granular tasks and implementation notes for µZX Studio and the shared engine. Player priorities are in [ROADMAP.md](ROADMAP.md); finished work goes to [CHANGELOG.md](../CHANGELOG.md).

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
- [ ] Start horizontal scroll if playhead is being dragged outside of the visible bounds
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

## PSG Clip / PSG data

- [x] Send initial PSG state on play start (noise-on bug when not in PSG file)
- [ ] PSG export
- [ ] Route every tempo change through `PsgTiming::setTempoBpmRetimingFrames` — today only `EditViewState::setBeatLength` / `setBpmSnappedToFps` rescale `framesPerBeat`; tempo edits from any other path (Tracktion tempo editing, future tempo track/automation) skip preserve-timing compensation
- [ ] `PsgList::loadFrom` should not write edit-wide project fps (`Helpers::setProjectFps`) — move that decision to the import caller (clip/controller)
- [ ] `PsgList::loadFrom` derives `framesPerBeat` from the beat length at 0, while migration uses the clip start — use the clip start in both

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
