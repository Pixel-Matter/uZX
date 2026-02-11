# µZX

µZX is a modern PC-based tool for retrocomputer chiptune music composing and playback, focused on ZX Spectrum and similar platforms. It features a timeline-based interface for arranging music and effects, with support for emulated AY-3-8910/YM2149 sound chips. It consist of three main components: Studio, Player and Tuner.

```text
        █▌█▌█▌  █▌█▌█▌  █▌  █▌  █▌█▌█▌  █▌
        █▌  █▌    █▌      █▌    █▌█▌    █▌
  █▌█▌█▌█▌█▌█▌  █▌█▌█▌  █▌  █▌  █▌█▌█▌  █▌█▌█▌█▌
        █▌
                        |   |
    ,---.---.  ,---.  --|---|--  ,---.   ,---
    |   |   |       |   |   |   |    |  |
        |   |   ,---|   |   |   |---'   |
            |  |    |   |   |   |       |
               `---'    |   |    `---'
                        `-- `--

         ,--------.   ,---.
         `---/ / / \ / / /
            / / / \ ' / /
    B8   88  / /   / / /    Copyright 2025–2026
    B8   88   /   / / . \     pixelmatter.org
    B8.  88. /---/ / / \ \
    B8"oo""8o --'---'   `-'
    BP
```

![Powered by Tracktion Engine](third_party/tracktion_engine/tutorials/images/tracktion_engine_powered.png)

Uses [ayumi library](https://github.com/true-grue/ayumi) by Peter Sovietov (true-grue) for highly precise emulation of AY-3-8910 and YM2149 sound chips.

### Timeline

- Timeline consist of tracks one under the other, tracks are composed of clips.
- Tracks can be sorted and moved around, added and removed
- Dragging a clip from track to empty space creates a new track of suitable type
- There are two time rulers: Bars/Beats/frames on top of the timeline and mm:ss:fr on bottom.
- Timeline clip tools and mouse zones:
  - Select clips
  - Cut clips
  - Time-stretch clips
  - Extend clips or move cut point between clips
- Some tracks can be hidden, for example, you can have visible only MIDI Instrument track, but between MIDI instrument and hearable audio there is:
  - Sliced MIDI stream track
  - Chip mixed MIDI stream track
  - Rendered audio track

### Track and clip types

#### Music composing mode tracks

This subsystem is like your favorite DAW.

- Markers track: for labeling sections of the timeline. Actually this track is on the top/bottom rulers.
  Top labels are linked to bars/beats and do not shift when tempo/time signature changed, bottom ones are linked to seconds.
- Tempo/Time signature track — can be displayed on special track inside the ruler aslo. Tempo is constrained by target
- Audio track: for prototyping with audio placeholders or for converting to target machine formats.
  - Clips:
    - AudiocClip. Can be time-stretched, looped. When extended, loops are repeated.
    - ContainerAudioClip. You can group AudiocClips together.
  - Display: Waveforms.
  - Plugins: EQ, compressor/limiter, normalizer, delay, reverb, downsamling with dithering.
  - Automation:
    - Volume, pan.
    - Plugins parameters.
  - Input for record/monitor/chaining: Audio
  - Output: Audio
- MIDI track: for prototyping with midi placeholders or for downmixing MIDI to target machine PSG data
  - Clips:
    - MidiClip. Multimple MidiClips can share data, so you can edit a bassline once and every instance of it reflects the changes. You can add/remove/edit individual notes.
    - ContainerMidiClip. You can group several midi clips together.
  - Display: MIDI notes on pianoroll.
  - Plugins:
    - MIDI plugins: for processing MIDI events: arps, midi-emulated ADSR, LFOs, echo,  etc.
    - Synth plugins: regular ones and Chip emulation.
    - After synth plugins you can apply audio plugins as usual.
  - Automation:
    - Volume, pan.
    - Midi: you can add any MIDI CC, PitchWheel, Aftertouch lane and automate it.
    - Plugin paramters.
  - Input for record/monitor/chaining: MIDI
  - Output: MIDI, Audio (if audio plugins applied)
- Chiptune grouping track: track for grouping all of that is below:
- Instrument track: MIDI track with an "instrument": every input note transformed with instrument settings into sliced and automated MIDI stream,
  so to say MIDI-emulate volume ADSRs, LFOs, legato, portamento, detune, etc.
  Can operate with parameters only meaningful to specific music chips, for example with AY envelope modulation or noise.
  Aware of target music chip and its parameters.
  - Input for record/monitor/chaining: MIDI. For example, hardware/virtual MIDI keyboard.
  - Output: MIDI. Intended output is Chip Mixing track.
- Drum rack track: Special instrument track that maps every midi note to its separate instrument.
  - Input for record/monitor/chaining: MIDI. For example, hardware MIDI pads controller.
  - Output: MIDI. Intended output is Chip Mixing track.
- Chip Mixing track: downmixes instrument tracks to chip register vaues (PSG).
  Aware of target music chip and its parameters.
  - Plugins: ChipMixer for specific music chip(s).
  - Input for record/chaining: MIDI, intended chain input is all instrument tracks.
  - Output: MIDI, CC stream for specific music chip. Intended output is PSG track.
- PSG track: music data for playback on a target machine. PSG track is a MIDI track with CC values mapped to music chip registers.
  - Clips: MIDI clips with CC values mapped to music chip registers.
  - Display: notes on pianoroll by chip channels, notes can slide continously, color coded channels, with displayed notes volume and all modulation. And you can edit this.
  - Plugins: specific music chip emulator. You can change tuning, clock rate, fps, etc.
  - Automation:
    - Master volume.
    - Register values.
  - Input for record/chaining: MIDI. Intended chain input is Chip Mixing track.
  - Output:
    - MIDI, CC stream for specific music chip
    - Audio, (emulated chip output)
    - Data, PSG binary data for native/emulated player
