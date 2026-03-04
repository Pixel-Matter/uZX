# µZX Development TODO

Granular tasks and implementation notes for active development.

## Arranger mode

- [ ] Timecode switching (frames, seconds, bars/beats)
- [ ] Grid respect new timecode format with beats and frames
- [ ] FPS editing of timecode or of a separate edit FPS setting
- [ ] Double timecode
- [ ] Ruler respects timecode format
- [ ] Double ruler (second is below the timeline)
- [ ] Better BPM editing
- [ ] Start horizontal scroll if playhead is being dragged outside of the visible bounds
- [ ] Clip names are visible
- [ ] Virtual MIDI keyboard (see audio plugin host)
- [ ] Loop region and looped playback
- [ ] Track level knob/slider
- [ ] Track devices intermediate level meters
- [ ] Zoom slider
- [ ] Moving clips around
- [ ] Moving tracks around
- [ ] BPM automation
- [ ] Time sig editing
- [ ] Time sig automation

## Global

- [ ] Icons (fontaudio, Font Awesome, octicons, nerd-fonts)
- [ ] CPU usage ticker in the status bar

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

- [ ] Send initial PSG state on play start (noise-on bug when not in PSG file)
- [ ] PSG export
- [ ] Send cumulative regs/params values on play start from new position
- [ ] Async or modal with progress bar PSG loading
- [ ] PSG clip paint image cache for performance

## AY Chip Emulator plugin

- [ ] Visual monitor of current AY registers (human readable PSG parameters)
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

- [ ] Do not capture keyboard in PSG param list component
- [ ] Actual editing of PSG parameters
- [ ] Multiple selection of param points
- [ ] Copy/paste param curves
